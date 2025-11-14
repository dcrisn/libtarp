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

    // to check against MAX_ADD;
    // we perform a modulo when count == MAX_ADD,
    // in order to prevent overflow.
    std::uint32_t cnt = 0;

    // to allow processing of buffers that are not {word_type}-aligned;
    // see inet.hxx for more details (the same technique is applied).
    std::uint8_t joint[sizeof(word_type)];
    bool truncated = false;

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

// get the modulus used for a fletcher checksum that reads
// the input as a sequence of Ts. For example, for fletcher16
// we use u16::max() = (2^16 - 1) as the modulus.
// I.e. this is the maximum word value.
template<typename T>
static inline constexpr T modulus_v = std::numeric_limits<T>::max();

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
    // The sum 1 + 2 + 3 + ... + n is the triangular number formula:
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
template<typename context_t, typename T>
static inline void update(context_t &ctx, T value, T modulus) {
    if (ctx.cnt == context_t::MAX_ADD) {
        ctx.sum1 %= modulus;
        ctx.sum2 %= modulus;
        ctx.cnt = 0;
    }

    ctx.sum1 = (ctx.sum1 + value);
    ctx.sum2 = (ctx.sum2 + ctx.sum1);
    ctx.cnt++;
}

// Calculate and return the fletcherX checksum from the
// 2 internal sums.
template<typename context_t>
typename context_t::word get_checksum(context_t &ctx) {
    using word = typename context_t::word;
    using T = typename context_t::acc;
    using checksum_t = typename context_t::checksum_t;

    ctx.sum1 %= modulus_v<T>;
    ctx.sum2 %= modulus_v<T>;
    ctx.run = 0;

    checksum_t checksum = ctx.sum1 | (ctx.sum2 << tarp::bits::width_v<T>);
    return checksum;
}

// Process a block of data and update the current checksum
// value based on it. len need not be a multiple of sizeof(T).
//
// NOTE: nbo = network byte order.
template<typename context_t, bool nbo>
void process_chunks(context_t &ctx, const std::uint8_t *buff, std::size_t len) {
    constexpr auto hbo = [](auto val) {
        if constexpr (nbo) {
            return bits::to_hbo(val);
        }
        return val;
    };

    if (ctx.truncated && bufflen > 0) {
        // get the previous contribution so we
        // can undo it (subtract it)
        std::uint16_t prev = 0;
        std::memcpy(&prev, ctx.joint, sizeof(prev));

        // next contribution
        ctx.joint[1] = *buff;
        std::uint16_t next = 0;
        std::memcpy(&next, ctx.joint, sizeof(next));

        next -= prev;
        ctx.sum += next;
        bufflen--;
        buff++;

        // std::cerr << "adding [truncated] " << int_to_hexstring(next)
        //           << ", (subtracted " << int_to_hexstring(prev)
        //           << "), sum=" << int_to_hexstring(ctx.sum) << std::endl;
        //
        ctx.truncated = false;
        std::memset(ctx.joint, 0, sizeof(std::uint16_t));
    }



    std::uint32_t n = len / sizeof(T);
    std::uint32_t idx = 0;

    while (n--) {
        T currval = 0;
        memcpy(&currval, msg + idx, sizeof(currval));

        // NOTE: obviously, if typeof(J) == std:uint8_t, then endianness does
        // not matter. Also, the widest type of checksum_t is std::uint64_t --
        // and therefore the widest type of T is std::uint32_t as per the
        // algorithm.
        static_assert(!std::is_same_v<T, std::uint64_t>);
        if constexpr (nbo) {
            currval = bits::to_hbo(currval);
        }

        update(ctx,
               currval,
               modulus_v<decltype(currval)>,
               max_num_additions_without_modulo);

        idx += sizeof(currval);
    }

    return idx;
}

