#ifndef TARP_BITARRAY_H
#define TARP_BITARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/***************************************************************************
 * Bit array data structure.                                               |
 *                                                                         |
 * The bits as well as the bytes are thought of as being numbered from 1,  |
 * from right (lower address in the bit array) to left (higher address in  |
 * the bit array), with increasing significance. IOW the storage is        |
 * little endian.                                                          |
 *                                                                         |
 *  uint8_t p[3] = {0xff, 0xff, 0xff};                                     |
 *                                                                         |
 *         byte 3   byte 2   byte 1                                        |
 *          |        |        |                                            |
 *   p+2    11111111 11111111 11111111  p+0                                |
 *         |                 |       |                                     |
 *       bit 24            bit 8    bit 1                                  |
 *                                                                         |
 * Mutator functions do change the bit array argument. If the user would   |
 * like a copy to be returned instead and leave the original unchanged,    |
 * then they should call clone() on the original and call the mutator on   |
 * the copy.                                                               |
 *------------------------------------------------------------------------/
 */
struct bitarray{
    size_t  size;  /* in bytes */
    size_t width;  /* in bits; might not be a byte multiple */
    uint8_t bytes[];
};

/*
 * Allocate new bit array big enough to hold nbits > 0.
 * If all_ones=True, all bits in the new array will be
 * initialized to 1, otherwise 0. */
struct bitarray *Bitr_new(size_t nbits, bool all_ones);

/*
 * Initialize bitarray from buffer.
 *
 * If bitr is NULL, this function allocates a bit array big enough
 * to hold buffsz bytes.
 *
 * If bitr is *not* NULL, then its *exact width*, converted (and truncated) to
 * bytes, must be >= buffsz.
 *
 * The bitarray is initialized with the bits from <buffer>, starting at the
 * low-order end. If bitr is *not* NULL and its width > the width of the buffer,
 * then the excess bits are not modified.
 *
 * <-- return
 * <bitr> (or a dynamically allocated bitarray, if NULL) initialized as explained
 * above. If <bitr> is not NULL, then NULL is returned if not wide enough.
 */
struct bitarray *Bitr_frombuff(
        struct bitarray *bitr,
        const uint8_t *buffer,
        size_t buffsz);

/*
 * Initialize bit array with the contents of <bitstring>.
 * Each character in bitstring must be either a '0' or a '1', with the leftmost
 * character representing the most significant bit. NULL is returned for an
 * invalid bitstring or if the width of bitr (if not NULL) is insufficient.
 *
 * See Bitr_frombuff FMI.
 */
struct bitarray *Bitr_fromstring(struct bitarray *bitr, const char *bitstring);

/*
 * Initialize bit array from the value of the specified integral type.
 * If bitr is NULL, it's allocated. If bitr is not NULL, it must be at least as
 * wide as the integral type.
 * See Bitr_frombuff FMI.
 */
struct bitarray *Bitr_fromu8(struct bitarray *bitr, uint8_t val);
struct bitarray *Bitr_fromu16(struct bitarray *bitr, uint16_t val);
struct bitarray *Bitr_fromu32(struct bitarray *bitr, uint32_t val);
struct bitarray *Bitr_fromu64(struct bitarray *bitr, uint64_t val);

/*
 * Return a (deep) copy of the bit array */
struct bitarray *Bitr_clone(const struct bitarray *bitr);

/*
 * Return a new bit array that is initialized by repeating bitr n > 0 times.
 * If leading zeroes are to be ignored
 */
struct bitarray *Bitr_repeat(
        const struct bitarray *bitr,
        size_t n
        );

/*
 * Append b to a. If a has bits 1..n, b's bits will come after n.
 * B and A themselves remains unchanged and a new bit array is returned. */
struct bitarray *Bitr_join(struct bitarray *a, const struct bitarray *b);

/*
 * Deallocate bitr and set the pointer to NULL */
void Bitr_destroy(struct bitarray **bitr);

/*
 * Get the total number of bits the bit array can hold */
