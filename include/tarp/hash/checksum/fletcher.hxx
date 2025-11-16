#pragma once

#include <tarp/bits.hxx>
#include <tarp/math.hxx>

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace tarp {
namespace hash {
namespace checksum {
using bits::byteswap;

// Wikipedia [nov 2025, https://en.wikipedia.org/wiki/Fletcher%27s_checksum]:
// > The Fletcher checksum cannot distinguish between blocks of all 0 bits
// > and blocks of all 1 bits. For example, if a 16-bit block in the data
// > word changes from 0x0000 to 0xFFFF, the Fletcher-32 checksum remains
// > the same. This also means a sequence of all 00 bytes has the same
// > checksum as a sequence (of the same size) of all FF bytes.

namespace fletcher {
namespace impl {
template<typename word, typename T>
inline constexpr T find_max_num_additions_without_overflow();

}  // namespace impl
}  // namespace fletcher

//

// State for block, incremental, and rolling updates.
template<typename word_type, typename accumulator_type>
struct fletcher_ctx {
    // accumulator type
    using acc = accumulator_type;
    using word = word_type;

    // the type of the checksum result
    using checksum_t = std::conditional_t<
      std::is_same_v<word, std::uint8_t>,
      std::uint16_t,
      std::conditional_t<std::is_same_v<word, std::uint16_t>,
                         std::uint32_t,
                         std::conditional_t<std::is_same_v<word, std::uint32_t>,
                                            std::uint64_t,
                                            void>>>;

    static inline constexpr accumulator_type MAX_ADD = fletcher::impl::
      find_max_num_additions_without_overflow<word_type, accumulator_type>();

    static inline constexpr accumulator_type modulus =
      std::numeric_limits<word>::max();

    // to check against MAX_ADD;
    // we perform a modulo when count == MAX_ADD,
    // in order to prevent overflow.
    std::uint32_t cnt = 0;

    // to allow processing of buffers that are not {word_type}-aligned;
    // see inet.hxx for more details (the same technique is applied).
    // Here 'deficit' replaces 'truncated', since we can have more than
    // 1 missing byte.
    std::uint8_t joint[sizeof(word_type)];
    std::uint8_t deficit = 0;

    acc sum1 = 0;
    acc sum2 = 0;
};

// NOTE: this requires some familiarity with the algorithm
// and its optimizations to understand. For example, fletcher16
// actually uses two uint8_t running sums that are ultimately
// combined into a uint16_t. However, while these sums, post
// any addition operation, will be < max(uint16), DURING an
// addition using a uint8_t incurs the risk of overflow;
// therefore these intermediate sums are normally stored as
// uint16s in order to prevent overflow.
//
// A common optimization is to use use even wider types e.g.
// u32 for these in order to increase the number of
// additions that can be carried out without risk of
// overflow, thereby delaying the need for the comparatively
// expensive modulo operation. Using a u64 for fletcher16,
// for example, would then reduce the number of modulo operations
// to the very bare minimum.
//
// Generically, if:
// - the input is read as a sequence of X, where X is
//   u8 for fletcher16, u16 for fletcher32, and u32 for fletcher64
// AND
// - we use a type T for the 2 sums (with T as wide or wider than X)
//
// then the maximum number n of additions we can perform without needing
// to do a modulo % on them (with no risk of overflow) is given by:
//     n < sqrt( (max(T) * 2)/max(X))
// where max(C) is std::numeric_limits<C>(), for C in {T, X}.
//
// The wikipedia article [https://en.wikipedia.org/wiki/Fletcher's_checksum]
// finds this value for fletcher16 like this:
// 	   > /*  Found by solving for c1 overflow: */
//	   > /* n > 0 and n * (n+1) / 2 * (2^8-1) < (2^32-1). */
//	   > [...] blocklen = 5802
//
// Here blocklen gives the max_num_additions_without_modulo.
// See the find_max_num_additions_without_overflow function
// below for an explanation.
//
// ctx.run counts the number of additions we've done without
// performing a modulo.
using fletcher16_ctx = fletcher_ctx<std::uint8_t, std::uint32_t>;
using fletcher32_ctx = fletcher_ctx<std::uint16_t, std::uint64_t>;
using fletcher64_ctx = fletcher_ctx<std::uint32_t, std::uint64_t>;

namespace fletcher {

namespace impl {

// Find the maximum number of additions (inclusive)
// that can be performed before overflow occurs.
template<typename word, typename T>
inline constexpr T find_max_num_additions_without_overflow() {
    static_assert(std::is_unsigned_v<T>);
    static_assert(std::is_unsigned_v<word>);

    // since T is the type of the accumulator, this is
    // the highest T value possible. If we exceed this,
    // we wrap around (overflow).
    constexpr auto maxT = std::numeric_limits<T>::max();

    // Each word has size sizeof(word), and hence
    // n=sizeof(word)<<3 bits, and hence a maximum
    // value of (2^n)-1. This is the maximum value
    // each input word can have.
    constexpr auto maxw = std::numeric_limits<word>::max();

    // in the worst case, each input word has a value
    // equal to maxw. Hence simplistically, the
    // maximum number of times we can safely perform
    // addition without overflow is the number of times
    // we can add up maxw while staying <= maxT:
    // maxT/maxw.
    //
    // However, the fletcher checksum uses _two_ internal
    // accumulators:
    //     ctx.sum1 = (ctx.sum1 + value);
    //     ctx.sum2 = (ctx.sum2 + ctx.sum1);
    //
    // Hence, after n additions, the worst case for sum1 is
    //    sum1 = n * maxw
    //
    // But for sum2 it's more complicated, since it accumulates
    // sum1 at each step. After n iterations:
    //     sum2 = sum1  // i.e. sum2 = maxw
    //     sum2 = sum1 + (sum1*2) = sum1 * 3
    //     sum2 = sum1 + (sum1*2) + (sum1 * 3) = sum1 * 6
    //     ....
    //     sum2 = sum1 * (1 + 2 + 3 + ... + n)
    //
    // The sum   1 + 2 + 3 + ... + n is the triangular number formula:
    //     n * (n+1) / 2
    //
    // So the worst case for sum2 after n additions is:
    //     sum2 = maxw * (n * (n+1) / 2)
    //
    // To avoid overflow in a T accumulator:
    //    maxw * (n * (n+1) / 2) <= maxT
    //    n * (n+1) / 2 <= maxT/maxw
    //    n * (n+1) <= (2 * maxT)/maxw
    //    n^2 + n <= (2 * maxT)/maxw
    //    ~n <= sqrt((2*maxT)/maxw)
    constexpr T val = tarp::math::sqrt((maxT / maxw) << 1);
    return val;
}

// Update the sums with the given value, while taking account of
// the optimization discussed above. We perform a modulo when we've
// reached the max_num_additions_without_modulo limit.
template<typename context_t>
static inline void update(context_t &ctx, typename context_t::word value) {
    if (ctx.cnt == context_t::MAX_ADD) {
        ctx.sum1 %= context_t::modulus;
        ctx.sum2 %= context_t::modulus;
        ctx.cnt = 0;
    }

    ctx.sum1 = (ctx.sum1 + value);
    ctx.sum2 = (ctx.sum2 + ctx.sum1);

    // std::cerr << "=> adding value=" << static_cast<unsigned>(value)
    //           << " sum1=" << ctx.sum1 << ", sum2=" << ctx.sum2 << std::endl;
    ctx.cnt++;
}

template<typename context_t>
inline void subtract(context_t &ctx, typename context_t::word value) {
    constexpr auto M = context_t::modulus;

    // 2*M
    static constexpr decltype(M) twoM = M << 1;

    // The following perform subtraction instead of modulo
    // where possible; this will commonly be the case if
    // subtract() is called continuously (for example,
    // to process byte-at-a-time, even where the word
    // is u16 (fletcher32) or u32 (fletcher64));
    // in fact, because this function is only ever called
    // to subtract the contribution of a _partial_ word
    // (i.e. we by definition do _not_ have a full word
    // when this function is called), then the % will
    // never be used! (but leaving it in for safety).
    // ---------
    if (ctx.sum1 >= M) {
        if (ctx.sum1 < twoM) ctx.sum1 -= M;
        else ctx.sum1 %= M;
    }

    if (ctx.sum2 >= M) {
        if (ctx.sum2 < twoM) ctx.sum2 -= M;
        else ctx.sum2 %= M;
    }
    // ---------

    // undo the last sum1+sum2
    if (ctx.sum2 >= ctx.sum1) {
        ctx.sum2 -= ctx.sum1;
    } else {
        ctx.sum2 = ctx.sum2 + M - ctx.sum1;
        if (ctx.sum2 >= M) ctx.sum2 -= M;
    }

    // undo the last 'sum1 += value'
    if (ctx.sum1 > value) {
        ctx.sum1 -= value;
    } else {
        // else it means sum1 is below word::max, hence below
        // the modulus; so if we add a whole modulus to it,
        // we'll be between (modulus, 2*modulus), and in
        // that specific case subtraction is the same as modulo.
        ctx.sum1 = ctx.sum1 + M - value;
        if (ctx.sum1 >= M) ctx.sum1 -= M;
    }

    // when we reach here sum1 and sum2 are < M
    // (we will've performed a % or equivalent reduction);
    // hence we can reset the counter to 0.
    ctx.cnt = 0;
}

// Calculate and return the fletcherX checksum from the
// 2 internal sums.
template<typename context_t>
typename context_t::checksum_t get_checksum(context_t &ctx) {
    using word = typename context_t::word;
    using checksum_t = typename context_t::checksum_t;

    ctx.sum1 %= context_t::modulus;
    ctx.sum2 %= context_t::modulus;
    ctx.cnt = 0;

    // std::cerr << "reduction: ctx.sum1=" << ctx.sum1 << ", ctx.sum2=" <<
    // ctx.sum2
    //           << " modulus=" <<
    //           static_cast<std::uint64_t>(context_t::modulus)
    //           << std::endl;
    //
    checksum_t checksum = ctx.sum1 | (ctx.sum2 << tarp::bits::width_v<word>);
    return checksum;
}

// Process a block of data and update the current checksum
// value based on it. len need not be a multiple of sizeof(T).
template<typename context_t>
void update(context_t &ctx, const std::uint8_t *buff, std::size_t bufflen) {
    using word_t = typename context_t::word;

    if (ctx.deficit > 0 && bufflen > 0) {
        word_t oldval = 0, newval = 0;
        std::size_t l = sizeof(word_t) - ctx.deficit;

        // if still not enough bytes to form a word, then remain
        // in deficit state, but consume these new bytes.
        if (bufflen < ctx.deficit) {
            std::memcpy(&oldval, ctx.joint, l);
            std::memcpy(ctx.joint + l, buff, bufflen);
            ctx.deficit -= bufflen;
            std::memcpy(&newval, ctx.joint, sizeof(word_t) - ctx.deficit);

            // std::cerr << "--- deficit case 1\n";
            //  subtract old contribution
            subtract(ctx, oldval);
            // add new contribution
            update(ctx, newval);
            return;
        }

        // else we have enough bytes to clear the deficit and form
        // a full word
        std::memcpy(&oldval, ctx.joint, l);
        std::memcpy(ctx.joint + l, buff, ctx.deficit);
        std::memcpy(&newval, ctx.joint, sizeof(word_t));

        // std::cerr << "--- deficit case 2\n";
        subtract(ctx, oldval);
        update(ctx, newval);

        buff += ctx.deficit;
        bufflen -= ctx.deficit;
        ctx.deficit = 0;
        if (bufflen == 0) {
            return;
        }
    }

    // process words
    const std::size_t nwords = bufflen / sizeof(word_t);
    for (std::size_t i = 0; i < nwords; ++i) {
        word_t val;
        std::memcpy(&val, buff, sizeof(val));
        // std::cerr << "--- main case \n";
        update(ctx, val);
        bufflen -= sizeof(word_t);
        buff += sizeof(word_t);
    }

    if (bufflen == 0) {
        return;
    }

    // trailing bytes (< sizeof(word)) ?
    word_t val = 0;
    std::memcpy(&val, buff, bufflen);

    // std::cerr << "--- last trailing case \n";
    update(ctx, val);

    std::memcpy(ctx.joint, buff, bufflen);
    ctx.deficit = sizeof(word_t) - bufflen;
}

// TODO: implement incremental checksum update, following the example
// in the inetcksum.cxx. The difference here is that the word
// is bigger and differs between fletcher16,32,64, and hence
// it's a bit more work and testing.

}  // namespace impl

namespace fletcher16 {
inline void
update(fletcher16_ctx &ctx, const std::uint8_t *buff, std::size_t bufflen) {
    impl::update(ctx, buff, bufflen);
}

inline std::uint16_t get_checksum(fletcher16_ctx &ctx) {
    return impl::get_checksum(ctx);
}
}  // namespace fletcher16

namespace fletcher32 {

inline void
update(fletcher32_ctx &ctx, const std::uint8_t *buff, std::size_t bufflen) {
    impl::update(ctx, buff, bufflen);
}

inline std::uint32_t get_checksum(fletcher32_ctx &ctx) {
    return impl::get_checksum(ctx);
}
}  // namespace fletcher32

namespace fletcher64 {
inline void
update(fletcher64_ctx &ctx, const std::uint8_t *buff, std::size_t bufflen) {
    return impl::update(ctx, buff, bufflen);
}

inline std::uint64_t get_checksum(fletcher64_ctx &ctx) {
    return impl::get_checksum(ctx);
}
}  // namespace fletcher64
}  // namespace fletcher

// Convenience functions for the common case where all data is available
// upfront. They all produce a checksum on a single call.
// To process data in blocks, call the process_block in the appropriate
// namespace (fletcher16::, fletcher32::, fletcher64::) with last_block=false
// for all but the last block; then call get_checksum at the end to compute the
// actual checksum.
//
// If network_byte_order=true, then data is expected to be in
// network byte order and the implementation will internally
// convert the bytes to host-byte-order as appropriate for
// internal calculations.
inline std::uint16_t fletcher16(const std::uint8_t *data, std::size_t len) {
    fletcher16_ctx ctx;
    fletcher::impl::update(ctx, data, len);
    return fletcher::fletcher16::get_checksum(ctx);
}

inline std::uint32_t fletcher32(const std::uint8_t *data, std::size_t len) {
    fletcher32_ctx ctx;
    fletcher::impl::update(ctx, data, len);
    return fletcher::fletcher32::get_checksum(ctx);
}

inline std::uint64_t fletcher64(const std::uint8_t *data, std::size_t len) {
    fletcher64_ctx ctx;
    fletcher::impl::update(ctx, data, len);
    return fletcher::fletcher64::get_checksum(ctx);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