// The input is a buffer of bytes to be processed in sizeof(T)-byte chunks;
// IOW the buffer is treated as a sequence of Ts.
//
// The buffer may be stored in network-byte order (big endian), in which
// case that should be specified in the call to this function so that the
// appropriate byte swapping can be performed as the data is read.
//
// If last_block=false, then the function stops when the number of
// bytes remaining in the buffer is < sizeof(J). The index returned
// will be < len.
//
// Otherwise if last_block=true, all of data is consumed and padding is
// automatically added as required. In this case the index returned
// will always be equal to len and can be ignored.
//
// Return:
// - the index in data after the last byte that was processed; when all the
// bytes have been processed (which is always the case when last_block=true)
// this index is equal to len.
template<typename T,
         typename checksum_t,
         typename sum_t,
         bool network_byte_order,
         std::uint32_t max_num_additions_without_modulo>
std::uint32_t process_block(fletcher_ctx<sum_t> &ctx,
                            const std::uint8_t *data,
                            std::size_t len,
                            bool last_block) {
    std::uint32_t next_idx =
      impl::process_chunks<T,
                           checksum_t,
                           sum_t,
                           network_byte_order,
                           max_num_additions_without_modulo>(ctx, data, len);

    // pad and process this last chunk if this is the last block.
    // If this is not the last block, stop short; do not process
    // the remaining chunk of bytes if it is < sizeof(J) --
    // otherwise we would break the final checksum.
    if (last_block && next_idx < len) {
        // number of unprocessed bytes
        const auto n = len - next_idx;

        // Here we've just copied a number of bytes < sizeof(J)
        // to the smallest address of <value>.
        // (1) If the host system has the same endianness as the data (e.g. both
        // the host system and the data are big-endian or little endian) then
        // there is nothing to do.
        // (2) If the host system is big endian and the data is little endian,
        // this is an unsupported situation. Data is expected to be converted
        // to network byte order when moved between systems.
        // (3) If the host system is little endian and the data is big endian,
        // then we must shift up the bytes to increase their significance,
        // so that the bits::to_hbo call in process_chunks() works as expected.
        T value = 0;
        std::memcpy(&value, data + next_idx, std::min(sizeof(value), n));
        if constexpr (network_byte_order) {
            const unsigned nshifts = (sizeof(value) - n) * 8;

            // What we're doing here: if the host system is big-endian, then
            // because we copied the bytes just now to the lowest address,
            // shifting left will actually shift bits off the register,
            // decreasing the value. OTOH if the system is little endian,
            // shifting left will increase the value. IOW: we check whether
            // we can increase the significance of the bytes we have, and
            // if so, we do that.
            if ((value << nshifts) > value) {
                value <<= nshifts;
            }
        }

        std::uint8_t buff[sizeof(T)];
        std::memcpy(buff, &value, sizeof(value));

        next_idx = impl::process_chunks<T,
                                        checksum_t,
                                        sum_t,
                                        network_byte_order,
                                        max_num_additions_without_modulo>(
          ctx, buff, sizeof(T));
    }

    return next_idx;
}

// Assume data is the one and only block that will be used for
// computing the checksum.
template<typename T,
         typename checksum_t,
         typename sum_t,
         bool network_byte_order,
         std::uint32_t max_num_additions_without_modulo>
checksum_t process_all(const std::uint8_t *data, std::size_t len) {
    fletcher_ctx<sum_t> ctx;

    process_block<T,
                  checksum_t,
                  sum_t,
                  network_byte_order,
                  max_num_additions_without_modulo>(ctx, data, len, true);

    return get_checksum<T, checksum_t, sum_t>(ctx);
}

}  // namespace impl

