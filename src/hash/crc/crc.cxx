#include <tarp/hash/crc.hxx>

#include <cstdint>
#include <cstring>

namespace tarp {
namespace hash {
namespace crc {
namespace bitaat {

// CRC-16/DECT-X  (X-CRC-16).
// NOTE: DECT = Digital Enhanced Cordless Telecommunications.
// G=0x0589: x^16+x^10+x^8+x^7+x^3+1.
uint16_t crc16_dectx(const uint8_t *msg, size_t len, crc16_ctx &ctx) {
    constexpr uint16_t G = 0x0589;
    constexpr uint16_t rinit = 0x0000;
    constexpr uint16_t xor_out = 0x0000;
    constexpr bool reflect_in = false;
    constexpr bool reflect_out = false;

    return make_crc_1bit_aat<std::uint16_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

uint16_t crc16_dectx(const uint8_t *msg, size_t len) {
    crc16_ctx ctx;
    return crc16_dectx(msg, len, ctx);
}

uint16_t crc16_usb(const uint8_t *msg, size_t len, crc16_ctx &ctx) {
    constexpr uint16_t G = 0x8005;
    constexpr uint16_t rinit = 0xffff;
    constexpr uint16_t xor_out = 0xffff;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint16_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

uint16_t crc16_usb(const uint8_t *msg, size_t len) {
    crc16_ctx ctx;
    return crc16_usb(msg, len, ctx);
}

uint16_t crc16_gsm(const uint8_t *msg, size_t len, crc16_ctx &ctx) {
    constexpr uint16_t G = 0x1021;
    constexpr uint16_t rinit = 0x0000;
    constexpr uint16_t xor_out = 0xffff;
    constexpr bool reflect_in = false;
    constexpr bool reflect_out = false;

    return make_crc_1bit_aat<std::uint16_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

uint16_t crc16_gsm(const uint8_t *msg, size_t len) {
    crc16_ctx ctx;
    return crc16_gsm(msg, len, ctx);
}

uint16_t crc16_kermit(const uint8_t *msg, size_t len, crc16_ctx &ctx) {
    constexpr uint16_t G = 0x1021;
    constexpr uint16_t rinit = 0x0000;
    constexpr uint16_t xor_out = 0x0000;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint16_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

uint16_t crc16_kermit(const uint8_t *msg, size_t len) {
    crc16_ctx ctx;
    return crc16_kermit(msg, len, ctx);
}

uint16_t crc16_modbus(const uint8_t *msg, size_t len, crc16_ctx &ctx) {
    constexpr uint16_t G = 0x8005;
    constexpr uint16_t rinit = 0xFFFF;
    constexpr uint16_t xor_out = 0x0000;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint16_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

uint16_t crc16_modbus(const uint8_t *msg, size_t len) {
    crc16_ctx ctx;
    return crc16_modbus(msg, len, ctx);
}

std::uint8_t
crc8_bluetooth(const uint8_t *msg, std::size_t len, crc8_ctx &ctx) {
    constexpr uint8_t G = 0xA7;
    constexpr uint8_t rinit = 0x0000;
    constexpr uint8_t xor_out = 0x0000;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint8_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint8_t crc8_bluetooth(const uint8_t *msg, std::size_t len) {
    crc8_ctx ctx;
    return crc8_bluetooth(msg, len, ctx);
}

// CRC-32/BZIP2
std::uint32_t crc32_bzip2(const uint8_t *msg, std::size_t len, crc32_ctx &ctx) {
    constexpr uint32_t G = 0x04C11DB7;
    constexpr uint32_t rinit = 0xFFFFFFFF;
    constexpr uint32_t xor_out = 0xFFFFFFFF;
    constexpr bool reflect_in = false;
    constexpr bool reflect_out = false;

    return make_crc_1bit_aat<std::uint32_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint32_t crc32_bzip2(const uint8_t *msg, std::size_t len) {
    crc32_ctx ctx;
    return crc32_bzip2(msg, len, ctx);
}

// CRC-32/CKSUM aka CRC-32/POSIX. This is the crc32 algorithm used by
// the cksum cli utility. Well... apparently according to:
// https://reveng.sourceforge.io/crc-catalogue/17plus.htm#crc.cat-bits.32
// https://crccalc.com.
// In practice the cli (ubuntu 24) does not seem to actually produce a checksum
// that matches the expected one for a given input!
std::uint32_t crc32_cksum(const uint8_t *msg, std::size_t len, crc32_ctx &ctx) {
    constexpr uint32_t G = 0x04C11DB7;
    constexpr uint32_t rinit = 0x00000000;
    constexpr uint32_t xor_out = 0xFFFFFFFF;
    constexpr bool reflect_in = false;
    constexpr bool reflect_out = false;

    return make_crc_1bit_aat<std::uint32_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint32_t crc32_cksum(const uint8_t *msg, std::size_t len) {
    crc32_ctx ctx;
    return crc32_cksum(msg, len, ctx);
}

// CRC-32/CASTAGNOLI
std::uint32_t crc32c(const uint8_t *msg, std::size_t len, crc32_ctx &ctx) {
    constexpr uint32_t G = 0x1edc6f41;
    constexpr uint32_t rinit = 0xFFFFFFFF;
    constexpr uint32_t xor_out = 0xFFFFFFFF;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint32_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint32_t crc32c(const uint8_t *msg, std::size_t len) {
    crc32_ctx ctx;
    return crc32c(msg, len, ctx);
}

// This is the crc used by the crc32 utility on Linux (ubuntu24).
std::uint32_t
crc32_iso_hdlc(const uint8_t *msg, std::size_t len, crc32_ctx &ctx) {
    constexpr uint32_t G = 0x04c11db7;
    constexpr uint32_t rinit = 0xFFFFFFFF;
    constexpr uint32_t xor_out = 0xFFFFFFFF;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint32_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint32_t crc32_iso_hdlc(const uint8_t *msg, std::size_t len) {
    crc32_ctx ctx;
    return crc32_iso_hdlc(msg, len, ctx);
}

// The Go Authors, The Go Programming Language, package crc64 (as per reveng).
std::uint64_t crc64_go(const uint8_t *msg, std::size_t len, crc64_ctx &ctx) {
    constexpr uint64_t G = 0x000000000000001b;
    constexpr uint64_t rinit = ~0;
    constexpr uint64_t xor_out = ~0;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint64_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint64_t crc64_go(const uint8_t *msg, std::size_t len) {
    crc64_ctx ctx;
    return crc64_go(msg, len, ctx);
}

std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len, crc64_ctx &ctx) {
    constexpr uint64_t G = 0x42f0e1eba9ea3693;
    constexpr uint64_t rinit = ~0;
    constexpr uint64_t xor_out = ~0;
    constexpr bool reflect_in = true;
    constexpr bool reflect_out = true;

    return make_crc_1bit_aat<std::uint64_t,
                             rinit,
                             xor_out,
                             reflect_in,
                             reflect_out>(G, msg, len, ctx);
}

std::uint64_t crc64_xz(const uint8_t *msg, std::size_t len) {
    crc64_ctx ctx;
    return crc64_xz(msg, len, ctx);
}



}  // namespace bitaat
}  // namespace crc
}  // namespace hash
}  // namespace tarp
