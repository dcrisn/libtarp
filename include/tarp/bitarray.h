#ifndef TARP_BITARRAY_H
#define TARP_BITARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

/*****************************************************************************
 * Bit array data structure.                                                 |
 *                                                                           |
 * The bits as well as the bytes are thought of as being numbered from 1,    |
 * from right (lower address in the bit array) to left (higher address in    |
 * the bit array), with increasing significance. IOW the storage is          |
 * little endian.                                                            |
 *                                                                           |
 *  uint8_t p[3] = {0xff, 0xff, 0xff};                                       |
 *                                                                           |
 *         byte 3   byte 2   byte 1                                          |
 *          |        |        |                                              |
 *   p+2    11111111 11111111 11111111  p+0                                  |
 *         |                 |       |                                       |
 *       bit 24            bit 8    bit 1                                    |
 *                                                                           |
 * ------------------------------------------------------------------------  |
 * Functions marked as @mutator do change the bit array argument.            |
 * If the user would like a copy to be returned instead and leave the        |
 * original unchanged, then they should call clone() on the original and     |
 * call the mutator on the copy.                                             |
 *                                                                           |
 * Functions that return a dynamically allocated bit array expect the user   |
 * to call Bitr_destroy() when the bitarray is not longer needed.            |
 * ------------------------------------------------------------------------  |
 *                                                                           |
 * Runtime complexity characteristics                                        |
 * ------------------------------------                                      |
 *                                                                           |
 *  - '.'  means 'same as previous'.                                         |
 *                                                                           |
 * Bitr_new                     O(n) // n = width of array being created     |
 * Bitr_destroy                 O(1) // .                                    |
 * Bitr_width                   O(1) // .                                    |
 * Bitr_frombuff                O(n) // n = width of input being copied from |
 * Bitr_clone                   O(n) // .                                    |
 * Bitr_{set,clear,toggle,get}  O(1) //                                      |
 * Bitr_fromu{8,16,32,64}       O(1) // n = (fixed) number of input bytes    |
 * Bitr_tou{8,16,32,64}         O(1) // n = (fixed) number of output bytes   |
 * Bitr_{setn,clearn,togglen}   O(n) // n = the number of bits to operate on |
 * Bitr_{any,all,none}          O(n) // n = width of the bit array           |
 * Bitr_{band,bor,bxor,bnot}    O(n) // .                                    |
 * Bitr_{l,r}shift              O(n) // .                                    |
 * Bitr_{l,r}{push,pop}         O(n) // .                                    |
 * Bitr_reverse                 O(n) // .                                    |
 * Bitr_tostring                O(n) // .                                    |
 * Bitr_fromstring              O(n) // n = the length of the bitstring      |
 * Bitr_slice                   O(n) // n = width of the slice               |
 * Bitr_repeat                  O(n) // n = width or number of repetitions   |
 * Bitr_join                    O(n) // n = width of the bit array(s)        |
 * Bitr_equal                   O(n) // .                                    |
 *                                                                           |
 *---------------------------------------------------------------------------/
 */
struct bitarray{
    size_t  size;  /* in bytes; for convenience, to minimize conversion calls */
    size_t width;  /* in bits; might not be a byte multiple */
    uint8_t bytes[];
};

/*
 * Allocate new bit array big enough to hold nbits > 0.
 * If one=True, all bits in the new array will be
 * initialized to 1, otherwise 0. */
struct bitarray *Bitr_new(size_t nbits, bool one);

/*
 * Deallocate bitr and set the pointer to NULL */
void Bitr_destroy(struct bitarray **bitr);

/*
 * Get the total number of bits the bit array can hold */
size_t Bitr_width(const struct bitarray *bitr);



/*
 * Initialize bitarray from buffer.
 * @mutator
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
 * @mutator
 *
 * Each character in bitstring must be either a '0' or a '1', with the leftmost
 * character representing the most significant bit. A *non-empty* substring used
 * as a separator to be ignored can be specified via 'sep'. Otherwise 'sep'
 * should be NULL.
 *
 * NULL is returned for an invalid bitstring or if the width of bitr
 * (if not NULL) is insufficient.
 *
 * See Bitr_frombuff FMI.
 */
struct bitarray *Bitr_fromstring(struct bitarray *bitr,
        const char *bitstring, const char *sep);

/*
 * Initialize bit array from the value of the specified integral type.
 * @mutator
 *
 * If bitr is NULL, it's allocated. If bitr is not NULL, it must be at least as
 * wide as the integral type.
 * See Bitr_frombuff FMI.
 */
