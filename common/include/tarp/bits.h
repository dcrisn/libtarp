#ifndef MO_BITS_H
#define MO_BITS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* todo : I've already written a bitarray implementation, making 
 * many of the routines here superfluous; remove them. 
 * only keep generic functions operating on a single uint64_t or whatever; 
 * perhaps implement these as macros instead */

enum {OFF_BIT, ON_BIT};

/*
 * This module offers multiple functions (rather than CPP macros)
 * that deal with clearing, setting, and getting the value of a
 * a bit, either inside a data type (***) or a bit array of such types.
 *
 *              * * *
 * The Data type almost all these functions operate on is uint32_t; 
 * The normal word size is 4 bytes - as provided by the standard int type.
 * But while an int is often 4 bytes, it's not GUARANTEED to be, hence
 * the motivation for using uint32_t.
 * 
 * Bits are counted/numbered starting from 1, from right to left, i.e.
 * from least to most significant. Therefore in a 32-bit int (uint32_t),
 * the rightmost bit is bit 1 and the leftmost bit is bit 32.
 *
 * The set_bits, clear_bits, and get_bits functions operate on multiple
 * bits at a time, unlike the set_bit, is_bit_on and clear_bit functions. 
 * The type they operate on is also different: size_t, which can
 * accomodate any size on the system.
 * The 'bits' also refer to the bits in terms of an OFFSET: that is to say, 
 * for these functions the right most bit is though of as the bit at
 * offset 0, while their 'bit' counterparts think of it as 'bit 1'.
 */

/* 
 * Output should be the same type as the input of bigger, otherwise it will
 * overflow.
 */
#define TARP_REVBITS(input, output)  \
    do { \
        output = 0; \
        size_t bits = (sizeof(input)* 8); \
        while (bits--) { \
            output = (output << 1) | (input & 1);  \
            input >>= 1; \
        } \
    } while (0);

#define TARP_PRINTBITS(input, bits) \
        do { \
            uint32_t n = bits; \
            while (n--) { \
                printf("%u", (input >> n) & 1); \
            } \
            puts(""); \
        } while (0)

// test bit BIT in NUM
bool Tarp_is_bit_on(uint32_t num, unsigned int bit);

// set bit BIT in NUM
uint32_t Tarp_set_bit(uint32_t num, unsigned int bit);

// clear bit BIT in NUM
uint32_t Tarp_clear_bit(uint32_t num, unsigned int bit);

// test bit BIT in bitarray BITARRAY
bool Tarp_array_is_bit_on(uint32_t bitarray[], size_t array_size, unsigned int bit);

// set bit BIT in bitarray BITARRAY
void Tarp_array_set_bit(uint32_t bitarray[], size_t array_size, unsigned int bit);

// clear bit BIT in bitarray BITARRAY
void Tarp_array_clear_bit(uint32_t bitarray[], size_t array_size, unsigned int bit);

// set HOW_MANY bits in num OFFSET bits from the right;
// rightmost bit is at offset 0, second bit at offset 1 etc
size_t Tarp_set_bits(size_t num, unsigned int offset, unsigned int how_many);

// clear HOW_MANY bits in num OFFSET bits from the right;
// rightmost bit is at offset 0, second bit at offset 1 etc
size_t Tarp_clear_bits(size_t num, int offset, int how_many);

// get HOW_MANY bits in num OFFSET bits from the right;
// rightmost bit is at offset 0, second bit at offset 1 etc
// e.g. returns 15 for a number where the 4 rightmost bits
// are all 1
size_t Tarp_get_bits(size_t num, int offset, int how_many);

// return true if bit number BIT is 1, else false if 0
// NOTE: counting starts from 1, from the left
bool Tarp_get_string_bit(char *str, unsigned int n);


// return true if bit number BIT is 1, else false if 0
// NOTE: counting starts from 1, from the left
bool Tarp_get_int_bit(uint32_t i, uint32_t n);
#endif
