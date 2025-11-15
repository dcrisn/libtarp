#pragma once

#include <tarp/bits.hxx>
#include <tarp/math.hxx>

#include <cstdint>
#include <cstring>

// -------------------------------------------------
// Adler32 is a modification of Fletcher32.
// They both maintain internal 16-bit accumulators.
// The modulus used by Adler32 is different ( a prime number);
// and, while the Fletcher checksum is word-oriented
// (Fletcher32 uses 16-bit words, Fletcher64 uses 32-bit words),
// the adler32 checksum deals in bytes.
// This makes makes it inherently endianness-agnostic.
// It also makes it very convenient for use as a rolling checksum,
// where the window slides one byte at a time.
// It's possible to make the Fletcher checksum into a rolling
// checksum as well; byte significance must be tracked at
// all times though, based on where the new byte would fit within
// a word; this is possible (e.g. via the ctx.joint approach
// in fletcher.hxx), but is much more inconvenient than working
// with a byte-oriented checksum like adler32.
// 'Incremental checksum updates' (see inet.hxx) are also
// made very convenient with Adler32.
// -------------------------------------------------
//
// References:
// https://en.wikipedia.org/wiki/Adler-32
// https://github.com/madler/zlib/blob/develop/adler32.c
//
// Note: Adler32 is written by Mark adler (see zlib above).
// The following merely translates or adjusts the implementation.
namespace tarp {
namespace hash {
namespace checksum {
namespace adler {

// largest prime number < 2^16-1 */
static inline constexpr std::uint32_t MOD_ADLER = 65521;

// This is the maximum number of additions that can be performed
// on the internal sums before overflow would occur;
// in other words, this is the number of additions that can
// safely be performed _without_ performing a modulo reduction.
//
// In zlib source code this is 'NMAX'. This is used for the same
// optimization commonly applied to fletcher checksums (see
// fletcher.hxx): delaying the relatively expensive modulo
// operation for as long as it's safe and hence performing
// as many additions as possible without modulo.
// However, the way this constant is derived is different from
// fletcher, because:
// - the adler32 checksum's 'sum1' internal accumulator starts
//   at 1, not 0
// - the internal accumulators are reduced modulo MOD_ADLER, as
//   opposed to modulo maxw for fletcher
// - subtraction is performed instead of modulo on a given
//   internal accumulator A when the value of A is >= MOD_ADLER
//   and < MOD_ADLER*2; therefore when a call to process a buffer
//   arrives, the value in a given accumulator could be anywhere
//   between [0, MOD_ADLER-1].  NOTE: this optimization is also
//   in fact known from fletcher checksums (see adler32.md fmi).
// See find_max_num_additions_without_overflow in fletcher.hxx to
// see how it's derived for fletcher checksums.
// See src/hash/adler32.md for a full derivation of the value
// below for adler32 and further comments.
static inline constexpr std::uint32_t MAX_ADD = 5552;

// When x in [y, 2*y), we can do
// x -= y instead of x %= y.
// The two are equivalent in this specific case.
// This is one of the optimizations that are well known
// from the flethcer checksum.
static inline std::uint32_t modsub(std::uint32_t x) {
    return (x < MOD_ADLER) ? x : x - MOD_ADLER;
}

struct adler32_ctx {
    using acc = std::uint32_t;
    using checksum_t = std::uint32_t;
    using word = std::uint8_t;

    static inline constexpr acc modulus = MOD_ADLER;
    static inline constexpr acc MAX_ADD = adler::MAX_ADD;

    // internal accumulators
    acc sum1 = 1;
    acc sum2 = 0;
};

void update(adler32_ctx &ctx, const std::uint8_t *buff, std::size_t bufflen);

inline std::uint32_t get_checksum(const adler32_ctx &ctx) {
    return (ctx.sum2 << 16) | ctx.sum1;
}

// Roll the checksum forward by 1.
// Assuming a window of n bytes is kept, sliding the window forward
// by 1 causes one byte to drop out of the window on the left side
// and a new byte to enter on the right side.
// The contribution of the byte droppping out must be subtracted,
// and the contribution of the byte entering must be added.
// Hence, subtract the contribution of the byte 'out', and
// add the contribution of byte 'in'.
//
// -----------------------
// NOTE:
// while not implemented here, incremental updates (see /inet.hxx)
// would seem to also be feasible using similar logic to the
// one used for rolling; however in that case, a function would
// be needed that can find the contribution of a byte given the
// byte value and its position in the buffer (window); and
// instead of a byte leaving at one end and a byte entering
// at the other, one byte would be replacing the other at
// its exact position.
// -----------------------
inline void roll(adler32_ctx &ctx,
                 std::uint32_t window_size,
                 std::uint8_t in,
                 std::uint8_t out) {
    // std::cerr << "BYTE IN: " << static_cast<unsigned>(in)
    //           << ", BYTE OUT: " << static_cast<unsigned>(out) << std::endl;

    // A byte contributes to sum1 only once, upon entering
    // the window:
    //    sum  += in
    // And hence to remove its contribution:
    //    sum1 -= out
    // We must be careful of overflow  -- or rather _underflow_ -- here;
    // since u8::max < MOD_ADLER, then sum1+MOD_ADLER+in > 'out', and
    // we can safely subtract 'out'. The addition of MOD_ADLER is then
    // simply to prevent underflow and disappears when
    // the % MOD_ADLER is performed.
    ctx.sum1 = (ctx.sum1 + MOD_ADLER + in - out) % MOD_ADLER;

    // The contribution of a byte to sum2 is greater, because
    // after n steps sum1 will've been added to sum2 n times;
    // and each time sum1 _contains_ 'in' -- hence 'in' is
    // added to sum2 n times.
    // Therefore the contribution of a byte exiting the window
    // will've been n*'out', where n is the number of steps i.e.
    // the width of the window in bytes.
    // Again here we must guard against overflow in this intermediate
    // result; using a u64 makes overflow impossible, since window_size
    // is a u32, and u32::max * u8::max < u64::max.
    std::uint64_t old_contrib =
      static_cast<std::uint64_t>(out) * window_size % MOD_ADLER;

    // The usual adler32 update formula is:
    //   ctx.sum2 += ctx.sum1
    // Obviously we must use the same here; ctx.sum1 has already taken
    // in the contribution of 'in', hence sum2 is now getting it as well.
    // Subtracting the old contribution can result in underflow;
    // however because we reduced old_contrib % MOD_ADLER, the value
    // of old_contrib is at most MOD_ADLER-1. Hence if we add MOD_ADLER,
    // (old_contrib+1) can be subtracted safely without underflow.
    // The -1 that is further subtracted is due to the fact that
    // adler32's sum1 internal sum is initialized to 1, not 0;
    // hence when the contribution of a new byte enters sum2
    // (via sum2 += sum1), the '1' is also implicitly is addded in,
    // and hence it must be subtracted when the cotribution of the
    // byte is subtracted.
    ctx.sum2 = (ctx.sum2 + ctx.sum1 + MOD_ADLER - old_contrib - 1) % MOD_ADLER;
}
}  // namespace adler

inline std::uint32_t adler32(const std::uint8_t *data, std::size_t len) {
    adler::adler32_ctx ctx;
    update(ctx, data, len);
    return adler::get_checksum(ctx);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
