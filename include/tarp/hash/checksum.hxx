#pragma once

// local project
#include <tarp/bits.hxx>

// cxx stdlib
#include <limits>

// c stdlib
#include <cstdint>
#include <cstring>

extern "C" {
#include <endian.h>
}

namespace tarp {
namespace hash {
namespace checksum {

// This is to be able to maintain state and therefore be able to
// process data in blocks rather than all up front.
template<typename T>
struct fletcher_ctx {
    T sum1 = 0;
    T sum2 = 0;

    // for optimization to minimize and delay expensive
    // modulo operations (see below)
    std::uint32_t run = 0;
};

// NOTE: this requires some familiarity with the algorithm
// and its optimizations to understand. For example, fletcher16
// actually uses two uint8_t running sums that are ultimately
// combined into a uint16_t. However, while these sums, post
// any addition operation, will be < max(uint16), DURING an
// addition using a uint8_t incurs the risk of overflow;
// therefore these intermediate sums are normally stored as
// uint16_ts in order to addition overflow.
//
// A common optimization is to use use even wider types e.g.
// uint32_t for these in order to increase the number of
// additions that can be carried out without risk of
// overflow, thereby delaying the need for the comparatively
// expensive modulo operation. Using a uint64_t for fletcher16,
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
// to do a modulo % on them (without risk of overflow) is given by:
//     n < sqrt( (max(T) * 2)/max(X))
// where max(C) is std::numeric_limits<C>(), for C in {T, X}.
//
// See https://en.wikipedia.org/wiki/Fletcher's_checksum FMI.
//
// ctx.run counts the number of additions we've done without
// performing a modulo.
using fletcher16_ctx = fletcher_ctx<std::uint32_t>;
using fletcher32_ctx = fletcher_ctx<std::uint32_t>;
using fletcher64_ctx = fletcher_ctx<std::uint64_t>;

//

// state to enable procesing data in blocks
struct inetcksum_ctx {
    // internal accumulator keeping running sum;
    // NOTE: the assumption behind using a u32 here is that
    // it does NOT wrap around; this is a safe assumption
    // for ipv4 packets (and packets/frames of any typical
    // protocol) since they cannot be anywhere near as big
    // as u32::max(). However, this does mean the checksum
    // is not suitable for data that is so big it could possibly
    // cause the u32 to overflow [of course, the inet
    // checksum is weak and not suitable for that anyway!].
    std::uint32_t sum = 0;

    // true if the number of bytes consumed so far is not
    // a multiple of sizeof(u16)=2, and hence the last
    // addend was a u8 value, rather than a u16 value.
    bool truncated = false;

