#ifndef TARP_BITARRAY_H
#define TARP_BITARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/*
 * Bit array */
struct bitarray{
    size_t size;
    uint8_t bits[];
};

/*
 * Allocate new bit array big enough to hold num_bits;
 * If all_ones=True, all bits in the new array will be
 * initialized as 1s (set), otherwise as 0s (unset) */
struct bitarray *Bitr_new(uint32_t nbits, bool all_ones);

/*
 * Deallocate bitr */
void Bitr_destroy(struct bitarray **bitr);

/*
 * Get the total number of bits the bit array can hold */
uint32_t Bitr_size(const struct bitarray *bitr);

/*
 * Set bitr[pos-1].
 * <-- return
 *     0 on success, -1 on failure (out of bounds).
 */
int Bitr_set(struct bitarray *bitr, uint32_t pos);

/*
 * Unset bitr[pos-1].
 * <-- return
 *     0 on success, -1 on failure (out of bounds).
 */
int Bitr_clear(struct bitarray *bitr, uint32_t pos);

/*
 * Toggle bitr[pos-1].
 * <-- return
 *     0 on success, -1 on failure (out of bounds).
 */
int Bitr_toggle(struct bitarray *bitr, uint32_t pos);

/*
 * Get bitr[pos-1].
 * <-- return
 *     -1 on failure (out of bounds), else 1 if bitr[pos-1]
 *     is turned on, else 0 if bitr[pos-1] is turned off.
 */
int Bitr_get(struct bitarray *bitr, uint32_t pos);

/*
 * Todo:
 * add functions to:
 * 1) set,get,toggle,clear n bits at a time (setn, getn, clearn)
 * 2) shift bit from pos, in a certain direction,
 * 3) shift-rotate bits
 * 4) populate bit array from byte aray
 * 5) print representation of the bit array
 */

#ifdef __cplusplus
}
#endif

#endif
