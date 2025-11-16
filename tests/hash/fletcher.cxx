#include "tarp/timeutils.hxx"
#include <tarp/hash/checksum/fletcher.hxx>
#include <tarp/ioutils.hxx>
#include <tarp/random.hxx>
#include <tarp/stopwatch.hxx>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <endian.h>

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using byte_vector = std::vector<std::uint8_t>;
namespace rd = tarp::random;
using namespace tarp::hash::checksum;

// implementations taken from
// https://en.wikipedia.org/wiki/Fletcher%27s_checksum
// in order to be able to test my implementations;
// I can only assume the wikipedia code is correct;
// finding reference test vectors for this checksum
// appears impossible beyond the basic (and very very short)
// strings in the wikipedia article.
namespace reference {
uint16_t fletcher16(const uint8_t *data, size_t len) {
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;
    std::size_t index;

    for (index = 0; index < len; ++index) {
        sum1 = (sum1 + data[index]) % 255;
        sum2 = (sum2 + sum1) % 255;
    }

    return (sum2 << 8) | sum1;
}

uint32_t fletcher32(const uint8_t *buff, size_t len) {
    const std::uint16_t *data = reinterpret_cast<const std::uint16_t *>(buff);

    if (len % 2 > 0) {
        throw std::invalid_argument("expects len to be a multiple of 2");
    }

    uint32_t sum1 = 0;
    uint32_t sum2 = 0;

    constexpr std::uint16_t M = std::numeric_limits<std::uint16_t>::max();

    while (len > 0) {
        std::uint16_t value = 0;
        std::memcpy(&value, data, sizeof(std::uint16_t));
        sum1 = (sum1 + value) % M;
        sum2 = (sum2 + sum1) % M;
        data++;
        len -= sizeof(std::uint16_t);
    }

    return (sum2 << 16) | sum1;
}

uint64_t fletcher64(const uint8_t *buff, size_t len) {
    const std::uint32_t *data = reinterpret_cast<const std::uint32_t *>(buff);

    if (len % 4 > 0) {
        throw std::invalid_argument("expects len to be a multiple of 4");
    }

    uint64_t sum1 = 0;
    uint64_t sum2 = 0;

    constexpr std::uint32_t M = std::numeric_limits<std::uint32_t>::max();

    while (len > 0) {
        std::uint32_t value = 0;
        std::memcpy(&value, data, sizeof(std::uint32_t));
        sum1 = (sum1 + value) % M;
        sum2 = (sum2 + sum1) % M;
        data++;
        len -= sizeof(std::uint32_t);
    }

    return (sum2 << 32) | sum1;
}

};  // namespace reference

template<typename context_t>
typename context_t::checksum_t process_in_blocks(context_t &ctx,
                                                 const byte_vector &bytes) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const std::size_t blocksz =
          remaining > 0 ? rd::randsz(1, remaining) : 0;

        if (blocksz == 0) break;

        std::cerr << "-- processing block of size " << blocksz << " at offset "
                  << offset << " in input of size " << bytes.size()
                  << std::endl;

        tarp::hash::checksum::fletcher::impl::update(
          ctx, bytes.data() + offset, blocksz);
        offset += blocksz;
    }

    return tarp::hash::checksum::fletcher::impl::get_checksum(ctx);
}

template<typename cb_t, typename T>
std::pair<bool, T> test(cb_t f, T expected_result, const std::string &input) {
    std::vector<std::uint8_t> bytes;
    for (auto c : input) {
        bytes.push_back(c);
    }

    auto res = f(bytes);
    return {(res == expected_result), res};
}