size_t Bitr_width(const struct bitarray *bitr);

/*
 *
 * Shift the bits in the array left or right.
 *
 * --> n [<= width]
 * The number of bit positions to shift by.
 *
 * --> rotate
 * If true, the shift is rotational such that bits that fall off one end
 * re-emerge at the other.
 *
 * <-- return
 * 0 = Success
 * 1 = invalid pos value (> width)
 */
int Bitr_lshift(struct bitarray *bitr, size_t n, bool rotate);
int Bitr_rshift(struct bitarray *bitr, size_t n, bool rotate);

/*
 * Push/pop a bit at the left (most significant)/right (least significant) end.
 * @mutator
 *
 * The bit pushed is 1 if on=true, else 0. The rest of the bits are shifted
 * left/right by one
 */
void Bitr_lpush(struct bitarray *bitr, bool on);
void Bitr_rpush(struct bitarray *bitr, bool on);
int Bitr_lpop(struct bitarray *bitr);
int Bitr_rpop(struct bitarray *bitr);

/*
 * Reverse the sequence of bits in the array.
 * @mutator
 *
 * A pointer to bitr is returned for convenience.
 */
struct bitarray *Bitr_reverse(struct bitarray *bitr);

/*
 * Set, get, clear, or toggle the bit at bitr[pos-1].
 *
 * Bitr_get returns 1 if the bit is turned on, else 0 if it is turned off.
 *
 * <-- return
 *     0 on success, 1 on failure (out of bounds: !(0 < pos <= width).
 */
int Bitr_set(struct bitarray *bitr, size_t pos);
int Bitr_get(const struct bitarray *bitr, size_t pos);
int Bitr_clear(struct bitarray *bitr, size_t pos);
int Bitr_toggle(struct bitarray *bitr, size_t pos);


/*
 * Set, get, clear, or toggle nbits from bitr[pos-1] (inclusive), to the right.
 *
 * Bitr_getn returns returns a
 *
 * <-- return
 * 0 = Success
 * 1 = pos > width (out of bounds)
 * 2 = nbits > pos (out of bounds)
 */
int Bitr_setn(struct bitarray *bitr, uint32_t pos, size_t nbits);
int Bitr_clearn(struct bitarray *bitr, uint32_t pos, size_t nbits);
int Bitr_togglen(struct bitarray *bitr, uint32_t pos, size_t nbits);

/* Create and return a new bit array that is a [start, end) slice of <bitr>.
 * 0 for the value of start or end means the lower/bound is unspecified. Therefore
 * (0,0) represents a slice the size of bitr.
 */
struct bitarray *Bitr_slice(const struct bitarray *bitr, size_t start, size_t end);

/*
 * Test if any, all, or none of the bits in the array are set. */
bool Bitr_any(const struct bitarray *bitr);
bool Bitr_all(const struct bitarray *bitr);
bool Bitr_none(const struct bitarray *bitr);

/*
 * Perform a <x> b or <x> is the specified binary operation, and store the result
 * in a. a and b *must* be of the same size.
 *
 * <-- return
 * 0 = Success
 * 1 = a and b are not of the same size.
 */
int Bitr_bor(struct bitarray *a, const struct bitarray *b);
int Bitr_band(struct bitarray *a, const struct bitarray *b);
int Bitr_bxor(struct bitarray *a, const struct bitarray *b);

/*
 * True if a == b, else false. A and b must be of the same width.
 */
bool Bitr_equal(const struct bitarray *a, const struct bitarray *b);

/*
 * Return a dynamically allocated string that represents the bitarray as a
 * sequence of bits from most (left) to least (right) significant.
 * If split_every is > 0, then the bits are split in groups of the specified
 * size, separated by sep, which must *not* be NULL (may only be NULL if
 * split_every is 0).
 *
 * The user must free() the string when no longer needed.
 */
char *Bitr_tostring(struct bitarray *bitr, int split_every, const char *sep);

#ifdef __cplusplus
}
#endif

#endif
