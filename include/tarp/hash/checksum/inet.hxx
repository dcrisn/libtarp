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
        ctx.joint[1] = *buff;
        std::uint16_t v;
        // interpret correctly no matter the endianness
        std::memcpy(&v, ctx.joint, sizeof(v));
        v -= ctx.joint[0];
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
        ctx.joint[0] = v;
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

// ipv4 (RFC1071) checksum
inline std::uint16_t inetv4(const std::uint8_t *data, std::size_t len) {
    return inet::process_all(data, len);
}

}  // namespace checksum
}  // namespace hash
}  // namespace tarp
