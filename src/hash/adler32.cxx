#include <tarp/hash/checksum/adler32.hxx>
#include <tarp/string_utils.hxx>

namespace tarp {
namespace hash {
namespace checksum {
namespace adler {

#define ADD1(ctx, buff, pos)   \
    {                          \
        ctx.sum1 += buff[pos]; \
        ctx.sum2 += ctx.sum1;  \
    }

#define ADD16(ctx, buff) \
    ADD1(ctx, buff, 0);  \
    ADD1(ctx, buff, 1);  \
    ADD1(ctx, buff, 2);  \
    ADD1(ctx, buff, 3);  \
    ADD1(ctx, buff, 4);  \
    ADD1(ctx, buff, 5);  \
    ADD1(ctx, buff, 6);  \
    ADD1(ctx, buff, 7);  \
    ADD1(ctx, buff, 8);  \
    ADD1(ctx, buff, 9);  \
    ADD1(ctx, buff, 10); \
    ADD1(ctx, buff, 11); \
    ADD1(ctx, buff, 12); \
    ADD1(ctx, buff, 13); \
    ADD1(ctx, buff, 14); \
    ADD1(ctx, buff, 15);

// This function is taken from
// https://github.com/madler/zlib/blob/develop/adler32.c#L10C14-L10C20.
// Tiny adjusments made:
// - NMAX renamed to MAX_ADD, which is discussed at length in adler32.hxx
// and derived in adler32.md.
// - changed API slightly to match other checksums/crcs libtarp has
// (use of ctx, etc).
// Structure kept as in the zlib code as it is highy optimized. Unclear
// why 16 is chosen for the unrolling factor as opposed to e.g. 32;
// probably the sweet spot for instruction parallelism.
void update(adler32_ctx &ctx, const std::uint8_t *buff, std::size_t len) {
    // NOTE: no truncation, MAX_ADD is a multiple of 16
    static_assert((MAX_ADD & 0xf) == 0);
    constexpr std::uint32_t BULKSZ = MAX_ADD >> 4;

    //std::cerr << "*** adler32 called with bufflen=" << len << std::endl;

    // byte-at-a-time: most common expected case.
    // use subtraction-instead-of-modulo optimization;
    if (len == 1) {
        //std::cerr << "processing byte: " << static_cast<unsigned>(buff[0])
        //          << std::endl;
        ctx.sum1 += buff[0];
        ctx.sum1 = modsub(ctx.sum1);
        ctx.sum2 += ctx.sum1;
        ctx.sum2 = modsub(ctx.sum2);
        return;
    }

    if (len == 0) {
        return;
    }

    if (len < 16) {
        while (len--) {
            //std::cerr << "processing byte: " << static_cast<unsigned>(*buff)
            //          << std::endl;
            ctx.sum1 += *buff++;
            ctx.sum2 += ctx.sum1;
        }
        ctx.sum1 = modsub(ctx.sum1);
        ctx.sum2 %= MOD_ADLER;
        return;
    }

    // perform up to MAX_ADD additions without modulo;
    // NOTE when we start iterating here, the values
    // for the internal sums can already be (at most)
    // MOD_ADLER-1, due to the fast byte-by-byte
    // processing case above. However we still will
    // not overflow, because MAX_ADD is derived such
    // that it accounts for this (see adler32.md).
    while (len >= MAX_ADD) {
        len -= MAX_ADD;

        // perform 16 unrolled additions each pass.
        for (std::uint32_t i = 0; i < BULKSZ; ++i) {
            ADD16(ctx, buff); /* 16 sums unrolled */
            //std::cerr << "processing bytes: "
            //          << utils::str::to_string(buff, 16, "[", "]") << std::endl;
            buff += 16;
        }

        ctx.sum1 %= MOD_ADLER;
        ctx.sum2 %= MOD_ADLER;
    }

    // leftover bytes < MAX_ADD
    if (len > 0) { /* avoid modulos if none remaining */
        while (len >= 16) {
            len -= 16;
            ADD16(ctx, buff);
            //std::cerr << "processing bytes: "
            //          << utils::str::to_string(buff, 16, "[", "]") << std::endl;
            buff += 16;
        }
        while (len--) {
            //std::cerr << "processing byte: " << static_cast<unsigned>(*buff)
            //          << std::endl;
            ctx.sum1 += *buff++;
            ctx.sum2 += ctx.sum1;
        }
        ctx.sum1 %= MOD_ADLER;
        ctx.sum2 %= MOD_ADLER;
    }
}

}  // namespace adler
}  // namespace checksum
}  // namespace hash
}  // namespace tarp
