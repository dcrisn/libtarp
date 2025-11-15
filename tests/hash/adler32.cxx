#include <algorithm>
#include <tarp/hash/checksum/adler32.hxx>
#include <tarp/random.hxx>
#include <tarp/stopwatch.hxx>
#include <tarp/string_utils.hxx>
#include <tarp/timeutils.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <endian.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

using byte_vector = std::vector<std::uint8_t>;
namespace rd = tarp::random;
namespace tu = tarp::time_utils;
namespace str = tarp::utils::str;
using namespace tarp::hash::checksum;

namespace ref {
uint32_t adler32(const unsigned char *data, size_t len) {
    constexpr uint32_t MOD_ADLER = 65521;
    uint32_t a = 1, b = 0;
    size_t index;

    // Process each byte of the data in order
    for (index = 0; index < len; ++index) {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }

    return (b << 16) | a;
}
}  // namespace ref

// rolling checksum:
// - each time we roll the window, compare with a checksum computed
// from scratch: the checksums obtained should be exactly the same
// if valid.
// - generate a few million bytes and insert some patterns in them;
// then roll the window beginning to end and verofy checksum is always
// valid (as if calculated from scratch), and also that the patterns are found.

static tarp::hash::checksum::adler::adler32_ctx::checksum_t
process_in_blocks(const byte_vector &bytes, std::size_t blocksz) {
    // std::cerr << "Processing input of size " << bytes.size()
    //           << " in blocks of size=" << blocksz << std::endl;

    tarp::hash::checksum::adler::adler32_ctx ctx;

    const auto n = bytes.size() / blocksz;
    for (std::size_t i = 0; i < n; ++i) {
        // std::cerr << "-- Processing block at offset " << i * blocksz
        //           << ", len=" << blocksz << ", block="
        //           << tarp::utils::str::to_string(
        //                bytes.data() + i * blocksz, blocksz, "[", "]")
        //           << std::endl;
        tarp::hash::checksum::adler::update(
          ctx, bytes.data() + i * blocksz, blocksz);
    }

    // std::cerr << "-- Processing remaining block at offset " << n * blocksz
    //           << ", len=" << bytes.size() - n * blocksz << ", block="
    //           << tarp::utils::str::to_string(bytes.data() + n * blocksz,
    //                                          bytes.size() - n * blocksz,
    //                                          "[",
    //                                          "]")
    //           << std::endl;

    tarp::hash::checksum::adler::update(
      ctx, bytes.data() + n * blocksz, bytes.size() - n * blocksz);

    return tarp::hash::checksum::adler::get_checksum(ctx);
}