    // if truncated=true, then 'last' stores the last
    // u8 addend.
    std::uint8_t last = 0;
};

namespace inet {
// This is the IPV4 and TCP checksum;
// See RFC1071.
// > Adjacent octets to be checksummed are paired to form 16-bit
// > integers, and the 1's complement sum of these 16-bit integers is
// > formed.
//
// An implementation almost exactly like the one in RFC1071
// can be found in tests/hash.inet, which is used for
// verifying this implementation.
//
// The implementation given here instead makes it possible
// the compute the checksum one arbitrary-sized block
// at a time, rather than all in one go.
// This makes it convenient to checksum noncontiguous
// fields (include some fields, skip others) -- _without_
// needing to copy non-contiguous bytes to a separate
// buffer.


// Update the checksum's state with new bytes from
// the given byte span.
inline void update_checksum(const std::uint8_t *buff,
                            std::size_t bufflen,
                            inetcksum_ctx &ctx) {

    // Each call to update_checksum() assumes it is
    // the last call. And so if the buffer fed to it
    // is not a multiple of sizeof(u16)=2, then we're
    // left with 1 single byte at the end that is added
    // to the running sum as a u8 (as per the RFC1071 code).
    // However, update_checksum may then be invoked again
    // (with a non-zero buffer).
    // In that case we have the following situation:
    // a b  c d  e
    //  |    |   |
    // u16  u16  `we added this as a u8
    //
    // and now we have we're given another buffer, which
    // for the purposes of the checksum, is the same stream
    // of bytes. So:
    // a b c d e f g h ..
    //           |.....> new bytes
    //
    // On a big endian machine, reading 2 bytes e and f,
    // at indices n and n+1, respectively, from a buffer
    // into a u16 gives H=e,L=f. On a little endian machine,
    // it gives H=f,L=e. In either case, things will break
    // if we now just read f g into a u16 since we're 8
    // bits off positionally and the all values hereafter
    // will be thrown off.
    // So the following conditional performs the required
    // fixup. If we are in the situation where the last
    // addend was a u8 (truncated=true), then we place it
    // back in a buffer at index n, and put the next byte
    // from the new span next to it at n[+1]. In essence
    // we join the start of this new buffer to the end
    // of the previous one. We subtract the last addend
    // from this new value (always safe, since the addend
    // will necessarily be less than it), and then add
    // the new value to the running checksum. Then adjust
    // the new buffer and carry on as usual.
    if (ctx.truncated && bufflen > 0) {
        std::uint8_t fix[2];
        fix[0] = ctx.last;
        fix[1] = *buff;
        std::uint16_t v;
        // interpret the fixbuf correctly no matter the endianness
        std::memcpy(&v, fix, sizeof(v));
        v -= ctx.last;
        ctx.sum += v;
        bufflen--;
        buff++;
        ctx.truncated = false;
    }

    std::uint16_t v = 0;

    // sizeof(u16)==2, so the number of u16s is bufflen/2
    const std::size_t n = bufflen >> 1;

    for (unsigned i = 0; i < n; ++i) {
        // the safer way to do type punning, rather than reinterpret_cast
        std::memcpy(&v, buff, sizeof(std::uint16_t));
        buff += sizeof(std::uint16_t);
        ctx.sum += v;
    }

    // if bufflen is not a multiple of 2, we'll have a remainder of 1 byte
    if (bufflen & 0x1) {
        v = 0;
        std::memcpy(&v, buff, sizeof(std::uint8_t));
        ctx.sum += v;
        
        // so we can join up with the next buffer
        // if there is one.
        ctx.truncated = true;
        ctx.last = v;
    }
}

// Fold the u32 accumulator into a u16 checksum
// and return it.
inline std::uint16_t get_checksum(inetcksum_ctx &ctx) {
    auto sum = ctx.sum;

    // avoid needless computation
    constexpr std::uint16_t mask = std::numeric_limits<std::uint16_t>::max();

    // Fold 32-bit sum to 16 bits;
    // ~ repeat until no bit to the left
    // of the 16 u16 bits is set;
    while (sum >> 16) {
        sum = (sum & mask) + (sum >> 16);
    }

    // checksum
    return ~sum;
}

// update the inet checksum state with the result of processing
// one block
inline std::uint16_t process_block(inetcksum_ctx &ctx,
                                   const std::uint8_t *data,
                                   std::size_t len,
                                   bool last_block) {
    update_checksum(data, len, ctx);
    if (last_block) {
        return get_checksum(ctx);
    }
    return 0;
}

inline std::uint16_t process_all(const std::uint8_t *data, std::size_t len) {
    inetcksum_ctx ctx;
    update_checksum(data, len, ctx);
    return get_checksum(ctx);
}
}  // namespace inet

//

namespace fletcher {

namespace impl {

// get the modulus used for a fletcher that reads the input
// as a sequence of Ts. For example, for fletcher16 we use
// u16::max() = (2^16 - 1) as the modulus.
template<typename T>
static inline constexpr T modulus_v = std::numeric_limits<T>::max();

// Update the sums with the given value, while taking account of
// the optimization discussed above. We perform a modulo when we've
// reached the max_num_additions_without_modulo limit.
template<typename context_t, typename T>
static inline void update(context_t &ctx,
                          T value,
                          T modulus,
                          std::uint32_t max_num_additions_without_modulo) {
    if (ctx.run >= max_num_additions_without_modulo) {
        ctx.sum1 %= modulus;
        ctx.sum2 %= modulus;
        ctx.run = 0;
    }

    ctx.sum1 = (ctx.sum1 + value);
    ctx.sum2 = (ctx.sum2 + ctx.sum1);
    ctx.run++;
}

// Calculate and return the fletcherX checksum from the
// 2 internal sums.
template<typename T, typename checksum_t, typename sum_t>
checksum_t get_checksum(fletcher_ctx<sum_t> &ctx) {
    ctx.sum1 %= modulus_v<T>;
    ctx.sum2 %= modulus_v<T>;
    ctx.run = 0;

    checksum_t checksum = ctx.sum1 | (ctx.sum2 << tarp::bits::width_v<T>);
    return checksum;
}

// Process a block of data and update the current checksum
// value based on it. len **must** be a multiple of sizeof(T):
// any partial chunk ( < sizeof(T) ) at the end is not processed.
//
// NOTE: nbo = network byte order.
//
// Return the index of the byte after the end of the last processed chunk.
template<typename T,
         typename checksum_t,
         typename sum_t,
         bool nbo,
         std::uint32_t max_num_additions_without_modulo>
std::uint32_t process_chunks(fletcher_ctx<sum_t> &ctx,
                             const std::uint8_t *msg,
                             std::size_t len) {
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

//

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

// ipv4 (RFC1071) checksum
inline std::uint16_t inetv4(const std::uint8_t *data, std::size_t len) {
    return inet::process_all(data, len);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
