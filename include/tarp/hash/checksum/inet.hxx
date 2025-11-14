#pragma once

// local project
#include <tarp/bits.hxx>
// #include <tarp/string_utils.hxx>

// cxx stdlib
#include <limits>

// c stdlib
#include <cstdint>
#include <cstring>
#include <type_traits>

extern "C" {
#include <endian.h>
}

namespace tarp {
namespace hash {
namespace checksum {

// using tarp::utils::str::int_to_hexstring;

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

    // if truncated=true, then joint[0] stores the last u8 addend.
    std::uint8_t joint[2];
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
inline void update_checksum(inetcksum_ctx &ctx,
                            const std::uint8_t *buff,
                            std::size_t bufflen) {
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
    // u16  u16  `we added this as 0e (LE) or e0 (BE)
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
    // fixup. If we are in the situation where we last
    // had a trailing byte, (truncated=true), then we place
    // it back in a buffer at index n, and put the next byte
    // from the new span next to it at n[+1]. In essence
    // we join the start of this new buffer to the end
    // of the previous one. We subtract the last addend
    // from this new value (always safe, since the addend
    // will necessarily be less than it), and then add
    // the new value to the running checksum. Then adjust
    // the new buffer and carry on as usual.
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

    // sizeof(u16)==2, so the number of u16s is bufflen/2
    const std::size_t n = bufflen >> 1;

    for (unsigned i = 0; i < n; ++i) {
        std::uint16_t v = 0;
        // the safer way to do type punning, rather than reinterpret_cast
        std::memcpy(&v, buff, sizeof(std::uint16_t));
        buff += sizeof(std::uint16_t);
        ctx.sum += v;
        // std::cerr << "[" << i << "] adding " << int_to_hexstring(v)
        //           << ", sum=" << int_to_hexstring(ctx.sum) << std::endl;
    }

    // if bufflen is not a multiple of 2, we'll have a remainder of 1 byte
    if (bufflen & 0x1) {
        std::uint16_t v = 0;
        std::memcpy(&v, buff, sizeof(std::uint8_t));
        ctx.sum += v;
        // std::cerr << "adding trailing byte: " << int_to_hexstring(v)
        //           << ", sum=" << int_to_hexstring(ctx.sum) << std::endl;

        // so we can join up with the next buffer
        // if there is one.
        ctx.truncated = true;
        ctx.joint[0] = v;
    }
}

// Incremental checksum update.
// This lets us update a checksum when a field changes,
// without recomputing it from scratch.
// get_checksum can be called as usual to then retrieve
// the updated checksum.
//
// RFC1624-2:
// > As RFC 1141 points out, the equation above is not useful for direct
// > use in incremental updates since C and C' do not refer to the actual
// > checksum stored in the header.  In addition, it is pointed out that
// > RFC 1071 did not specify that all arithmetic must be performed using
// > one's complement arithmetic.
// >   HC' = ~(C + (-m) + m')    --    [Eqn. 3]
// >       = ~(~HC + ~m + m')
// With:
// >   HC  - old checksum in header
// >   C   - one's complement sum of old header
// >   HC' - new checksum in header
// >   C'  - one's complement sum of new header
// >   m   - old value of a 16-bit field
// >   m'  - new value of a 16-bit field
//
// ----------------------------------------------
// ----------------------------------------------
// NOTE: A tacit assumption in the RFCS seems to be that all inputs
// to the checksum (both in general, and in particular here
// for the incremental update procedure) are u16 words, always
// starting at sizeof(u16) boundaries.
//
// In other words, it appears incremental updates only work for
// fields for which **all** of the following are true:
// - they start (in the buffer over which the original checksum
// was computed) at an offset that is a multiple of sizeof(u16).
// For example: 0, 2, 4, 6, 128, but not 1, 3, 5, 7, 121.
// - their size is a multiple of sizeof(u16). For example: u16,
// u32, u64, but not u8, or a packed field of 5 bytes etc.
// Therefore the cases where incremental updates are straightforwardly
// applicable are quite narrow and it is easy to make mistakes.
//
// To understand the issue flagged here, consider an input buffer with
// bytes {a, b, c, d, e}. Assume 'a' corresponds to a u8 field and {b,c}
// to a u16 field. Assume we want to update the u16 field.
// The incremental update procedure fundamentally subtracts the old {b,c}
// and adds the new {b,c} value. However, notice {b,c} was never a u16
// that was added to the sum in the original checksum! Because iterating
// over the buffer would yield {a,b}, {c,d} .. as the u16 words!
// Therefore subtracting {b,c} is not subtracting a contribution
// that was ever made, and hence is incorrect. In short, in the
// conditions given in the NOTE above, the byte significance
// of the field will not coincide with the u16 word(s) that were
// used in the original checksum and hence will break it.
// Of course, incremental checksum can still be performed even
// then, but only in a manual way, carefully including adjacent
// fields as needed, forming the bytes into the exact same words
// that would've been used in the original checksum. Inevitably,
// a very error prone task.
// ----------------------------------------------
// ----------------------------------------------
template<bool to_hbo, typename T>
inline void update_checksum(inetcksum_ctx &ctx, T old_value, T new_value) {
    static_assert(std::is_unsigned_v<T>);
    static_assert(sizeof(T) >= sizeof(std::uint16_t));

    if constexpr (to_hbo) {
        old_value = bits::to_hbo(old_value);
        new_value = bits::to_hbo(new_value);
    }

    const auto add = [&ctx](T value) {
        constexpr std::size_t u16sz = sizeof(std::uint16_t);
        std::uint8_t tmp[sizeof(T)];

        std::memcpy(tmp, &value, sizeof(T));

        // NOTE: this means u8s are rejected offhand;
        // which is ok, incremental updates will not work
        // with u8 or any field whose size is not a multiple
        // of sizeof(u16) or that does not start at an offset
        // that is a multiple of sizeof(u16).
        constexpr std::size_t n = sizeof(T) / u16sz;

        for (unsigned i = 0; i < n; ++i) {
            std::uint16_t v = 0;
            std::memcpy(&v, &tmp[i * u16sz], u16sz);
            ctx.sum += v;
            // std::cerr << "adding " << v << ", sum=" << ctx.sum << std::endl;
        }
    };

    // subtract old contribution from sum;
    // add new contribution to sum;
    // see RFC1624-4.
    add(~old_value);
    add(new_value);

    // no get_checksum() will return the updated checksum.
}

// Incremental checksum update.
// This function, unlike the overload that takes a type T
// field value as input, works even when:
// - the number of changed bytes < sizeof(u16)
// - the change starts at an odd offset (i.e. an offset
//   that is not a multiple of sizeof(u16))
// - the length of the change is odd (not a multiple of
//   sizeof(u16))
//
// buff is the buffer that original checksum (that the update
// is being applied to) was calculated over.
// change is a buffer that contains the bytes that have
// changed; the change occurs at
// buff[change_offset] ... buff[change_offset+change_len].
//
// change_offset+change_len must be <= bufflen.
void update_checksum(inetcksum_ctx &ctx,
                     const std::uint8_t *buff,
                     std::size_t bufflen,
                     std::size_t change_offset,
                     std::size_t change_len,
                     const std::uint8_t *change);

inline std::uint16_t fold_sum(std::uint32_t sum) {
    // avoid needless computation
    constexpr std::uint16_t mask = std::numeric_limits<std::uint16_t>::max();

    // Fold 32-bit sum to 16 bits;
    // ~ repeat until no bit to the left
    // of the 16 u16 bits is set;
    while (sum >> 16) {
        sum = (sum & mask) + (sum >> 16);
    }

    return sum;
}

// Fold the u32 accumulator into a u16 checksum
// and return it.
inline std::uint16_t get_checksum(inetcksum_ctx &ctx) {
    // we calculate on temporary variable, not changing
    // context variable, since update_checksum()
    // could be called again!
    auto sum = fold_sum(ctx.sum);

    // RFC791-3.1:
    // The checksum field is the 16 bit one's complement of the
    // one's complement sum of all 16 bit words in the header.
    const std::uint16_t cksum = ~sum;
    return cksum;
}

// update the inet checksum state with the result of processing
// one block
inline std::uint16_t process_block(inetcksum_ctx &ctx,
                                   const std::uint8_t *data,
                                   std::size_t len,
                                   bool last_block) {
    update_checksum(ctx, data, len);
    if (last_block) {
        return get_checksum(ctx);
    }
    return 0;
}

inline std::uint16_t process_all(const std::uint8_t *data, std::size_t len) {
    inetcksum_ctx ctx;
    update_checksum(ctx, data, len);
    return get_checksum(ctx);
}
}  // namespace inet

// ipv4 (RFC1071) checksum
inline std::uint16_t inetv4(const std::uint8_t *data, std::size_t len) {
    return inet::process_all(data, len);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