#if 1
TEST_CASE("basics") {
    std::uint64_t NUM_TESTS = 0;
    std::uint64_t NUM_PASSED = 0;

    // Columns: host-byte-order input string, network-byte-order input string,
    // expected result. The expected result is the same in both cases but
    // when handling the network byte order input, endianness conversion must be
    // performed by the algorithm (which is the point of the test).

    // These test input strings are taken from wikipedia
    // (https://en.wikipedia.org/wiki/Fletcher%27s_checksum)

    // endianness does not matter here since every block is 1 single byte.
    std::vector<std::tuple<std::string, std::uint16_t>> fletcher16_tests {
      {"abcde",    0xC8F0},
      {"abcdef",   0x2057},
      {"abcdefgh", 0x0627},
    };

    std::vector<std::tuple<std::string, std::uint32_t>> fletcher32_tests {
      {"abcde",    0xF04FC729},
      {"abcdef",   0x56502D2A},
      {"abcdefgh", 0xEBE19591},
    };

    std::vector<std::tuple<std::string, std::uint64_t>> fletcher64_tests {
      {"abcde",    0xC8C6C527646362C6},
      {"abcdef",   0xC8C72B276463C8C6},
      {"abcdefgh", 0x312E2B28CCCAC8C6},
    };

    auto print_passed_or_not =
      [](bool passed, const auto &input, auto expected, auto actual) {
          std::cerr << "[" << (passed ? "x" : " ") << "]" << " : " << input
                    << ". Results: expected=" << expected
                    << ", actual=" << actual << std::endl;
      };

    std::cerr << "Fletcher16 tests: \n";
    for (const auto &[input, expected_result] : fletcher16_tests) {
        auto [passed, res] = test(
          [](auto in) {
              return fletcher16(&in[0], in.size());
          },
          expected_result,
          input);

        NUM_TESTS++;
        if (passed) NUM_PASSED++;

        print_passed_or_not(passed, input, expected_result, res);
    }

    std::cerr << "Fletcher32 tests: \n";
    for (const auto &[input, expected_result] : fletcher32_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher32(&in[0], in.size());
              },
              expected_result,
              input);

            NUM_TESTS++;
            if (passed) NUM_PASSED++;

            print_passed_or_not(passed, false, expected_result, res);
        }
    }

    std::cerr << "Fletcher64 tests: \n";
    for (const auto &[input, expected_result] : fletcher64_tests) {
        {
            auto [passed, res] = test(
              [](auto in) {
                  return fletcher64(&in[0], in.size());
              },
              expected_result,
              input);

            NUM_TESTS++;
            if (passed) NUM_PASSED++;

            print_passed_or_not(passed, false, expected_result, res);
        }
    }

    std::cerr << "\n"
              << "PASSED: " << NUM_PASSED << "/" << NUM_TESTS << std::endl;
    REQUIRE(NUM_PASSED == NUM_TESTS);
}

TEST_CASE("misc basic checks") {
    {
        byte_vector input = {'a', 'b', 'c', 'd', 'e'};
        fletcher16_ctx::checksum_t expected = 51440;
        fletcher16_ctx ctx;
        auto actual =
          tarp::hash::checksum::fletcher16(input.data(), input.size());

        REQUIRE(reference::fletcher16(input.data(), input.size()) == expected);
        REQUIRE(actual == expected);
    }

    std::cerr << "-------------\n";
    {
        byte_vector input = {'a', 'b', 'c', 'd', 'e', 0};
        fletcher32_ctx::checksum_t expected = 0xF04FC729;
        fletcher32_ctx ctx;
        auto actual =
          tarp::hash::checksum::fletcher32(input.data(), input.size());

        REQUIRE(reference::fletcher32(input.data(), input.size()) == expected);
        REQUIRE(actual == expected);
    }
    std::cerr << "-------------\n";
}

TEST_CASE("unaligned inputs, fletcher32") {
    std::cerr << "\n\n";
    // must be aligned to u16 for fletcher32, else the reference function
    // cannot safely be called.

    byte_vector input = {0xaa, 0xbb, 0xcc, 0xdd};
    fletcher32_ctx ctx;

    std::cerr << "START\n";
    // process a block of size 3, then one of size 1
    fletcher::impl::update(ctx, input.data(), 3);
    fletcher::impl::update(ctx, input.data() + 3, 1);
    auto cksum = fletcher::impl::get_checksum(ctx);

    std::cerr << "END\n";
    auto ref = reference::fletcher32(input.data(), input.size());

    REQUIRE(cksum == ref);
}

