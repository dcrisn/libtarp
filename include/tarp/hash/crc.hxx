#pragma once

#include <tarp/bits.hxx>

// cxx stdlib
#include <array>

// c stdlib
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace tarp {
namespace hash {
namespace crc {

// Structure to maintain state so that we can buffer
// and therefore be able to process data in blocks
// as they arrive rather than all in one go.
template<typename T>
struct crc_context {
    // crc register
    T r = 0;

    // for idempotence.
    bool initialized = false;
};

using crc8_ctx = crc_context<std::uint8_t>;
using crc16_ctx = crc_context<std::uint16_t>;
using crc32_ctx = crc_context<std::uint32_t>;
using crc64_ctx = crc_context<std::uint64_t>;

//

// Parameters for various CRCs.
//
// See the crc_1bit_aat_v2 function below fmi.
//
// G = polynomial generator aka 'poly'
//
// rinit = CRC register initilization value
//
// xor_out = value to XOR the final checksum with before returning.
//
// reflect_in = whether to bit-reverse every input byte as it is read.
//
// reflect_out = whether to reflect the entire checksum before XORing
// it with xor_out.
//
namespace params {

// CRC-16/DECT-X  (X-CRC-16).
// NOTE: DECT = Digital Enhanced Cordless Telecommunications.
// G=0x0589: x^16+x^10+x^8+x^7+x^3+1.
namespace crc16_dectx {
static inline constexpr uint16_t G = 0x0589;
static inline constexpr uint16_t rinit = 0x0000;
static inline constexpr uint16_t xor_out = 0x0000;
static inline constexpr bool reflect_in = false;
static inline constexpr bool reflect_out = false;
}  // namespace crc16_dectx

namespace crc16_usb {
static inline constexpr uint16_t G = 0x8005;
static inline constexpr uint16_t rinit = 0xffff;
static inline constexpr uint16_t xor_out = 0xffff;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc16_usb

namespace crc16_gsm {
static inline constexpr uint16_t G = 0x1021;
static inline constexpr uint16_t rinit = 0x0000;
static inline constexpr uint16_t xor_out = 0xffff;
static inline constexpr bool reflect_in = false;
static inline constexpr bool reflect_out = false;
}  // namespace crc16_gsm

namespace crc16_kermit {
static inline constexpr uint16_t G = 0x1021;
static inline constexpr uint16_t rinit = 0x0000;
static inline constexpr uint16_t xor_out = 0x0000;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc16_kermit

namespace crc16_modbus {
static inline constexpr uint16_t G = 0x8005;
static inline constexpr uint16_t rinit = 0xFFFF;
static inline constexpr uint16_t xor_out = 0x0000;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc16_modbus

namespace crc8_bluetooth {
static inline constexpr uint8_t G = 0xA7;
static inline constexpr uint8_t rinit = 0x0000;
static inline constexpr uint8_t xor_out = 0x0000;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc8_bluetooth

// CRC-32/BZIP2
namespace crc32_bzip2 {
static inline constexpr uint32_t G = 0x04C11DB7;
static inline constexpr uint32_t rinit = 0xFFFFFFFF;
static inline constexpr uint32_t xor_out = 0xFFFFFFFF;
static inline constexpr bool reflect_in = false;
static inline constexpr bool reflect_out = false;
}  // namespace crc32_bzip2

// CRC-32/CKSUM aka CRC-32/POSIX. This is the crc32 algorithm used by
// the cksum cli utility. Well... apparently according to:
// https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.32
// https://crccalc.com.
// In practice the cli (ubuntu 24) does not seem to actually produce a
// checksum that matches the one listed as 'expected' for a given input!
namespace crc32_cksum {
static inline constexpr uint32_t G = 0x04C11DB7;
static inline constexpr uint32_t rinit = 0x00000000;
static inline constexpr uint32_t xor_out = 0xFFFFFFFF;
static inline constexpr bool reflect_in = false;
static inline constexpr bool reflect_out = false;
}  // namespace crc32_cksum

// CRC-32/CASTAGNOLI
namespace crc32c {
static inline constexpr uint32_t G = 0x1edc6f41;
static inline constexpr uint32_t rinit = 0xFFFFFFFF;
static inline constexpr uint32_t xor_out = 0xFFFFFFFF;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc32c

// This is the crc used by the crc32 utility on Linux (ubuntu24).
namespace crc32_hdlc {
static inline constexpr uint32_t G = 0x04c11db7;
static inline constexpr uint32_t rinit = 0xFFFFFFFF;
static inline constexpr uint32_t xor_out = 0xFFFFFFFF;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc32_hdlc

// The Go Authors, The Go Programming Language, package crc64 (as per reveng).
namespace crc64_go {
static inline constexpr uint64_t G = 0x000000000000001b;
static inline constexpr uint64_t rinit = ~0;
static inline constexpr uint64_t xor_out = ~0;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc64_go

namespace crc64_xz {
static inline constexpr uint64_t G = 0x42f0e1eba9ea3693;
static inline constexpr uint64_t rinit = ~0;
static inline constexpr uint64_t xor_out = ~0;
static inline constexpr bool reflect_in = true;
static inline constexpr bool reflect_out = true;
}  // namespace crc64_xz
}  // namespace params

//

// Bit-at-a-time implementations. These all read in the input data
// one bit at a time.
//
// SLOW. Not for direct use (unless you don't mind slowness).
//
// Meant for:
// - testing or validating implementations
// - generating tables for fast table-driven implementations.
// - understanding the actual CRC algorithms (the code in its pure form
// is typically unreadable).
//
// The overloads that take a ctx argument consume data in blocks.
// The overloads that do not take a ctx argument expect all data upfront.
namespace bitaat {
std::uint8_t crc8_bluetooth(const uint8_t *msg, std::size_t len);
std::uint8_t crc8_bluetooth(const uint8_t *msg, std::size_t le, crc8_ctx &ctx);

std::uint16_t crc16_dectx(const uint8_t *msg, std::size_t len);
std::uint16_t crc16_dectx(const uint8_t *msg, std::size_t len, crc16_ctx &ctx);

std::uint16_t crc16_usb(const uint8_t *msg, std::size_t len);
std::uint16_t crc16_usb(const uint8_t *msg, std::size_t len, crc16_ctx &ctx);

std::uint16_t crc16_gsm(const uint8_t *msg, std::size_t len);
std::uint16_t crc16_gsm(const uint8_t *msg, std::size_t len, crc16_ctx &ctx);

std::uint16_t crc16_kermit(const uint8_t *msg, std::size_t len);
std::uint16_t crc16_kermit(const uint8_t *msg, std::size_t len, crc16_ctx &ctx);

std::uint16_t crc16_modbus(const uint8_t *msg, std::size_t len);
std::uint16_t crc16_modbus(const uint8_t *msg, std::size_t len, crc16_ctx &ctx);

std::uint32_t crc32_bzip2(const uint8_t *msg, std::size_t len);
std::uint32_t crc32_bzip2(const uint8_t *msg, std::size_t len, crc32_ctx &ctx);

// CRC-32/CASTAGNOLI
std::uint32_t crc32c(const uint8_t *msg, std::size_t len);
std::uint32_t crc32c(const uint8_t *msg, std::size_t len, crc32_ctx &ctx);

// CRC-32/CKSUM a.k.a. CRC-32/POSIX.
std::uint32_t crc32_cksum(const uint8_t *msg, std::size_t len);
std::uint32_t crc32_cksum(const uint8_t *msg, std::size_t len, crc32_ctx &ctx);

std::uint32_t crc32_iso_hdlc(const uint8_t *msg, std::size_t len);
std::uint32_t
crc32_iso_hdlc(const uint8_t *msg, std::size_t len, crc32_ctx &ctx);

std::uint64_t crc64_go(const uint8_t *msg, std::size_t len);
std::uint64_t crc64_go(const uint8_t *msg, std::size_t len, crc64_ctx &ctx);

std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len);
std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len, crc64_ctx &ctx);

}  // namespace bitaat

namespace byteaat {
template<typename T, typename = std::enable_if_t<std::is_unsigned_v<T>, T>>
using lookup_table_t = std::array<T, 256>;

// CRC-16 GSM
std::uint16_t crc16_gsm(const uint8_t *msg,
                        std::size_t len,
                        const lookup_table_t<std::uint16_t> &);

std::uint16_t crc16_gsm(const uint8_t *msg,
                        std::size_t len,
                        crc16_ctx &ctx,
                        const lookup_table_t<std::uint16_t> &);

// CRC-32/CASTAGNOLI
std::uint32_t crc32c(const uint8_t *msg,
                     std::size_t len,
                     const lookup_table_t<std::uint32_t> &);

std::uint32_t crc32c(const uint8_t *msg,
                     std::size_t len,
                     crc32_ctx &ctx,
                     const lookup_table_t<std::uint32_t> &);

std::uint64_t crc64_xz(const uint8_t *msg,
                       std::size_t len,
                       const lookup_table_t<std::uint64_t> &);

std::uint64_t crc64_xz(const uint8_t *msg,
                       std::size_t len,
                       crc64_ctx &ctx,
                       const lookup_table_t<std::uint64_t> &);

};  // namespace byteaat

//

namespace impl {
// Template model that can be instantiated to implement specific CRC algorithms.
//
// --> T
// T is a fixed-width unsigned integral type w bits in width (e.g.
// uint8_t, uint16_t, uint32_t, uint64_t and such) used to implement
// an n-bit CRC. For example a CRC8 would use T=std::uint8_t and a CRC32
// would use T=std::uint32_t.
//
// --> init
// An initial value to XOR into the first w bits of the input message, for
// initialization. NOTE: if RefIn (input reflection) is used, then the XOR
// initialization happens **after** the reflection.
//
// --> xor_out
// A value to XOR the final CRC register value with before returning it.
//
// --> refin
// reflect input bits. If true, the bits in each individual byte are reversed
// ('reflected'). For example if the next input byte is 0xff10 then after
// reflection it will be 0x01ff.
//
// --> refout
// reflect output. If true, the entire checksum is bitwise reversed
// ('reflected') before returning it. NOTE: if xor_out is specified
// (as a non-0 value), then the XOR happens **after** the output reflection.
// NOTE: the output reflection does not happen byte by byte, but for the
// entire T-type checksum collectively, treated as a bitstring.
//
// This implementation makes some optimizations compared to the previous
// version.
// - the message is read one byte at a time instead of one bit at a time
// (but the processing is still bit by bit i.e. we still do something
// _per bit_)
// - no trailer is used and therefore w fewer iterations are made.
template<typename T, T init, T xor_out, bool refin, bool refout>
T crc_1bit_aat_v2(T G, const uint8_t *msg, std::size_t len) {
    const uint8_t *p = msg;

    // checksum register. Holds w bits, implementing a w-bit CRC.
    T r = init;

    // for each byte in the input message.
    while (len--) {
        std::uint8_t next_byte = *p++;

        // reflect bits in each individual input byte
        if constexpr (refin) {
            next_byte = bits::reflect_bits(next_byte);
        }

        r ^= (static_cast<T>(next_byte) << (bits::width_v<T> - 8));

        // for each bit in the byte
        for (unsigned i = 0; i < bits::BITS_IN_BYTE; ++i) {
            // if the most significant bit of the most significant byte in the
            // register, before the left shift, is 1, then we perform the XOR.
            // In concrete terms: if the (w+1)th bit in the w-bit register
            // is 1, then the value (imagined as a (w+1)-bit value) in the
            // register is >= to the (w+1)-bit divisor (the (w+1)-bit generator
            // polynomial, 'poly'), according to the rules of MOD2 polynomial
            // arithmetic. IOW the bit just shifted off extends the w-bit value
            // in the register to a w+1-bit value, and is as such compared with
            // the unwritten 'understood' bit (which is always a constant 1)
            // that corresponds to the highest-power in the generator
            // polynomial.
            bool must_subtract = (bits::msb(bits::MSB(r)) == 1);
            r <<= 1;

            // In MOD2 polynomial arithmetic subtraction (and addition) are
            // simply XOR since no carries are involved. Therefore here we
            // (conceptually) subtract the divisor (the poly) from the value in
            // the register (which has been determined to be >= the poly by the
            // (w+1)-bit comparison above).
            if (must_subtract) {
                r = r ^ G;
            }
        }
    }

    // reflect all bits in the output
    if (refout) {
        r = bits::reflect_bits(r);
    }

    // final output xor. NOTE: if xor_out is 0, then nothing happens
    // since a XOR 0 = a.
    return r ^ xor_out;
}

// See crc_1bit_aat_v2.
// This implementation reads the message bit by bit.
// It also appends a w-bit trailer to the message.
// This implementation is essentially to most readable and basic
// following of a CRC algorithm.
template<typename T, T init, T xor_out, bool refin, bool refout>
T crc_1bit_aat_v1(T G, const uint8_t *msg, std::size_t len) {
    const uint8_t *p = msg;

    // checksum register. Holds w bits, implementing a w-bit CRC.
    T r = 0;

    // the XOR initializer for the first w bits (including the trailer, if
    // the message proper has fewer bits than the register width).
    T initializer = init;

    // w all-0s bits get appended to the message.
    std::uint8_t trailer[sizeof(T)];
    std::memset(trailer, 0, sizeof(T));
    bool trailer_added = false;

loop:
    // for each byte in the input message.
    while (len--) {
        std::uint8_t next_byte = *p++;

        // reflect bits in each individual input byte
        if constexpr (refin) {
            next_byte = bits::reflect_bits(next_byte);
        }

        // If an XOR initializer was specified (and hasn't been used up)
        if (initializer != 0) {
            next_byte ^= (bits::MSB(initializer));
            initializer <<= bits::BITS_IN_BYTE;
        }

        // for each bit in the byte
        for (unsigned i = 0; i < bits::BITS_IN_BYTE; ++i) {
            // if the most significant bit of the most significant byte in the
            // register, before the left shift, is 1, then we perform the XOR.
            // In concrete terms: if the (w+1)th bit in the w-bit register
            // is 1, then the value (imagined as a (w+1)-bit value) in the
            // register is >= to the (w+1)-bit divisor (the (w+1)-bit generator
            // polynomial, 'poly'), according to the rules of MOD2 polynomial
            // arithmetic. IOW the bit just shifted off extends the w-bit value
            // in the register to a w+1-bit value, and is therefore compared
            // compared with the unwritten 'understood' bit (which is always a
            // constant 1) that corresponds to the highest-power in the
            // generator polynomial.
            bool must_subtract = (bits::msb(bits::MSB(r)) == 1);
            r <<= 1;

            // shift into the register the next most-significant
            // bit in the most-significant message byte remaining.
            const uint8_t msg_bit = (next_byte >> (7 - i)) & 0x1;
            r |= msg_bit;

            // In MOD2 polynomial arithmetic subtraction (and addition) are
            // simply XOR since no carries are involved. Therefore here we
            // (conceptually) subtract the divisor (the poly) from the value in
            // the register (which has been determined to be >= the poly by the
            // (w+1)-bit comparison above).
            if (must_subtract) {
                r = r ^ G;
            }
        }
    }

    // append a w-bit all-0s trailer to the message; notice how when the message
    // is shorter than the register width, the xor-in initializer ends up being
    // applied to (some of) these padding/trailer bytes.
    if (!trailer_added) {
        trailer_added = true;
        p = trailer;
        len = sizeof(trailer);
        goto loop;
    }

    // reflect all bits in the output
    if (refout) {
        r = reflect_bits(r);
    }

    // final output xor. NOTE: if xor_out is 0, then nothing happens
    // since a XOR 0 = a.
    return r ^ xor_out;
}
}  // namespace impl

template<typename T, T init, T xor_out, bool refin, bool refout>
T crc_1bit_aat(T G, const uint8_t *msg, std::size_t len) {
    return impl::crc_1bit_aat_v2<T, init, xor_out, refin, refout>(G, msg, len);
}

// Function that can be called on data that arrives in blocks.
// The value returned is the CRC calculated based on all the data
// fed in up to the point of the latest call. I.e. the latest CRC
// register value is returned, prepared as a final output.
// To simply retrieve the latest CRC without processing any data,
// specify len=0 (msg is ignored in that case).
// Uses the bitaat v2 function code from above.
template<typename T, T init, T xor_out, bool refin, bool refout>
T make_crc_1bit_aat(T G,
                    const uint8_t *msg,
                    std::size_t len,
                    struct crc_context<T> &ctx) {
    const uint8_t *p = msg;

    if (!ctx.initialized) {
        ctx.initialized = true;
        ctx.r = init;
    }

    // for each byte in the input message.
    while (len--) {
        std::uint8_t next_byte = *p++;

        // reflect bits in each individual input byte
        if constexpr (refin) {
            next_byte = bits::reflect_bits(next_byte);
        }

        ctx.r ^= (static_cast<T>(next_byte) << (bits::width_v<T> - 8));

        // for each bit in the byte
        for (unsigned i = 0; i < bits::BITS_IN_BYTE; ++i) {
            // if the most significant bit of the most significant byte in the
            // register, before the left shift, is 1, then we perform the XOR.
            // In concrete terms: if the (w+1)th bit in the w-bit register
            // is 1, then the value (imagined as a (w+1)-bit value) in the
            // register is >= to the (w+1)-bit divisor (the (w+1)-bit generator
            // polynomial, 'poly'), according to the rules of MOD2 polynomial
            // arithmetic. IOW the bit just shifted off extends the w-bit value
            // in the register to a w+1-bit value, and is as such compared with
            // the unwritten 'understood' bit (which is always a constant 1)
            // that corresponds to the highest-power in the generator
            // polynomial.
            bool must_subtract = (bits::msb(bits::MSB(ctx.r)) == 1);
            ctx.r <<= 1;

            // In MOD2 polynomial arithmetic subtraction (and addition) are
            // simply XOR since no carries are involved. Therefore here we
            // (conceptually) subtract the divisor (the poly) from the value in
            // the register (which has been determined to be >= the poly by the
            // (w+1)-bit comparison above).
            if (must_subtract) {
                ctx.r = ctx.r ^ G;
            }
        }
    }

    // Here we perform these final changes on the copy returned
    // to the caller, not the actual register, since we don't
    // know if this is the last block of data!
    T r = ctx.r;

    // reflect all bits in the output
    if (refout) {
        r = bits::reflect_bits(ctx.r);
    }

    // final output xor. NOTE: if xor_out is 0, then nothing happens
    // since a XOR 0 = a.
    return r ^ xor_out;
}

// D.Sarwate algorithm/optimization. Read the input one byte at a time,
// instead of bit by bit, and use the byte as a key in a precomputed
// lookup table. The lookup replaces the equivalent 8-pass loop
// (for each bit in a byte). See also make_lookup_table() below.
template<typename T, T init, T xor_out, bool refin, bool refout>
T make_crc_1byte_aat(const uint8_t *msg,
                     std::size_t len,
                     struct crc_context<T> &ctx,
                     const std::array<T, 256> &t) {
    if (!ctx.initialized) {
        ctx.initialized = true;
        ctx.r = init;
    }

    static constexpr unsigned shift = bits::width_v<T> - 8;

    // for each byte in the input message.
    while (len--) {
        std::uint8_t next_byte = *msg++;

        // reflect bits in each individual input byte
        if constexpr (refin) {
            next_byte = bits::reflect_bits(next_byte);
        }

        ctx.r = (ctx.r << 8) ^ t[next_byte ^ (ctx.r >> shift)];
    }

    // Here we perform these final changes on the copy returned
    // to the caller, not the actual register, since we don't
    // know if this is the last block of data!
    T r = ctx.r;

    // reflect all bits in the output
    if (refout) {
        r = bits::reflect_bits(ctx.r);
    }

    // final output xor. NOTE: if xor_out is 0, then nothing happens
    // since a XOR 0 = a.
    return r ^ xor_out;
}

// Populate a lookup table to be used for a CRCX
// (e.g. CRC32 (with T=std::uint32_t)) with poly=G.
// The lookup table is byte-indexed and therefore
// it stores 2^8=256 entries.
// The lookup table can then be fed to make_crc_1byte_aat().
template<typename T>
void make_lookup_table(T G, std::array<T, 256> &t) {
    // for each possible 8-bit value
    for (unsigned byte = 0; byte < 256; ++byte) {
        // the CRC register starts as all-0s;
        // put the byte in the MSB of the CRC register;
        T r = (static_cast<T>(byte) << (bits::width_v<T> - 8));

        // we run the usual loop for each bit in the byte,
        // XORing the poly into the register as appropriate.
        // After the 8-pass loop, the register contains
        // a value resulting from all the XORs; this value
        // therefore is equivalent to the 8-pass loop
        // and we store it in the lookup table, thereby replacing
        // the loop with a lookup. This is the
        // idea behind this optimization (V. Sarwate algorithm).
        for (unsigned i = 0; i < bits::BITS_IN_BYTE; ++i) {
            bool must_subtract = (bits::msb(bits::MSB(r)) == 1);
            r <<= 1;
            if (must_subtract) {
                r = r ^ G;
            }
        }

        t[byte] = r;
    }
}

}  // namespace crc
}  // namespace hash
}  // namespace tarp
