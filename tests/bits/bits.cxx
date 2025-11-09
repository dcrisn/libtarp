#include <tarp/bits.hxx>

#include <tarp/cohort.h>

#include <cstdint>
#include <iostream>
#include <limits>

prepare_test_variables();

void check_basics() {
    std::uint32_t val = 0;
    using namespace tarp::bits;

    std::cerr << "initial value: " << val << std::endl;

    // 0111 in the 4 most significant bits
    val = set_bits<std::size_t>(val, 0xf, 28, 7);
    CHECK(((val & 0xf0000000) >> 28) == 7);
    CHECK(get_masked_bits<std::size_t>(val, 0xf, 28) == 7);

    std::cerr << "after setting 0111 in the 4 ms bits: " << std::hex << val
              << std::endl;

    // set most significant bit
    val = set_bits<std::size_t>(val, 0x1, 31, 1);
    std::cerr << "after setting msb : " << std::hex << val << std::endl;

    // clear the 2 bits next to the most significant bit
    val = clear_bits<std::size_t>(val, 0x3, 29);
    std::cerr << "after clearing bits 31,30: " << std::hex << val << std::endl;

    // set the least two significant bits
    val = set_bits<std::size_t>(val, 0x3, 0, 3);
    std::cerr << "after setting bits 1,0: " << std::hex << val << std::endl;

    // clear bit 1
    val = clear_bits<std::size_t>(val, 0x2, 0);
    std::cerr << "after clearing bit 1: " << std::hex << val << std::endl;

    // now check the most significant bit is still set
    CHECK(((val & 0x80000000) >> 31) == 1);
    CHECK(get_masked_bits<std::size_t>(val, 0x1, 31) == 1);

    // check the 2 bits next to it are 0
    CHECK(get_masked_bits<std::size_t>(val, 0x3, 29) == 0);
    CHECK(((val & 0x60000000) >> 29) == 0);

    // check the least significant bit is set
    CHECK((val & 0x1) == 1);
    CHECK(get_masked_bits<std::size_t>(val, 0x1, 0x0) == 1);

    // now check the bit to the right of it is 0
    CHECK((val & 0x2) == 0);
    CHECK(get_masked_bits<std::size_t>(val, 0x1, 0x1) == 0);

    // now set all the bits in between the least significant bit
    // and the most significant bit
    val = set_bits<std::size_t>(val, 0x7ffffffe, 0, 0xffffffff);
    std::cerr << "after setting bits 30..2: " << std::hex << val << std::endl;

    // check all 32 bits are set
    CHECK(val == std::numeric_limits<std::uint32_t>::max());
    // check specifically all bits (without the two extreme bits) are set
    CHECK(get_masked_bits<std::size_t>(val, 0x7ffffffe, 0) == 0x7ffffffe);

    // now clear all bits in the middle
    val = clear_bits<std::size_t>(val, 0x7ffffffe, 0);
    std::cerr << "after clearing bits 30..2: " << std::hex << val << std::endl;

    // check they have been cleared (but two extreme bits stay set)
    CHECK(val == 0x80000001);
    // check specifically all bits (without the 2 extreme bits) are cleared
    CHECK(get_masked_bits<std::size_t>(val, 0x7ffffffe, 0) == 0);

    // and check the msb and lsb remain set
    CHECK(((val & 0x80000000) >> 31) == 0);
    CHECK(get_masked_bits<std::size_t>(val, 0x1, 31) == 1);
    CHECK(((val & 0x60000000) >> 29) == 0);
    CHECK(get_masked_bits<std::size_t>(val, 0x3, 29) == 0);
}

int main(int, const char **) {
    check_basics();

    report_test_summary();

    return 0;
}
