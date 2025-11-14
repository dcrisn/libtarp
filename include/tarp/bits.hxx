#pragma once

// cxx stdlib
#include <endian.h>
#include <type_traits>

// c stdlib
#include <cstdint>
#include <iostream>

namespace tarp {
namespace bits {

static inline constexpr unsigned BITS_IN_BYTE = 8;

// The width in bits of a type T.
template<typename T>
struct width_t {
    static inline std::integral_constant<unsigned, sizeof(T) * BITS_IN_BYTE>
      value;
};

template<typename T>
static inline constexpr unsigned width_v = width_t<T>::value;

// test whether the most signicant bit in a byte is 0 or 1
static inline std::uint8_t msb(std::uint8_t byte) {
    return ((0x80 & byte) >> 7);
}

// get the most significant byte of the input type, shifted down.
template<typename T>
static inline std::uint8_t MSB(T input) {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);

    constexpr unsigned n = (sizeof(T) - 1) * BITS_IN_BYTE;
    return (input >> n) & 0xFF;
}

namespace impl {
extern std::uint8_t reflection[];

inline std::uint8_t reflect_byte_fast(std::uint8_t byte) {
    return reflection[byte];
}
}  // namespace impl

// reverse the lowest n bits in input.
// e.g. for n=4: 0b1010....1100 => 0b1010.....0011.
template<typename T>
inline T reflect_bits(T input, unsigned n) {
    static_assert(std::is_integral_v<T>);

    if constexpr (sizeof(T) == sizeof(std::uint8_t)) {
        if (n == width_t<std::uint8_t>::value) {
            // use lookup table.
            return impl::reflect_byte_fast(static_cast<std::uint8_t>(input));
        }
    }

    // discard lower n bits.
    T output = (input >> n);

    for (unsigned i = 0; i < n; ++i) {
        uint8_t bit = (input >> i) & 0x1;
        output = (output << 1) | bit;
    }

    return output;
}

// Reverse all bits in input.
template<typename T>
T reflect_bits(T input) {
    return reflect_bits(input, width_v<T>);
}

// Swap the bytes in value from network byte order (nbo) to
// host byte order (hbo).
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T> to_hbo(T value) {
    static_assert(
      std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
      std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);

    if constexpr (std::is_same_v<T, std::uint8_t>) {
        return value;
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
        return be16toh(value);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        return be32toh(value);
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        return be64toh(value);
    }
}

template<typename T>
std::enable_if_t<std::is_enum_v<T>, T> to_hbo(T value) {
    using underlying_t = std::underlying_type_t<T>;
    return static_cast<T>(to_hbo(static_cast<underlying_t>(value)));
}

// big endian to little endian
template<typename T>
T be2le(T value) {
    return to_hbo(value);
}

// Swap the bytes in value from host byte order (hbo) to
// network byte order (nbo).
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T> to_nbo(T value) {
    static_assert(
      std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::uint16_t> ||
      std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);

    if constexpr (std::is_same_v<T, std::uint8_t>) {
        return value;
    } else if constexpr (std::is_same_v<T, std::uint16_t>) {
        return htobe16(value);
    } else if constexpr (std::is_same_v<T, std::uint32_t>) {
        return htobe32(value);
    } else if constexpr (std::is_same_v<T, std::uint64_t>) {
        return htobe64(value);
    }
}

template<typename T>
std::enable_if_t<std::is_enum_v<T>, T> to_nbo(T value) {
    using underlying_t = std::underlying_type_t<T>;
    return static_cast<T>(to_nbo(static_cast<underlying_t>(value)));
}

// little endian to big-endian
template<typename T>
T le2be(T value) {
    return to_nbo(value);
}

// big endian to little endian
template<typename T>
T byteswap(T value) {
    // note it does not matter if we call
    // be2le or le2be; the main thing is
    // the bytes are swapped to the opposite
    // order.
    return be2le(value);
}

// C++ type-safe rewrites of the equivalent implementations in tarp/bits.h.
// See tarp/bits.h for details.
// The rightmost bit is considered to be at pos 1 here.
template<typename T>
T get_bits(T num, unsigned pos, unsigned nbits) {
    // get the nbits low-order bits all turned on.
    const T mask = ~((~T {0}) << nbits);

    const unsigned shift = pos - nbits;

    // apply mask and shift back down.
    const T masked = num & (mask << shift);
    return (masked >> shift);
}

// Set (value & mask) in the (mask << shift) bits of TARGET.
// Return the updated TARGET.
// Example:
//   target=0b110000
//   mask=0x3
//   shift=1
//   value=3
//   updated_target=0b110110
template<typename T, typename value_t>
constexpr T set_bits(T target, T mask, T shift, value_t value) {
    target &= ~(mask << shift);
    target |= (static_cast<T>(value) & mask) << shift;
    return target;
}

// Set 0x1 << shift in the target.
// Return the updated TARGET.
template<typename T>
constexpr T set_bit(T target, T shift) {
    target &= ~(0x1 << shift);
    target |= (0x1 << shift);
    return target;
}

// Set the (mask << shift) bits of TARGET to 0.
// Return the updated TARGET.
template<typename T>
constexpr T clear_bits(T target, T mask, T shift) {
    return set_bits(target, mask, shift, 0);
}

// Set the bit at (0x1 << shift) in the target to 0.
// Return the updated TARGET.
template<typename T>
constexpr T clear_bit(T target, T shift) {
    return set_bit(target, shift, 0);
}

// Get the (mask << shift) bits of TARGET, shifted right by SHIFT.
// Example:
//  target=0b1100
//  mask=0x3
//  shift=2
//  result: 3
template<typename T>
constexpr T get_masked_bits(T target, T mask, T shift) {
    return (target >> shift) & mask;
}

// Get the bit at (0x1 << shift) in the TARGET, shifted right by SHIFT.
template<typename T>
T get_bit(T target, T shift) {
    return (target >> shift) & 0x1;
}

}  // namespace bits
}  // namespace tarp
