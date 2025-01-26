#pragma once

#include <tarp/bits.hxx>

// cxx stdlib

// c stdlib
#include <cstdint>
#include <cstring>

namespace tarp {
namespace hash {
namespace crc {


// Structure to maintain state so that we can buffer
// and therefore be able to process data in blocks
// as they arrive rather than all in one go.
template<typename T>
struct crc_context {
    T r = 0;

    // for idempotence.
    bool initialized = false;
};

using crc8_ctx = crc_context<std::uint8_t>;
using crc16_ctx = crc_context<std::uint16_t>;
using crc32_ctx = crc_context<std::uint32_t>;
using crc64_ctx = crc_context<std::uint64_t>;

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

//  CRC-32/CASTAGNOLI
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
        r = reflect_bits(r);
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

}  // namespace bitaat
}  // namespace crc
}  // namespace hash
}  // namespace tarp
