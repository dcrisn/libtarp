#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <iostream>

extern "C" {
#include <endian.h>
}

namespace tarp {
namespace hash {

// A template for fletcher's checksum algorithm.
// fletcher16, fletcher32, and fletcher64 all instantiate
// this template.
// The input is a buffer of bytes, potentially in network byte order
// (big endian). The result is a fletcher{16/32/64} checksum, in host byte
// order.
template<typename T, typename J, bool big_endian>
T fletcher(const std::vector<std::uint8_t> &data) {
    T sum1 = 0;
    T sum2 = 0;

    constexpr T modulus = std::numeric_limits<J>::max();
    const unsigned num_bits_in_J = sizeof(J) << 3;  // x8

    const std::size_t len = data.size();

    for (unsigned idx = 0; idx < len;) {
        J currval = 0;
        auto const n = std::min(sizeof(currval), len - idx);
        memcpy(&currval, &data[idx], n);
        // std::cerr << "copying n=" << n << " bytes from idx " << idx
        //           << std::endl;

        // NOTE: obviously, if typeof(J) == std:uint8_t, then endianness does
        // not matter. Also, the widest type of T is std::uint64_t and therefore
        // the widest type of J is std::uint32_t as per the algorithm.
        static_assert(!std::is_same_v<J, std::uint64_t>);
        if constexpr (big_endian) {
            // if in big endian, then any partial number of bytes we copied
            // must be made the most significant bytes of the type.
            const unsigned num_bits_copied = n << 3;
            currval <<= (num_bits_in_J - num_bits_copied);

            if constexpr (std::is_same_v<J, std::uint16_t>) {
                currval = be16toh(currval);
            } else if constexpr (std::is_same_v<J, std::uint32_t>) {
                currval = be32toh(currval);
            }
        }

        // std::cerr << "currval after swap: " << currval << std::endl;

        sum1 = (sum1 + currval) % modulus;
        sum2 = (sum2 + sum1) % modulus;

        idx += sizeof(currval);
    }

    return (sum2 << num_bits_in_J) | sum1;
}

template<bool big_endian>
std::uint16_t fletcher16(const std::vector<std::uint8_t> &data) {
    return fletcher<std::uint16_t, std::uint8_t, big_endian>(data);
}

template<bool big_endian>
std::uint32_t fletcher32(const std::vector<std::uint8_t> &data) {
    return fletcher<std::uint32_t, std::uint16_t, big_endian>(data);
}

template<bool big_endian>
std::uint64_t fletcher64(const std::vector<std::uint8_t> &data) {
    return fletcher<std::uint64_t, std::uint32_t, big_endian>(data);
}

}  // namespace hash
}  // namespace tarp
