#pragma once

// cxx stdlib
#include <type_traits>

// c stdlib
#include <cstdint>

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

// reverse the lowest n bits in input.
// e.g. for n=4: 0b1010....1100 => 0b1010.....0011.
template<typename T>
inline T reflect_bits(T input, unsigned n) {
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


}  // namespace bits
}  // namespace tarp
