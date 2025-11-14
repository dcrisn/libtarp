#include <algorithm>
#include <cstring>
#include <tarp/bits.hxx>
#include <tarp/hash/checksum/inet.hxx>
#include <tarp/ioutils.hxx>
#include <tarp/random.hxx>
#include <tarp/string_utils.hxx>

#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

namespace str = tarp::utils::str;
namespace io = tarp::utils::io;
namespace cksum = tarp::hash::checksum;
namespace rd = tarp::random;

using byte_vector = std::vector<std::uint8_t>;

// Function taken from RFC1071-4.1, with the only modifications beying
// syntactical and replacing the c-cast with reinterpret_cast.
// This can be used for validating other implementations.
static std::uint16_t inet_checksum_rfc(const std::uint8_t *buff,
                                       std::size_t len) {
    std::uint32_t sum = 0;

    // unsigned i = 0;
    while (len > 1) {
        /*  This is the inner loop */
        const std::uint16_t v = *reinterpret_cast<const std::uint16_t *>(buff);
        sum += v;
        // std::cerr << "iteration " << i++ << ", pushed v=" << v
        //           << ", got=" << sum << std::endl;
        buff += 2;
        len -= 2;
    }

    /*  Add left-over byte, if any */
    if (len > 0) {
        const std::uint8_t v = *reinterpret_cast<const std::uint8_t *>(buff);
        sum += v;
        // std::cerr << "pushed v=" << static_cast<unsigned>(v) << ", got=" <<
        // sum
        //<< std::endl;
    }

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // RFC791-3.1:
    // The checksum field is the 16 bit one's complement of the
    // one's complement sum of all 16 bit words in the header.
    const std::uint16_t cksum = ~sum;
    return cksum;
}

static std::uint16_t process_in_blocks(tarp::hash::checksum::inetcksum_ctx &ctx,
                                       const std::vector<std::uint8_t> &bytes,
                                       std::size_t blocksz) {
    // std::cerr << "Processing input of size " << bytes.size() << std::endl;
    std::size_t len = bytes.size();
    std::size_t offset = 0;

    // must still invoke once! otherwise a checksum that is all 0s
    // will get us a result of all 0s, when in fact it should be ~0!
    // so we have to go through the function still.
    while (len >= blocksz) {
        std::cerr << "processing block of size " << std::dec << blocksz
                  << " at offset " << offset << " ctx.sum=" << ctx.sum
                  << std::endl;
        cksum::inet::update_checksum(ctx, bytes.data() + offset, blocksz);
        len -= blocksz;
        offset += blocksz;
    }

    // if len not a multiple of the blocksz, we have one pass left.
    if (len > 0) {
        cksum::inet::update_checksum(ctx, bytes.data() + offset, len);
        std::cerr << "processing block of size " << std::dec << len
                  << " at offset " << offset << " ctx.sum=" << ctx.sum
                  << std::endl;
    }

    return cksum::inet::get_checksum(ctx);
}