namespace fletcher16 {
namespace detail {
// see https://en.wikipedia.org/wiki/Fletcher's_checksum.
//   < sqrt( sum_t::max()*2 / T::max()),
// with sum_t=u32 and T=u8.
using T = std::uint8_t;
using sum_t = std::uint32_t;
using checksum_t = std::uint16_t;

static inline constexpr std::uint32_t max_num_additions_without_modulo = 5802;
}  // namespace detail

inline std::uint16_t get_checksum(fletcher16_ctx &ctx) {
    using namespace detail;
    return impl::get_checksum<T, checksum_t, sum_t>(ctx);
}

template<bool big_endian>
std::uint32_t process_block(fletcher16_ctx &ctx,
                            const std::uint8_t *data,
                            std::size_t len,
                            bool last_block) {
    using namespace detail;

    //
    return impl::process_block<T,
                               checksum_t,
                               sum_t,
                               big_endian,
                               max_num_additions_without_modulo>(
      ctx, data, len, last_block);
}

template<bool big_endian>
std::uint16_t process_all(const std::uint8_t *data, std::size_t len) {
    using namespace detail;
    return impl::process_all<T,
                             checksum_t,
                             sum_t,
                             big_endian,
                             max_num_additions_without_modulo>(data, len);
}
}  // namespace fletcher16

/*  Found by solving for c1 overflow: */
/* n > 0 and n * (n+1) / 2 * (2^8-1) < (2^32-1). */

namespace fletcher32 {
namespace detail {
// see https://en.wikipedia.org/wiki/Fletcher's_checksum.
//   < sqrt( sum_t::max()*2 / T::max()),
// with sum_t=u32 and T=u16.
using T = std::uint16_t;
using sum_t = std::uint32_t;
using checksum_t = std::uint32_t;
static inline constexpr std::uint32_t max_num_additions_without_modulo = 361;
}  // namespace detail

inline std::uint32_t get_checksum(fletcher32_ctx &ctx) {
    using namespace detail;
    return impl::get_checksum<T, checksum_t, sum_t>(ctx);
}

template<bool big_endian>
std::uint32_t process_block(fletcher32_ctx &ctx,
                            const std::uint8_t *data,
                            std::size_t len,
                            bool last_block) {
    using namespace detail;

    return impl::process_block<T,
                               checksum_t,
                               sum_t,
                               big_endian,
                               max_num_additions_without_modulo>(
      ctx, data, len, last_block);
}

template<bool big_endian>
std::uint32_t process_all(const std::uint8_t *data, std::size_t len) {
    using namespace detail;

    return impl::process_all<T,
                             checksum_t,
                             sum_t,
                             big_endian,
                             max_num_additions_without_modulo>(data, len);
}
}  // namespace fletcher32

namespace fletcher64 {
namespace detail {
// see https://en.wikipedia.org/wiki/Fletcher's_checksum.
//   < sqrt( sum_t::max()*2 / T::max()),
// with sum_t=u64 and T=u32.
using T = std::uint32_t;
using sum_t = std::uint64_t;
using checksum_t = std::uint64_t;
static inline constexpr std::uint32_t max_num_additions_without_modulo = 92680;
}  // namespace detail

inline std::uint64_t get_checksum(fletcher64_ctx &ctx) {
    using namespace detail;
    return impl::get_checksum<T, checksum_t, sum_t>(ctx);
}

template<bool big_endian>
std::uint64_t process_all(const std::uint8_t *data, std::size_t len) {
    using namespace detail;
    return impl::process_all<T,
                             checksum_t,
                             sum_t,
                             big_endian,
                             max_num_additions_without_modulo>(data, len);
}

template<bool big_endian>
std::uint32_t process_block(fletcher64_ctx &ctx,
                            const std::uint8_t *data,
                            std::size_t len,
                            bool last_block) {
    using namespace detail;
    return impl::process_block<T,
                               checksum_t,
                               sum_t,
                               big_endian,
                               max_num_additions_without_modulo>(
      ctx, data, len, last_block);
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
template<bool network_byte_order>
std::uint16_t fletcher16(const std::uint8_t *data, std::size_t len) {
    return fletcher::fletcher16::process_all<network_byte_order>(data, len);
}

template<bool network_byte_order>
std::uint32_t fletcher32(const std::uint8_t *data, std::size_t len) {
    return fletcher::fletcher32::process_all<network_byte_order>(data, len);
}

template<bool network_byte_order>
std::uint64_t fletcher64(const std::uint8_t *data, std::size_t len) {
    return fletcher::fletcher64::process_all<network_byte_order>(data, len);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