TEST_CASE("fletcher fuzz test - random inputs and block sizes") {
    constexpr std::size_t N = 1000;

    for (std::size_t i = 0; i < N; ++i) {
        // Generate random input
        auto buffsz = rd::randsz(4, 120'000);

        // ensure a multiple of 4 so that all reference functions work,
        // as they expect a u8,u16,and u32 pointer respectively.
        if (buffsz % 4) buffsz += (4 - buffsz % 4);

        auto buff = rd::bytes(buffsz);

        std::cerr << "Test " << i << "/" << N << " buffsz=" << buffsz
                  << std::endl;

        // ============ Fletcher-16 ============
        {
            // 1. Direct API call (all data at once)
            const auto direct =
              tarp::hash::checksum::fletcher16(buff.data(), buff.size());

            // 2. Process in random-sized blocks
            fletcher16_ctx ctx_blocks;
            const auto blocks = process_in_blocks(ctx_blocks, buff);

            // 3. Reference implementation
            const auto reference =
              reference::fletcher16(buff.data(), buff.size());

            REQUIRE(direct == reference);
            REQUIRE(blocks == reference);
            REQUIRE(direct == blocks);

            std::stringstream ss;
            ss << "processing fletcher16 buffer of size=" << buff.size()
               << std::endl;

            REQUIRE_MESSAGE(direct == reference, ss.str());
            REQUIRE_MESSAGE(blocks == reference, ss.str());
            REQUIRE_MESSAGE(direct == blocks, ss.str());
        }

        // ============ Fletcher-32 ============
        {
            // 1. Direct API call (all data at once)
            const auto direct =
              tarp::hash::checksum::fletcher32(buff.data(), buff.size());

            // 2. Process in random-sized blocks
            fletcher32_ctx ctx_blocks;
            const auto blocks = process_in_blocks(ctx_blocks, buff);

            // 3. Reference implementation (needs u16 pointer)
            // Note: Wikipedia impl expects length in bytes, will round up to
            // u16 words
            const auto reference =
              reference::fletcher32(buff.data(), buff.size());

            std::stringstream ss;
            ss << "processing fletcher32 buffer of size=" << buff.size()
               << std::endl;

            REQUIRE_MESSAGE(direct == reference, ss.str());
            REQUIRE_MESSAGE(blocks == reference, ss.str());
            REQUIRE_MESSAGE(direct == blocks, ss.str());
        }

        // ============ Fletcher-64 ============
        {
            // 1. Direct API call (all data at once)
            const auto direct =
              tarp::hash::checksum::fletcher64(buff.data(), buff.size());

            // 2. Process in random-sized blocks
            fletcher64_ctx ctx_blocks;
            const auto blocks = process_in_blocks(ctx_blocks, buff);

            // 3. Reference implementation (needs u32 pointer)
            auto reference = reference::fletcher64(buff.data(), buff.size());

            std::stringstream ss;
            ss << "processing fletcher64 buffer of size=" << buff.size()
               << std::endl;

            REQUIRE_MESSAGE(direct == reference, ss.str());
            REQUIRE_MESSAGE(blocks == reference, ss.str());
            REQUIRE_MESSAGE(direct == blocks, ss.str());
        }
    }

    std::cerr << "All " << N << " fuzz tests passed!" << std::endl;
}

TEST_CASE("fletcher edge cases") {
    // Empty input
    {
        byte_vector empty;

        auto f16_direct = tarp::hash::checksum::fletcher16(empty.data(), 0);
        auto f16_ref = reference::fletcher16(empty.data(), 0);
        REQUIRE(f16_direct == f16_ref);

        fletcher16_ctx ctx16;
        auto f16_blocks = process_in_blocks(ctx16, empty);
        REQUIRE(f16_blocks == f16_ref);
    }

    // Single byte
    {
        byte_vector single = {0x42};

        auto f16_direct = tarp::hash::checksum::fletcher16(single.data(), 1);
        auto f16_ref = reference::fletcher16(single.data(), 1);
        REQUIRE(f16_direct == f16_ref);
    }

    // All zeros
    {
        byte_vector zeros(1000, 0);

        auto f16_direct =
          tarp::hash::checksum::fletcher16(zeros.data(), zeros.size());
        auto f16_ref = reference::fletcher16(zeros.data(), zeros.size());
        REQUIRE(f16_direct == f16_ref);

        auto f32_direct =
          tarp::hash::checksum::fletcher32(zeros.data(), zeros.size());
        auto f32_ref = reference::fletcher32(zeros.data(), zeros.size());
        REQUIRE(f32_direct == f32_ref);
    }

    // All 0xFF (max values)
    {
        byte_vector maxvals(1000, 0xFF);

        auto f16_direct =
          tarp::hash::checksum::fletcher16(maxvals.data(), maxvals.size());
        auto f16_ref = reference::fletcher16(maxvals.data(), maxvals.size());
        REQUIRE(f16_direct == f16_ref);
    }

    // Powers of 2 lengths
    for (std::size_t len : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}) {
        byte_vector data = rd::bytes(len);

        auto f16_direct = tarp::hash::checksum::fletcher16(data.data(), len);
        auto f16_ref = reference::fletcher16(data.data(), len);
        REQUIRE(f16_direct == f16_ref);

        // reference implementation only works with lengths that are u16 aligned
        if (len >= 2) {
            auto f32_direct =
              tarp::hash::checksum::fletcher32(data.data(), len);
            auto f32_ref = reference::fletcher32(data.data(), len);
            REQUIRE(f32_direct == f32_ref);
        }

        // reference implementation only works with lengths that are u32 aligned
        if (len >= 4) {
            auto f64_direct =
              tarp::hash::checksum::fletcher64(data.data(), len);
            auto f64_ref = reference::fletcher64(data.data(), len);
            REQUIRE(f64_direct == f64_ref);
        }
    }

    std::cerr << "All edge case tests passed!" << std::endl;
}