#if 1
TEST_CASE("test vector validation against reference") {
    using namespace tarp::hash::checksum;

    // basics: 0, 1, 0xff, odd and even lengths
    std::vector<byte_vector> inputs {
      {0},
      {1},
      {0, 1},
      {1, 1},
      {1, 1},
      {1, 0},
      {0, 0, 0, 0, 0, 0},
      {1, 1, 1, 1, 1, 1},
      {0xf, 0xf, 0xf, 0xf},
      {0xf, 0xa, 0xb, 0xc},
      {0xc, 0xc, 0xc, 0xc, 0xc},
      {0xc, 0xc, 0xc, 0xc, 0xd},
      {0xd, 0xc, 0xc, 0xc, 0xc},
      {0xc, 0xc, 0xc, 0xc, 0xc, 0xd},
      {0xd, 0xc, 0xc, 0xc, 0xc, 0xc},
    };

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        auto &buff = inputs[i];
        //std::cerr << "INPUT: " << str::to_string(buff) << std::endl;
        //std::cerr << "---- one-shot processing: \n";
        const auto cksum1 = adler32(buff.data(), buff.size());
        //std::cerr << "---- block processing: \n";
        const auto cksum2 = process_in_blocks(buff, 3);
        REQUIRE(cksum1 == cksum2);
        REQUIRE(adler32(buff.data(), buff.size()) ==
                ref::adler32(buff.data(), buff.size()));
    }

    std::cerr << " === buffsz >= 16 ====\n";
    // try buffer of size >= 16 to hit later code paths
    // beyond byte at a time for short buffers
    auto buff = rd::bytes(19);
    REQUIRE(adler32(buff.data(), buff.size()) ==
            ref::adler32(buff.data(), buff.size()));

    std::cerr << " === BIG BUFFER === \n";
    // try big buffer, to hit MAX_ADD code paths
    buff = rd::bytes(231'000);
    REQUIRE(buff.size() == 231'000);
    REQUIRE(adler32(buff.data(), buff.size()) ==
            ref::adler32(buff.data(), buff.size()));

    std::cerr << " === RANDOM FUZZ ===\n";
    constexpr std::size_t N = 7'000;
    constexpr std::size_t MAXSZ = 120'000;
    for (std::size_t i = 0; i < N; ++i) {
        buff = rd::bytes(rd::randsz(0, MAXSZ));
        std::cerr << "  | PASS " << i << ", buffsz=" << buff.size()
                  << std::endl;
        const auto cksum = adler32(buff.data(), buff.size());
        REQUIRE(cksum == ref::adler32(buff.data(), buff.size()));


        // go past the end of the buffer to test that possibility as well
        const auto blocksz = rd::randsz(1, buff.size() + 10);
        const auto cksum2 = process_in_blocks(buff, blocksz);
        REQUIRE(cksum2 == cksum);
    }
}

TEST_CASE("perf") {
    using namespace std::chrono;
    const auto buff = rd::bytes(15'000'000);
    tarp::StopWatch<tu::fractional_ms> sw;

    tu::fractional_ms elapsed1, elapsed2;

    sw.start();
    auto cksum1 = tarp::hash::checksum::adler32(buff.data(), buff.size());
    sw.stop();
    elapsed1 = sw.get_time();

    sw.start();
    auto cksum2 = ref::adler32(buff.data(), buff.size());
    sw.stop();
    elapsed2 = sw.get_time();

    std::cerr << "Libtarp speed=" << elapsed1.count()
              << "ms; ref speed=" << elapsed2.count() << "ms" << std::endl;

    REQUIRE(cksum1 == cksum2);
}
#endif

TEST_CASE("rolling checksum basics") {
    byte_vector buff = {0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x1, 0x2, 0x3, 0x4};

    constexpr std::size_t wsz = 3;
    adler::adler32_ctx ctx;
    adler::update(ctx, buff.data(), wsz);

    for (std::size_t i = wsz; i < buff.size(); ++i) {
        adler::roll(ctx, wsz, buff[i], buff[i - wsz]);
        auto rolled = adler::get_checksum(ctx);

        // compare to if we calculated the checksum clean from
        // scratch
        auto cksum = adler32(buff.data() + i + 1 - wsz, wsz);

        REQUIRE(rolled == cksum);
    }
}

TEST_CASE("rolling checksum fuzz and pattern finding") {
    constexpr std::size_t N = 100;
    for (std::size_t i = 0; i < N; ++i) {
        std::cerr << "# " << i << "/" << N << std::endl;
        const auto buffsz = rd::randsz(2, 73'000);
        // const auto buffsz = rd::randsz(2, 37);
        const auto wsz = rd::randsz(1, buffsz - 1);
        const auto s = rd::alpha_string(1, buffsz);
        auto buff = rd::bytes(buffsz);

        // ------ add some strings to the buffer to check
        // the rolling checksum finds them
        const auto strlen = wsz;
        const auto max_strings = buffsz / strlen;
        auto nstrings = rd::pick<std::size_t>(1, max_strings);
        const auto string = rd::alphanumeric_string(wsz);
        std::size_t strings_added = 0;
        std::size_t strings_found = 0;

        if (nstrings > 0) {
            // add one string right at the end to test edge case
            std::memcpy(buff.data() + buffsz - wsz, string.c_str(), wsz);
            strings_added = 1;
            --nstrings;

            // rest of the strings
            for (std::size_t x = 0, offset = 1;
                 x < nstrings && offset < buff.size();
                 ++x) {
                std::memcpy(buff.data() + offset, string.c_str(), wsz);
                offset += wsz;
                ++strings_added;
            }
        }
        // --------

        const auto strcksum =
          adler32(reinterpret_cast<const std::uint8_t *>(string.c_str()),
                  string.length());

        adler::adler32_ctx ctx;
        adler::update(ctx, buff.data(), wsz);

        std::cerr << "\n\n";
        // std::cerr << "TEST: buffsz=" << buffsz << " wsz=" << wsz
        //           << " buff=" << str::to_string(buff, "[", "]") << std::endl;

        // now roll window
        for (std::size_t r = wsz; r < buff.size(); ++r) {
            // end=5, window must contain 3,4,5
            const std::size_t l = r + 1 - wsz;
            const std::size_t out = l - 1;
            adler::roll(ctx, wsz, buff[r], buff[out]);

            // std::cerr << "-- Current window: [" << l << "," << r
            //           << "] : " << str::to_string(buff.data() + l, wsz)
            //           << std::endl;

            // check that the rolled checksum exactly matches
            // the checksum we get if we compute it clean, from
            // scratch
            auto cksum1 = adler::get_checksum(ctx);

            // std::cerr << "-- Computing clean: offset=" << l << ", len=" <<
            // wsz
            //           << std::endl;
            auto cksum2 = adler32(buff.data() + l, wsz);
            REQUIRE(cksum1 == cksum2);

            // --- check if we've found a string
            if (cksum1 == strcksum) {
                std::string mystring(buff.begin() + l, buff.begin() + l + wsz);
                if (mystring == string) {
                    std::cerr << "String found at offset "
                              << l /* << ": " << mystring */
                              << std::endl;
                    strings_found++;
                }
            }
            // --- check if we've found a string
        }

        REQUIRE(strings_added == strings_found);
    }
}

int main(int argc, const char **argv) {
    doctest::Context ctx;

    ctx.setOption("abort-after",
                  1);  // default - stop after 5 failed asserts

    ctx.applyCommandLine(argc, argv);  // apply command line - argc / argv

    ctx.setOption("no-breaks",
                  true);  // override - don't break in the debugger

    int res = ctx.run();  // run test cases unless with --no-run

    if (ctx.shouldExit())  // query flags (and --exit) rely on this
    {
        return res;  // propagate the result of the tests
    }

    return 0;
}