struct bitarray *Bitr_fromu8(struct bitarray *bitr, uint8_t val);
struct bitarray *Bitr_fromu16(struct bitarray *bitr, uint16_t val);
struct bitarray *Bitr_fromu32(struct bitarray *bitr, uint32_t val);
struct bitarray *Bitr_fromu64(struct bitarray *bitr, uint64_t val);

/*
 * Load the 8/16/32/64 lower-order bits from bitr into the specified type.;
 * The request is always safe: only as many bytes as will fit in TYPE *and*
 * are in the bit array will be read and loaded */
uint8_t Bitr_tou8(const struct bitarray *bitr);
uint16_t Bitr_tou16(const struct bitarray *bitr);
uint32_t Bitr_tou32(const struct bitarray *bitr);
uint64_t Bitr_tou64(const struct bitarray *bitr);

/*
 * Return a (deep) copy of the bit array */
struct bitarray *Bitr_clone(const struct bitarray *bitr);

/*
 * Return a new bit array that is initialized by repeating bitr n > 0 times. */
struct bitarray *Bitr_repeat(
        const struct bitarray *bitr,
        size_t n
        );

/*
 * Append a to b. If b has bits 1..n, a's bits will come after n. That is, a's
 * least significant bit is more significant than b's most significant bit.
 * B and A themselves remains unchanged and a new bit array is returned. */
struct bitarray *Bitr_join(struct bitarray *a, const struct bitarray *b);

/*
 * Shift the bits in the array left or right.
 *
 * --> n [<= width]
 * The number of bit positions to shift by.
 *
 * --> rotate
 * If true, the shift is rotational such that bits that fall off one end
 * re-emerge at the other.
 */
void Bitr_lshift(struct bitarray *bitr, size_t n, bool rotate);
void Bitr_rshift(struct bitarray *bitr, size_t n, bool rotate);

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
 * @mutator (set,get,toggle)
 *
 * Bitr_get returns 1 if the bit is turned on, else 0 if it is turned off.
 *
 * Bitr_setval takes an explicit bitval value, which must be either 1 or 0.
 * <-- return
 *     0 on success, -(ERROR_OUTOFBOUNDS) when (!(0 < pos <= width).
 */
int Bitr_set(struct bitarray *bitr, size_t pos);
int Bitr_setval(struct bitarray *bitr, size_t pos, int bitval);
int Bitr_get(const struct bitarray *bitr, size_t pos);
int Bitr_clear(struct bitarray *bitr, size_t pos);
int Bitr_toggle(struct bitarray *bitr, size_t pos);


/*
 * Set, get, clear, or toggle nbits from pos (inclusive), to the right.
 * @mutator
 *
 * NOTE
 * - pos is 1-based, not 0 based (see top of file FMI).
 * - if pos is 0, then the default is used (most significant bit position)
 * - if nbits is 0, then the default is used (nbits, i.e. start of the bitarray)
 *
 * <-- return
 * 0 = Success
 * -(ERROR_OUTOFBOUNDS) = nbits > pos or  !(0 <= pos <= width of the bit array)
 */
int Bitr_setn(struct bitarray *bitr, uint32_t pos, size_t nbits);
int Bitr_clearn(struct bitarray *bitr, uint32_t pos, size_t nbits);
int Bitr_togglen(struct bitarray *bitr, uint32_t pos, size_t nbits);

/*
 * Create and return a new bit array that is a [start, end) slice of <bitr>.
 * 0 for the value of either start end means the lower/bound is unspecified.
 * Therefore (0,0) represents a slice the size of bitr. The slice begins at
 * the low-order end of the bitarray and runs toward the high-order end.
 *
 * NOTE indexing is 1-based.
 *
 * - The following are expected:
 *   start < end
 *   0 <= start,end <= width+1
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
 * @mutator
 *
 * <-- return
 * 0 = Success
 * ERROR_INVALIDVALUE = a and b are not of the same size.
 */
int Bitr_bor(struct bitarray *a, const struct bitarray *b);
int Bitr_band(struct bitarray *a, const struct bitarray *b);
int Bitr_bxor(struct bitarray *a, const struct bitarray *b);
int Bitr_bnot(struct bitarray *a);

/*
 * True if a == b, else false. A and b must be of the same width.
 */
bool Bitr_equal(const struct bitarray *a, const struct bitarray *b);

/*
 * Return a dynamically allocated string that represents the bitarray as a
 * sequence of bits from most (left) to least (right) significant.
 * If split_every is > 0, then the bits are split in groups of the specified
 * size (working from low-order end), separated by sep, which must *not* be
 * NULL (may only be NULL if split_every is 0).
 *
 * The user must free() the string when no longer needed.
 */
char *Bitr_tostring(struct bitarray *bitr, size_t split_every, const char *sep);

#ifdef __cplusplus
}
#endif

#endif
