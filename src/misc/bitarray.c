#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <tarp/bits.h>
#include <tarp/bitarray.h>


#define MSB 0x80u  /* decimal 128: the most significant bit in a byte, turned on */
#define LSB 0x1u   /* least significant i.e. leftmost bit in a byte */

size_t get_required_bitr_size(uint32_t num_bits){
    size_t sz = sizeof(size_t);  /* first member */
    sz += num_bits / BITS_IN_BYTE;   /* variable array */
    sz += num_bits % 8 ? 1 : 0; /* if num_bits is not a multiple of 8, another byte is needed */

    return sz;
}

struct bitarray *Bitr_new(uint32_t num_bits, bool all_ones){
    assert(num_bits > 0);
    size_t sz = get_required_bitr_size(num_bits);

    struct bitarray *new = calloc(1, sz);
    assert(new);
    new->size = sz - sizeof(new->size); /* only the size of the variable array */

    if (all_ones){
        for(uint32_t i = 0; i < new->size; ++i){
            new->bits[i] = 0xFFu;
        }
    }

    return new;
}

void Bitr_destroy(struct bitarray **bitr){
    assert(bitr && *bitr);
    free(*bitr);
    *bitr = NULL;
}

uint32_t Bitr_size(const struct bitarray *bitr){
    assert(bitr);
    return bitr->size * BITS_IN_BYTE;
}

/*
 * Return true if n < bytes*BITS_IN_BYTE, else false.
 * Specifiying n means we want to act on the value
 * at index [n-1]. In an n-bit array, n-1 is therefore
 * the greatest index available and n is the greatest
 * allowable value for the param. */
static inline int is_within_range(uint32_t bytes, uint32_t n){
    return (n > bytes * BITS_IN_BYTE) ? false : true;
}

int Bitr_set(struct bitarray *bitr, uint32_t n){
    assert(bitr);
    if (!is_within_range(bitr->size, n)) return -1;

    --n;  /* bit n goes into bitarray[n-1] */
    uint32_t byte = n / BITS_IN_BYTE;  /* the index of the byte in the actual array */
    uint32_t idx = n % BITS_IN_BYTE;   /* the index of the bit within the byte */

    bitr->bits[byte] |= (MSB >> idx);

    return 0;
}

int Bitr_clear(struct bitarray *bitr, uint32_t n){
    assert(bitr);
    if (!is_within_range(bitr->size, n)) return -1;

    --n;  /* bit n goes into bitarray[n-1] */
    uint32_t byte = n / BITS_IN_BYTE;  /* the index of the byte in the actual array */
    uint32_t idx = n % BITS_IN_BYTE;   /* the index of the bit within the byte */

    bitr->bits[byte] &= ~(MSB >> idx);

    return 0;
}

int Bitr_toggle(struct bitarray *bitr, uint32_t n){
    assert(bitr);
    if (!is_within_range(bitr->size, n)) return -1;

    --n;  /* bit n goes into bitarray[n-1] */
    uint32_t byte = n / BITS_IN_BYTE;  /* the index of the byte in the actual array */
    uint32_t idx = n % BITS_IN_BYTE;   /* the index of the bit within the byte */

    bitr->bits[byte] ^= (MSB >> idx);

    return 0;
}

int Bitr_get(struct bitarray *bitr, uint32_t n){
    assert(bitr);
    if (!is_within_range(bitr->size, n)) return -1;

    --n;  /* bit n goes into bitarray[n-1] */
    uint32_t byte = n / BITS_IN_BYTE;  /* the index of the byte in the actual array */
    uint32_t idx = n % BITS_IN_BYTE;   /* the index of the bit within the byte */

    // printf("looking at byte=%i, which is 0x%02x; need bit %i\n", byte, bitr->bits[byte], idx+1);

    /* check if the nth most significant/leftmost bit is turned on or off,
     * then shift it to the left to the least significant/rightmost position: 0 or 1 */
    return ( bitr->bits[byte] & (MSB >> idx) ) >> (7 - idx);
}