TEST_CASE("fletcher deficit handling - unaligned buffers") {
    // Test the joint/deficit mechanism by processing data in small chunks
    // that don't align to word boundaries

    constexpr std::size_t N = 50;

    for (std::size_t i = 0; i < N; ++i) {
        auto buffsz = rd::randsz(1, 1000);

        // ensure multiple of 4 so it is safe to call reference
        // functions
        if (buffsz % 4) buffsz += (4 - buffsz % 4);

        auto buff = rd::bytes(buffsz);

        // Fletcher-32: process in 1-byte chunks to stress test deficit handling
        {
            fletcher32_ctx ctx;
            for (std::size_t j = 0; j < buff.size(); ++j) {
                tarp::hash::checksum::fletcher::impl::update(ctx, &buff[j], 1);
            }
            auto byte_by_byte =
              tarp::hash::checksum::fletcher::impl::get_checksum(ctx);

            auto reference = reference::fletcher32(buff.data(), buff.size());

            REQUIRE(byte_by_byte == reference);
        }

        // Fletcher-64: process in 3-byte chunks (not aligned to u32)
        {
            fletcher64_ctx ctx;
            std::size_t offset = 0;
            while (offset < buff.size()) {
                std::size_t chunk = std::min(size_t(3), buff.size() - offset);
                tarp::hash::checksum::fletcher::impl::update(
                  ctx, buff.data() + offset, chunk);
                offset += chunk;
            }
            auto unaligned =
              tarp::hash::checksum::fletcher::impl::get_checksum(ctx);

            auto reference = reference::fletcher64(buff.data(), buff.size());

            REQUIRE(unaligned == reference);
        }
    }

    std::cerr << "Deficit handling tests passed!" << std::endl;
}

