#include "tarp/bits.hxx"
#include <tarp/hash/checksum/sha256.hxx>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tarp {
namespace hash {
namespace checksum {

namespace detail {
std::array<sha256_ctx::word_t, 64> sha256_constants {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
  0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
  0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
  0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
  0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
  0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

void process_last(sha256_ctx &ctx) {
    using context_t = std::decay_t<decltype(ctx)>;

    //--------------------
    // 5.1:
    // The purpose of this padding is to ensure that the padded message
    // is a multiple of 512 or 1024 bits, depending on the algorithm.
    // Padding can be inserted before hash computation begins on a message,
    // or at any other time during the hash computation prior to processing
    // the block(s) that will contain the padding.
    // [...]
    // Suppose that the length of the message, M, is l bits. Append
    // the bit “1” to the end of the message, followed by k zero bits,
    // where k is the smallest, non-negative solution to the equation:
    //   l+1+k = 448 mod 512
    //--------------------
    // => therefore:
    //  k = (n*448) - l - 1
    // For k to be non-negative, then we have two cases -- since
    // we only pad the very last chunk of an input IFF its size
    // is smaller than 512 bits (64 bytes):
    // 1)  l <= 448-1 => l <= 447
    //   -> we pad by 448 - l - 1        (therefore n=1)
    // 2) 448 <= l <= 448*2
    //   -> we pad by (448*2) - l -1     (therefore n=2)
    // NOTE the edge case: if the input chunk is 448 bits (56 bytes),
    // then we fall into the second case. The reason is that at the
    // end of the block, SHA256 requires exactly 8 bytes (64 bits)
    // for storing the entire message length. And while 512-448
    // leaves exactly 64 bits, SHA256 further requires that a '1'
    // bit is first added after the message, hence only leaving
    // 63 bits and therefore requiring padding (=> n=2).

    // NOTE: while the official algorithm works with bitstrings,
    // hence even with lengths that are not byte multiples, the
    // implementation here will only allow for byte multiples
    // since in practice this is the common case.

    // Since each chunk is processed in fixed blocks, then
    // the last chunk is the only one that may need padding,
    // and it will by definition be <= block size.
    const std::size_t chunklen = ctx.msglen % context_t::BLOCK_SIZE_BYTES;
    auto offset = chunklen;

    // Zero everything after the message
    std::memset(&ctx.buff[offset], 0, ctx.buff.size() - offset);

    // append bit '1' after the message;
    // here we accomplish this by setting the
    // most significant bit in the first byte
    // after the message.
    ctx.buff[offset] = 0x80;

    // find the end of the padding;
    offset = chunklen < 56 ? 56 : 64 + 56;

    // now append the message length _in bits_ as a u64 (big-endian)
    std::uint64_t len = bits::to_nbo(std::uint64_t(ctx.msglen) << 3);
    std::memcpy(ctx.buff.data() + offset, &len, sizeof(std::uint64_t));

    //std::cerr << "process block1\n";
    process_block(ctx, ctx.buff.data());
    if (offset > 64) {
        //std::cerr << "process block2\n";
        process_block(ctx, ctx.buff.data() + 64);
    }
    //std::cerr << "process block2 end\n";
}

void process_block(sha256_ctx &ctx, const std::uint8_t *data) {
    //std::cerr << "#1\n";
    using namespace detail;
    using word_t = sha256_ctx::word_t;
    using C = sha256_ctx;

    // initialize working variables based on previous
    // hash words.
    word_t a = ctx.hash[0];
    word_t b = ctx.hash[1];
    word_t c = ctx.hash[2];
    word_t d = ctx.hash[3];
    word_t e = ctx.hash[4];
    word_t f = ctx.hash[5];
    word_t g = ctx.hash[6];
    word_t h = ctx.hash[7];

    // temporary words
    word_t T1 = 0, T2 = 0;

    // prepare the message schedule
    std::array<word_t, 64> sched;

    // first 16 words are the block itself
    std::memcpy(sched.data(), data, C::WORD_SIZE * 16);
    for (auto &w : sched) {
        w = bits::to_hbo(w);
    }

    using bits::rotate_left;
    using bits::rotate_right;

    //std::cerr << "#2\n";

    // operations given on p10.
    constexpr auto sum0 = [](word_t x) {
        return rotate_right<2>(x) ^ rotate_right<13>(x) ^ rotate_right<22>(x);
    };
    constexpr auto sum1 = [](word_t x) {
        return rotate_right<6>(x) ^ rotate_right<11>(x) ^ rotate_right<25>(x);
    };
    constexpr auto op0 = [](word_t x) {
        return rotate_right<7>(x) ^ rotate_right<18>(x) ^ (x >> 3);
    };
    constexpr auto op1 = [](word_t x) {
        return rotate_right<17>(x) ^ rotate_right<19>(x) ^ (x >> 10);
    };

    //std::cerr << "#3\n";

    // rest of the words [17..64] are computed
    for (unsigned i = 16; i < sched.size(); ++i) {
        sched[i] =
          op1(sched[i - 2]) + sched[i - 7] + op0(sched[i - 15]) + sched[i - 16];
    }

    //std::cerr << "#4\n";

    for (unsigned i = 0; i < sched.size(); ++i) {
        //std::cerr << "iteration: " << i << std::endl;
        T1 = h + sum1(e) + choose(e, f, g) + sha256_constants[i] + sched[i];
        T2 = sum0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    //std::cerr << "#5\n";

    // compute the i-th intermediate hash value
    ctx.hash[0] = ctx.hash[0] + a;
    ctx.hash[1] = ctx.hash[1] + b;
    ctx.hash[2] = ctx.hash[2] + c;
    ctx.hash[3] = ctx.hash[3] + d;
    ctx.hash[4] = ctx.hash[4] + e;
    ctx.hash[5] = ctx.hash[5] + f;
    ctx.hash[6] = ctx.hash[6] + g;
    ctx.hash[7] = ctx.hash[7] + h;

    //std::cerr << "#6\n";
}


}  // namespace detail

void process(sha256_ctx &ctx,
             const std::uint8_t *data,
             std::size_t len,
             bool last_chunk) {
    using context_t = std::decay_t<decltype(ctx)>;
    using namespace detail;

    while (len > 0) {
        //std::cerr << "in loop\n";
        const std::size_t n =
          std::min(context_t::BLOCK_SIZE_BYTES - ctx.offset, len);
        std::memcpy(ctx.buff.data() + ctx.offset, data, n);

        ctx.msglen += n;
        ctx.offset += n;
        data += n;
        len -= n;

        if (ctx.offset == context_t::BLOCK_SIZE_BYTES) {
            //std::cerr << "before call to process_bloclk\n";
            process_block(ctx, ctx.buff.data());
            //std::cerr << "after call to process_bloclk\n";
            ctx.offset = 0;
        }
    }

    if (last_chunk) {
        //std::cerr << "before call to process_last\n";
        process_last(ctx);
        //std::cerr << "after call to process_last\n";
    }
}

std::string get_hashstring(sha256_ctx &ctx) {
    std::ostringstream ss;
    for (auto &word : ctx.hash) {
        ss << std::setfill('0') << std::setw(ctx.WORD_SIZE * 2) << std::hex
           << word;
    }
    return ss.str();
}

std::string sha256sum(const std::uint8_t *data, std::size_t len) {
    sha256_ctx ctx;
    process(ctx, data, len, true);
    return get_hashstring(ctx);
}


}  // namespace checksum
}  // namespace hash
}  // namespace tarp