#if 1
// As per RFC1071, if a big endian machine and small endian
// machine process the _same_ sequence of bytes hence being
// interpreted differently based on the machine's endianness),
// the same checksum is produced, albeit byte-swapped.
// The following explicitly checks this is the case with
// the current implementation.
//
// Of particular concern here is also what happens when the input
// buffer length is NOT a multiple of sizeof(u16). To understand
// what the issue is, consider the following sequence of bytes:
// {a, b, c}
// A little endian machine would form the following words:
// (L=a,H=b), (L=c)
// which is exactly what you would expect given the above.
// A big-endian machine however would form:
// (L=b,H=a), (L=c)
// why?
//
// BUT: if we want to test the big-endian behavior on a little endian
// machine we must perform byte-swapping explicitly.
// And given {a, b, c}, unless we append a 0, we would end up with:
//  (L=a,H=b) => swap => (L=b,H=a)
//  (L=c,)
TEST_CASE("check hbo and nbo bytes produce same checksum") {
    std::vector<std::vector<std::uint8_t>> inputs {
      {},
      {0x0},
      {0x1},
      {0x2, 0x3},
      {0x5, 0x4, 0x1},
      {0x4, 0x1, 0x7, 0x9},
      {0x4, 0x1, 0x7, 0x9, 0x11},
    };

    constexpr std::size_t N = 8000;
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::size_t> pick_size(0, 100);
    std::uniform_int_distribution<std::size_t> pick_blocksize(1, 100 + 10);
    std::bernoulli_distribution use_blocked(0.5);

    for (unsigned i = 0; i < N; ++i) {
        inputs.push_back(io::get_random_bytes(pick_size(rng)));
    }

    for (const auto &hboin : inputs) {
        std::cerr << "####################\n";
        auto nboin = hboin;

        // NOTE: here we MUST pad a 0 if we have a trailing byte;
        // otherwise on a little endian machine (likely the testing
        // machine) we won't correctly mimic what the input to
        // a big-endian machine would be. Since a trailing byte
        // memcpied to a u16 would end up being the HIGH byte
        // on a big-endian machine, but a LOW byte on a little
        // endian machine; however, if we append a trailing
        // byte, then we SWAP, hence the LOW byte on the little
        // endian machine will end up being shifted to the HIGH
        // byte, hence resulting in the same word that a big-endian
        // machine would process.
        // Consider an input of {a,b,c}, vs {a,b,c,0}.
        tarp::utils::string_utils::endian_swap<std::uint16_t>(nboin, true);

        tarp::hash::checksum::inetcksum_ctx hboctx;
        tarp::hash::checksum::inetcksum_ctx nboctx;

        std::uint16_t a = 0;
        std::uint16_t b = 0;

        if (use_blocked(rng)) {
            a = process_in_blocks(hboctx, hboin, pick_blocksize(rng));
        } else {
            tarp::hash::checksum::inet::update_checksum(
              hboctx, hboin.data(), hboin.size());
            a = tarp::hash::checksum::inet::get_checksum(hboctx);
        }

        if (use_blocked(rng)) {
            std::cerr << "b is processed in blocks also!\n";
            b = process_in_blocks(nboctx, nboin, pick_blocksize(rng));
        } else {
            std::cerr << "updating checksum\n";
            tarp::hash::checksum::inet::update_checksum(
              nboctx, nboin.data(), nboin.size());
            b = tarp::hash::checksum::inet::get_checksum(nboctx);
            std::cerr << "updateded checksum to " << std::hex << b
                      << " from sum=" << std::dec << nboctx.sum << std::endl;
        }

        std::cerr << "HBO input=" << str::to_string(hboin) << std::endl;
        std::cerr << "HBO ctx.sum=" << std::hex << hboctx.sum << std::endl;
        std::cerr << "HBO checksum=" << std::hex << a << std::endl;
        std::cerr << "\n";
        std::cerr << "NBO input=" << str::to_string(nboin) << std::endl;
        std::cerr << "NBO ctx.sum=" << std::hex << nboctx.sum << std::endl;
        std::cerr << "NBO checksum=" << std::hex << b << std::endl;

        REQUIRE(a == tarp::bits::to_hbo(b));
    }
}