// Tests
// - odd length, even lengths, power of 2 lengths;
// - odd blocks, even blocks, power of 2 blocks;
// - blocks that exceed the length of the buffer size
// - and various scenarios that crop up combinatorially
// NOTE: we don't compare against the reference checksum
// functions since those do not work with odd buffers etc;
// however, the behavior of the internal implementation
// should've already been validated by earlier tests.
TEST_CASE("fletcher block processing - systematic coverage") {
    const std::vector<std::size_t> buffer_sizes = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,   9,   10,  11,  12,  13, 14, 15,
      16, 17, 21, 23, 26, 28, 40, 59, 60,  61,  62,  63,  64,  65, 90, 91,
      92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105};

    const std::vector<std::size_t> block_sizes = {
      1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
      13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

    // Generate ONE random buffer for all tests to use
    // This ensures consistency across all variants
    const auto max_size =
      *std::max_element(buffer_sizes.begin(), buffer_sizes.end());
    const auto reference_buffer = rd::bytes(max_size);

    for (const auto buffsz : buffer_sizes) {
        byte_vector buff(reference_buffer.begin(),
                         reference_buffer.begin() + buffsz);

        // Fletcher-16
        {
            const auto direct =
              tarp::hash::checksum::fletcher16(buff.data(), buff.size());
            const auto reference =
              reference::fletcher16(buff.data(), buff.size());
            REQUIRE(direct == reference);

            for (const auto blocksz : block_sizes) {
                if (blocksz > buffsz && buffsz > 0)
                    continue;  // Skip impossible cases

                fletcher16_ctx ctx;
                std::size_t offset = 0;
                while (offset < buff.size()) {
                    std::cerr << "Checking Fletcher16 with input of size "
                              << buff.size() << " and block of size " << blocksz
                              << " offset=" << offset << std::endl;

                    std::size_t chunk = std::min(blocksz, buff.size() - offset);
                    tarp::hash::checksum::fletcher::impl::update(
                      ctx, buff.data() + offset, chunk);
                    offset += chunk;
                }
                const auto blocks =
                  tarp::hash::checksum::fletcher::impl::get_checksum(ctx);

                REQUIRE(blocks == direct);
            }
        }

        // Fletcher-32
        {
            auto direct =
              tarp::hash::checksum::fletcher32(buff.data(), buff.size());

            const auto padded = tarp::utils::io::get_aligned_buffer(buff, 2);
            const auto reference =
              reference::fletcher32(padded.data(), padded.size());
            REQUIRE(direct == reference);

            for (const auto blocksz : block_sizes) {
                if (blocksz > buffsz && buffsz > 0) continue;

                fletcher32_ctx ctx;
                std::size_t offset = 0;
                while (offset < buff.size()) {
                    std::cerr << "Checking Fletcher32 with input of size "
                              << buff.size() << " and block of size " << blocksz
                              << " offset=" << offset << std::endl;
                    std::size_t chunk = std::min(blocksz, buff.size() - offset);
                    tarp::hash::checksum::fletcher::impl::update(
                      ctx, buff.data() + offset, chunk);
                    offset += chunk;
                }
                auto blocks =
                  tarp::hash::checksum::fletcher::impl::get_checksum(ctx);

                REQUIRE(blocks == direct);
            }
        }

        // Fletcher-64
        {
            const auto direct =
              tarp::hash::checksum::fletcher64(buff.data(), buff.size());

            const auto padded = tarp::utils::io::get_aligned_buffer(buff, 4);
            const auto reference =
              reference::fletcher64(padded.data(), padded.size());
            REQUIRE(direct == reference);

            for (const auto blocksz : block_sizes) {
                if (blocksz > buffsz && buffsz > 0) continue;

                fletcher64_ctx ctx;
                std::size_t offset = 0;
                while (offset < buff.size()) {
                    std::cerr << "Checking Fletcher64 with input of size "
                              << buff.size() << " and block of size " << blocksz
                              << " offset=" << offset << std::endl;
                    std::size_t chunk = std::min(blocksz, buff.size() - offset);
                    tarp::hash::checksum::fletcher::impl::update(
                      ctx, buff.data() + offset, chunk);
                    offset += chunk;
                }
                auto blocks =
                  tarp::hash::checksum::fletcher::impl::get_checksum(ctx);

                REQUIRE(blocks == direct);
            }
        }
    }

    std::cerr << "Systematic block processing test passed!" << std::endl;
}
#endif

TEST_CASE("byte-at-a-time perf") {

    fletcher32_ctx ctx;
    const auto buff = rd::bytes(10'100'000);
    const auto *ptr = buff.data();

    const auto cksum = fletcher32(buff.data(), buff.size());

    tarp::StopWatch<tarp::time_utils::fractional_ms> sw;
    sw.start();
    for (std::size_t i = 0; i < buff.size(); ++i) {
        fletcher::fletcher32::update(ctx, ptr + i, 1);
    }
    const auto cksum2 = fletcher::fletcher32::get_checksum(ctx);
    sw.stop();
    REQUIRE(cksum == cksum2);

    std::cerr << "Fletcher32 byte-at-a-time on buffer of size " << buff.size()
              << " took " << sw.get_time().count() << "ms" << std::endl;


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