// when you calculate the checksum over a span of bytes that INCLUDES
// the checksum itself (e.g. at the receiver), you should get 0xffff.
// RFC1071-1.3:
// > To check a checksum, the 1's complement sum is computed over the
// > same set of octets, including the checksum field.  If the result
// > is all 1 bits (-0 in 1's complement arithmetic), the check
// > succeeds.
TEST_CASE(" test calculation over bytes including checksum") {
    constexpr std::size_t N = 100;
    // constexpr std::size_t N = 1;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::size_t> pick_len(1, 120);

    const auto get_cksum = [&](const auto &buf) {
        // return inet_checksum_rfc(buf.data(), buf.size());
        return tarp::hash::checksum::inetv4(buf.data(), buf.size());
    };

    for (unsigned i = 0; i < N; ++i) {
        // sender checksum: b861 --> NOT inlcuded when the sender computes it
        // receiver includes it when computing it; gets 0 = success.

        const auto input = io::get_random_bytes(pick_len(rng));

        auto sender_cksum = get_cksum(input);

        // sender_cksum = tarp::bits::to_nbo(sender_cksum);

        auto bytes = input;
        const auto n = 2 + (bytes.size() % 2 ? 1 : 0);
        bytes.resize(bytes.size() + n);

        std::memcpy(bytes.data() + bytes.size() - 2,
                    &sender_cksum,
                    sizeof(std::uint16_t));


        auto receiver_cksum = get_cksum(bytes);

        std::stringstream ss;
        ss << "\n------------------\n";
        ss << "Data:      " << str::to_string(input) << std::endl;
        ss << "SND.cksum: " << std::hex << sender_cksum << std::endl;
        ss << "Received: " << str::to_string(bytes) << std::endl;
        ss << "RCV.cksum: " << std::hex << receiver_cksum << std::endl;

        REQUIRE_MESSAGE(receiver_cksum == 0x00, ss.str());
    }
}

TEST_CASE(
  "check padding required when computing checksum to check against all 0s") {
    // example extracted from:
    // https://en.wikipedia.org/wiki/Internet_checksum.
    // first, checksum bytes with checksum field zeroed;
    auto input = std::vector<std::uint8_t> {
      0x00, 0x45, 0x73, 0x00, 0x00, 0x00, 0x00, 0x40, 0x11, 0x40,
      0x00, 0x00, 0xa8, 0xc0, 0x01, 0x00, 0xa8, 0xc0, 0xc7, 0x00};

    auto expected_checksum = 0xb861;
    std::uint16_t result =
      tarp::hash::checksum::inetv4(input.data(), input.size());
    REQUIRE(result == expected_checksum);

    bool oddlen = input.size() & 0x1;
    REQUIRE(!oddlen);

    // if we append the checksum to an even-length buffer,
    // and then calculaet the checksum over the entire thing
    // (that is compute the checksum over buff+cksum),
    // we should get checksum=0 if valid.
    auto tmp = input;
    tmp.resize(tmp.size() + 2);
    std::memcpy(tmp.data() + tmp.size() - 2, &result, sizeof(result));
    result = tarp::hash::checksum::inetv4(tmp.data(), tmp.size());
    REQUIRE(result == 0);

    // However, with an input of ODD length this would not work.
    input.push_back(0x77);
    oddlen = input.size() & 0x1;
    REQUIRE(oddlen);

    result = tarp::hash::checksum::inetv4(input.data(), input.size());
    tmp = input;
    tmp.resize(tmp.size() + 2);
    std::memcpy(tmp.data() + tmp.size() - 2, &result, sizeof(result));
    result = tarp::hash::checksum::inetv4(tmp.data(), tmp.size());
    REQUIRE(result != 0);

    // if the input is odd-length AND you want to append the checksum
    // to it in order to check 'cksum==0' for validity, then
    // you have to pad to buffer first with a 0 to make it event-len,
    // before computing the checksum.
    input.push_back(0x0);
    oddlen = input.size() & 0x1;
    REQUIRE(!oddlen);

    result = tarp::hash::checksum::inetv4(input.data(), input.size());
    tmp = input;
    tmp.resize(tmp.size() + 2);
    std::memcpy(tmp.data() + tmp.size() - 2, &result, sizeof(result));
    result = tarp::hash::checksum::inetv4(tmp.data(), tmp.size());
    REQUIRE(result == 0);
}

TEST_CASE("validate checksum against reference") {
    constexpr std::size_t N = 5000;

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::size_t> pick_len(0, 40'000);

    for (std::size_t i = 0; i < N; ++i) {
        // nbo, hbo, blocked and unblocked, and hb==swapped(nbo)
        std::cerr << "\n\n";
        std::cerr << " === TEST: " << std::dec << i << " ===" << std::endl;

        const auto hbo_input = io::get_random_bytes(pick_len(rng));
        auto nbo_input = hbo_input;
        str::endian_swap<std::uint16_t>(nbo_input, true);

        std::string e;

        // allow for bigger blocks than the input itself, to test
        // that possibility as well.
        std::uniform_int_distribution<std::size_t> pick_blocksz(
          1, hbo_input.size() + 10);

        //

        // --------- HBO
        tarp::hash::checksum::inetcksum_ctx hboctx;

        std::cerr << "Computing HBO reference result...\n";
        const auto hbo_input_reference_result =
          inet_checksum_rfc(hbo_input.data(), hbo_input.size());

        std::cerr << "Computing HBO one-shot result...\n";
        const auto hbo_input_result =
          cksum::inetv4(hbo_input.data(), hbo_input.size());

        std::cerr << "Computing HBO blocked result...\n";
        const bool hbo_input_result_valid =
          hbo_input_result == hbo_input_reference_result;

        const auto blocksz = pick_blocksz(rng);
        const auto hbo_input_result_blocked =
          process_in_blocks(hboctx, hbo_input, blocksz);
        const bool hbo_input_result_blocked_valid =
          hbo_input_result_blocked == hbo_input_reference_result;

        REQUIRE(hbo_input_result_valid);
        REQUIRE(hbo_input_result_blocked_valid);

        // --------- NBO
        tarp::hash::checksum::inetcksum_ctx nboctx;
        std::cerr << "Computing NBO reference result...\n";
        const auto nbo_input_reference_result =
          inet_checksum_rfc(nbo_input.data(), nbo_input.size());

        std::cerr << "Computing NBO one-shot result...\n";
        const auto nbo_input_result =
          cksum::inetv4(nbo_input.data(), nbo_input.size());

        const bool nbo_input_result_valid =
          nbo_input_result == nbo_input_reference_result;

        std::cerr << "Computing NBO blocked result...\n";
        const auto nbo_input_result_blocked =
          process_in_blocks(nboctx, nbo_input, blocksz);
        const bool nbo_input_result_blocked_valid =
          nbo_input_result_blocked == nbo_input_reference_result;

        REQUIRE(nbo_input_result_valid);
        REQUIRE(nbo_input_result_blocked_valid);

        std::cerr << "~~~ CHECK:" << " HBO checksum=" << std::hex
                  << hbo_input_reference_result << " == to_hbo("
                  << "NBO_checksum=" << std::hex << nbo_input_reference_result
                  << ")=" << tarp::bits::to_hbo(nbo_input_reference_result);

        // the checksums must be identical for the nbo and hbo
        // inputs -- only swapped
        REQUIRE(hbo_input_reference_result ==
                tarp::bits::to_hbo(nbo_input_reference_result));
    }
}

TEST_CASE("basic incremental checksum update") {
    using namespace tarp::hash::checksum;

    // Example taken from rfc1624-4.
    //--------------------
    tarp::hash::checksum::inetcksum_ctx ctx, ctx2;
    std::uint16_t oldfield = 0x5555;
    std::uint16_t newfield = 0x3285;
    ctx.sum = 0x22D0;
    REQUIRE(inet::get_checksum(ctx) == 0xDD2F);
    inet::update_checksum<false>(ctx, oldfield, newfield);
    REQUIRE(inet::get_checksum(ctx) == 0x0000);
    //--------------------

    //
    ctx = {};
    std::cerr << "ctx.sum is " << ctx.sum << std::endl;
    byte_vector buff {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::cerr << "#1\n";
    inet::update_checksum(ctx, buff.data(), buff.size());
    std::cerr << "#2\n";
    auto cksum = inet::get_checksum(ctx);
    // let's now say {0xc, 0xd} represents a u16 field
    // we want to update to {0x8,0x8}
    oldfield = 0xddcc;
    newfield = 0x8888;
    std::cerr << "#3\n";
    inet::update_checksum<false>(ctx, oldfield, newfield);
    std::cerr << "#4\n";
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // check that the result is the same as if we caculated the
    // checksum from scratch
    byte_vector buff2 {0xaa, 0xbb, 0x88, 0x88, 0xee, 0xff};
    std::cerr << "#5\n";
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    std::cerr << "#6\n";
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum.
    inet::update_checksum<false>(ctx, newfield, oldfield);
    REQUIRE(inet::get_checksum(ctx) == cksum);

    // now let's trying updating a field that is u32:
    // {cc,dd,ee,ff}
    ctx = {};
    inet::update_checksum(ctx, buff.data(), buff.size());
    cksum = inet::get_checksum(ctx);
    std::uint32_t oldu32field = 0xffeeddcc;
    std::uint32_t newu32field = 0x44332211;
    inet::update_checksum<false>(ctx, oldu32field, newu32field);
    REQUIRE(inet::get_checksum(ctx) != cksum);

    ctx2 = {};
    buff2 = {0xaa, 0xbb, 0x11, 0x22, 0x33, 0x44};
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum.
    inet::update_checksum<false>(ctx, newu32field, oldu32field);
    REQUIRE(inet::get_checksum(ctx) == cksum);

    // NOW: a fundamental thing that RFC1624 (and earlier ones)
    // neglects to mention is that the incremental update
    // only works IF the field being updated starts (in the buffer
    // that was used for computing the checksum being updated)
    // at an offset that is a multiple of u16 AND the field
    // length is a multiple of sizeof(u16).
    // Otherwise you would have to manually swap bytes to make
    // sure the values you subtract and add to the sum
    // have the same significance as when the checksum was
    // computed previously!
    // To understand the issue, consider an odd-length buffer:
    // {a, b, c, d, e}
    // Assume we have a u16 field to update and it consists of
    // bytes {b, c}. The incremental update algorithm assumes
    // u16 values are added and subtracted. But if we subtract
    // the 'old value' from the buffer (i.e. {b,c}), that is in
    // fact incorrect, since the u16s that would've been formed
    // using the given buffer are {a,b}, {c,d}, ... . Hence,
    // there is no {b,c} value that was added to the sum and
    // therefore we cannot subtract it. The problem of then
    // _adding_ a changed {b,c} field is the same: the
    // significance is incorrect.
    // This is shown in the following test.

    oldfield = 0xccbb;
    newfield = 0x7711;
    inet::update_checksum<false>(ctx, oldfield, newfield);
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // we will NOT get the same result as if we calculated the
    // checksum from scratch. Because the values
    // we add and subtract are actually different from
    // the ones used in the checksum computation, as explained
    // in the comment above.
    ctx2 = {};
    buff2 = {0xaa, 0x11, 0x77, 0x88, 0xee, 0xff};
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    REQUIRE_FALSE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // undoing the change gives back previous value
    inet::update_checksum<false>(ctx, newfield, oldfield);
    REQUIRE(inet::get_checksum(ctx) == cksum);

    //

    // NOTE: this also means that updating a field that is < sizeof(u16),
    // i.e. a u8 also will not work, since its sizeof is obviously
    // not a multiple of sizeof(u16) and hence it runs into the issue
    // described in the comment above.
    // However, cannot write a test for this, since the function
    // static assers that the input type is >= sizeof(u16), so it
    // will not compile.
}
#endif

#if 1
TEST_CASE("basic incremental checksum update -- buffer API") {
    using namespace tarp::hash::checksum;

    tarp::hash::checksum::inetcksum_ctx ctx, ctx2;

    byte_vector buff {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::cerr << "#1\n";
    inet::update_checksum(ctx, buff.data(), buff.size());
    std::cerr << "#2\n";
    auto cksum = inet::get_checksum(ctx);

    // let's now say {0xc, 0xd} represents a u16 field
    // we want to update to {0x8,0x8}
    byte_vector change = {0x88, 0x88};
    std::cerr << "#3\n";
    inet::update_checksum(ctx, buff.data(), buff.size(), 2, 2, change.data());
    std::cerr << "#4\n";
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // check that the result is the same as if we caculated the
    // checksum from scratch
    byte_vector buff2 {0xaa, 0xbb, 0x88, 0x88, 0xee, 0xff};
    std::cerr << "#5\n";
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    std::cerr << "#6\n";
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum. we use buff2 here,
    // since that's the original buffer _with the change applied_,
    // which we now want to undo.
    change = {0xcc, 0xdd};
    inet::update_checksum(ctx, buff2.data(), buff.size(), 2, 2, change.data());
    REQUIRE(inet::get_checksum(ctx) == cksum);

    // now let's trying updating a field that is u32:
    // {cc,dd,ee,ff}
    ctx = {};
    inet::update_checksum(ctx, buff.data(), buff.size());
    cksum = inet::get_checksum(ctx);
    change = {0x11, 0x22, 0x33, 0x44};
    inet::update_checksum(ctx, buff.data(), buff.size(), 2, 4, change.data());
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // calculate from scratch
    ctx2 = {};
    buff2 = {0xaa, 0xbb, 0x11, 0x22, 0x33, 0x44};
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum.
    change = {0xcc, 0xdd, 0xee, 0xff};
    inet::update_checksum(ctx, buff2.data(), buff2.size(), 2, 4, change.data());
    REQUIRE(inet::get_checksum(ctx) == cksum);

    //

    // unlike the other API that takes actual integers in, this buffer
    // based API works even if we start at an odd offset.
    ctx = {};
    inet::update_checksum(ctx, buff.data(), buff.size());
    cksum = inet::get_checksum(ctx);
    change = {0x11, 0x77};
    inet::update_checksum(ctx, buff.data(), buff.size(), 1, 2, change.data());
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // calculate from scratch
    ctx2 = {};
    buff2 = {0xaa, 0x11, 0x77, 0xdd, 0xee, 0xff};
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum.
    change = {0xbb, 0xcc};
    inet::update_checksum(ctx, buff2.data(), buff2.size(), 1, 2, change.data());
    REQUIRE(inet::get_checksum(ctx) == cksum);

    //

    // it also works if we update a single byte

    // unlike the other API that takes actual integers in, this buffer
    // based API works even if we start at an odd offset.
    ctx = {};
    inet::update_checksum(ctx, buff.data(), buff.size());
    cksum = inet::get_checksum(ctx);
    change = {0x35};
    inet::update_checksum(ctx, buff.data(), buff.size(), 3, 1, change.data());
    REQUIRE(inet::get_checksum(ctx) != cksum);

    // calculate from scratch
    ctx2 = {};
    buff2 = {0xaa, 0xbb, 0xcc, 0x35, 0xee, 0xff};
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    REQUIRE(inet::get_checksum(ctx2) == inet::get_checksum(ctx));

    // now if we undo the change, we should get
    // back the previous checksum.
    change = {0xdd};
    inet::update_checksum(ctx, buff2.data(), buff2.size(), 3, 1, change.data());
    REQUIRE(inet::get_checksum(ctx) == cksum);
}
#endif

void check_buffer_based_incremental_update(const byte_vector &input,
                                           byte_vector change,
                                           std::size_t change_offset) {
    using namespace tarp::hash::checksum;
    inetcksum_ctx ctx1, ctx2, ctx3;
    const std::size_t change_len = change.size();
    
    byte_vector buff2 = input;
    std::copy_n(change.begin() , change.size(), buff2.begin()+change_offset);

    std::cerr << "\n\nIncremental checksum update test: " << std::dec
              << "change_offset=" << change_offset
              << ", change_len=" << change_len << ", inputsz=" << input.size()
              << "\n"
              << "input=" << str::enumerate(input, 0, "[", "]", true)
              << "\nbuff2=" << str::enumerate(buff2, 0, "[", "]", true)
              << "\nchange=" << str::enumerate(change, 0, "[", "]", true);

    std::cerr
      << "\n------------------ calculating whole checksum on input buffer\n";
    // calculate from scratch with original input
    inet::update_checksum(ctx1, input.data(), input.size());
    const auto cksum1 = inet::get_checksum(ctx1);

    std::cerr
      << "\n------------------ calculating whole checksum on changed buffer\n";
    // calculate from scratch with changed overlaid
    inet::update_checksum(ctx2, buff2.data(), buff2.size());
    const auto cksum2 = inet::get_checksum(ctx2);

    std::cerr << "\n------------------ incremental checksum update\n";
    // now use the incremental update method
    // start from the original cksum state;
    ctx3 = ctx1;
    inet::update_checksum(ctx3,
                          input.data(),
                          input.size(),
                          change_offset,
                          change_len,
                          change.data());
    const auto cksum3 = inet::get_checksum(ctx3);

    // we get the same result when using incremental update method
    // as when we calculate the entire checksum
    REQUIRE(cksum3 == cksum2);

    // now if we undo the change, we should get
    // back the previous checksum.
    change.resize(change_len);
    std::copy_n(input.begin() + change_offset, change_len, change.begin());
    inet::update_checksum(ctx3,
                          buff2.data(),
                          buff2.size(),
                          change_offset,
                          change_len,
                          change.data());
    REQUIRE(inet::get_checksum(ctx3) == cksum1);
}

TEST_CASE("incremental update, buffer api, misc tests") {
    using namespace tarp::hash::checksum;
    inetcksum_ctx ctx1, ctx2, ctx3;
    constexpr std::size_t change_offset = 37;
    //constexpr std::size_t change_len = 19;
    //constexpr std::size_t buffersz = 86;

    byte_vector input = {
      114, 247, 55,  45,  117, 129, 89,  103, 212, 246, 118, 72,  208, 78,  50,
      100, 134, 14,  249, 246, 77,  221, 198, 226, 15,  168, 178, 91,  53,  16,
      19,  189, 146, 116, 27,  143, 24,  178, 44,  197, 19,  127, 218, 110, 169,
      54,  146, 187, 138, 191, 81,  234, 169, 92,  221, 103, 185, 91,  156, 127,
      184, 141, 51,  140, 217, 72,  94,  169, 68,  206, 91,  231, 88,  201, 148,
      160, 202, 50,  193, 132, 78,  230, 236, 219, 227, 231};

    byte_vector change = {168,
                          43,
                          77,
                          52,
                          24,
                          254,
                          26,
                          188,
                          59,
                          254,
                          31,
                          119,
                          203,
                          133,
                          15,
                          34,
                          1,
                          248,
                          191};

    check_buffer_based_incremental_update(input, change, change_offset);
}

#if 1
TEST_CASE("incremental checksum update fuzz") {
    using namespace tarp::hash::checksum;

    constexpr unsigned N = 1000;

    for (unsigned i = 0; i < N; ++i) {
        const auto fieldsz = rd::pick<unsigned>({2, 4, 8});
        const auto buffsz = rd::pick<std::size_t>(fieldsz, 121);
        const auto input = io::get_random_bytes(buffsz);
        auto field_offset = rd::pick<std::size_t>(0, buffsz - fieldsz);

        // include edge cases as well (0, buffsz)
        const auto change_offset = rd::pick<std::size_t>(0, buffsz);
        const auto change_len = rd::pick<std::size_t>(0, buffsz - change_offset);
        byte_vector change;

        // for the integer-based incremental update API,
        // if not a multiple of two it won't work,
        // so we have to adjust;
        // NOTE: for the buffer-based incremental update API,
        // no issues though.
        if (field_offset % 2) {
            if (field_offset + fieldsz < buffsz) {
                field_offset += 1;  // Align forward if there's room
            } else {
                field_offset -= 1;  // Otherwise align backward
            }
        }

        std::cerr << std::dec << "field_offset=" << field_offset
                  << ", fieldsz=" << fieldsz << ", buffsz=" << buffsz
                  << ", input=" << str::to_string(input) << std::endl;

        const auto set = [&](auto &oldval, auto &newval) {
            std::memcpy(&oldval, input.data() + field_offset, fieldsz);
            using T = std::decay_t<decltype(newval)>;
            newval = oldval + rd::pick<T>(0, std::numeric_limits<T>::max() - 1);
        };

        std::uint16_t oldu16 = 0, newu16 = 0;
        std::uint32_t oldu32 = 0, newu32 = 0;
        std::uint64_t oldu64 = 0, newu64 = 0;

        // initial checksum
        inetcksum_ctx ctx1, ctx2, ctx3;
        inet::update_checksum(ctx1, input.data(), input.size());
        auto cksum = inet::get_checksum(ctx1);
        auto buff2 = input;

        std::cerr << "initial checksum: " << std::hex << cksum << std::endl;
        std::cerr << "----- incremental update begin\n";
        // now update field;
        // NOTE: we also populate the 'change' vector so we run
        // a check for the buffer-based API as well.
        if (fieldsz == 2) {
            set(oldu16, newu16);
            std::memcpy(buff2.data() + field_offset, &newu16, 2);
            inet::update_checksum<false>(ctx1, oldu16, newu16);
            // buffer API
            change.resize(2);
            std::memcpy(change.data(), &newu16, 2);
        } else if (fieldsz == 4) {
            set(oldu32, newu32);
            std::memcpy(buff2.data() + field_offset, &newu32, 4);
            inet::update_checksum<false>(ctx1, oldu32, newu32);
            // buffer API
            change.resize(4);
            std::memcpy(change.data(), &newu32, 4);
        } else {
            set(oldu64, newu64);
            std::memcpy(buff2.data() + field_offset, &newu64, 8);
            inet::update_checksum<false>(ctx1, oldu64, newu64);
            // buffer API
            change.resize(8);
            std::memcpy(change.data(), &newu64, 8);
        }
        std::cerr << "----- incremental update end\n";

        // the checksum obtained via incremental update should be
        // the same as if we computed the checksum from scratch
        inet::update_checksum(ctx2, buff2.data(), buff2.size());
        const auto cksum2 = inet::get_checksum(ctx2);
        const auto inccksum = inet::get_checksum(ctx1);
        std::cerr << "inccksum=" << std::hex << inccksum << std::endl;
        std::cerr << "cksum2=" << std::hex << cksum2 << std::endl;
        REQUIRE(cksum2 == inet::get_checksum(ctx1));

        // also use the buffer API to make sure we get the same result
        check_buffer_based_incremental_update(input, change, field_offset);
        // first calculate the initial checksum, before
        // applying incremental change
        inet::update_checksum(ctx3, input.data(), input.size());
        // now apply change
        inet::update_checksum(ctx3,
                              input.data(),
                              input.size(),
                              field_offset,
                              fieldsz,
                              change.data());
        const auto cksum3 = inet::get_checksum(ctx3);
        std::cerr << "------ buffer-based API CHECK end\n";
        REQUIRE(cksum3 == cksum2);

        // now if we undo the change, we should get
        // back the previous checksum. NOTE: if new val
        // was randomly picked to be equal to newval,
        // tehn no change ever was made! -- this is a good test too.
        if (fieldsz == 2) {
            inet::update_checksum<false>(ctx1, newu16, oldu16);
        } else if (fieldsz == 4) {
            inet::update_checksum<false>(ctx1, newu32, oldu32);
        } else {
            inet::update_checksum<false>(ctx1, newu64, oldu64);
        }

        REQUIRE(inet::get_checksum(ctx1) == cksum);

        //------------------------------------------
        // now also do a completely separate check for the buffer-based
        // API alone; particularly since here we allow for lengths
        // that are not a multiple of sizeof(u16), that are unaligned,
        // that can be shorted than sizeof(u16) etc.
        std::cerr << "------------------ CHECK with change_offset=" << std::dec
                  << change_offset << " change_len=" << change_len
                  << " buffersz=" << buffsz << std::endl;
        change.resize(change_len);
        for (auto &byte : change) {
            byte = rd::pick<std::uint8_t>(0, 255);
        }
        check_buffer_based_incremental_update(input, change, change_offset);
        //------------------------------------------
    }
}
#endif

int main(int argc, char **argv) {
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
