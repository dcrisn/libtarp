# bitarray

A C bit array implementation

## Purpose

A bit array can serve as a space-saving substitute for a boolean array.
A boolean is typically 1 byte. 8 times less space occupied begins to
become more significant the larger the array is.

The real advantage of using a bit array, however, is a consistent API:
attempts to index out of bounds will returns an error rather than
crash the program, intent is more clearly communicated, and it
reduces the chance of making mistakes when performing bitwise
operations to set, clear, and toggle bits.

Additionally, shift-register -like methods are provided for shifting bits in
either direction and rotationally.

## Usage overview

See also `include/tarp/bitarray.h` for a good overview.

```C
/* initialize bit array big enough to hold n bits with all bits turned on */
struct bitr *b = Bitr_new(170, true);

/* initialize bit array big enough to hold n bits with all bits turned off */
struct bitr *b = Bitr_new(170, false);

/* set bit 17 */
Bitr_set(b, 17);

/* clear bit 17 */
Bitr_clear(b, 17);

/* toggle bit 7141 */
Bitr_toggle(b, 7141);

/* test bit 1 */
Bitr_test(b, 1);     // should return 1 if the bit is set, else 0 if unset

/* deallocate bit array */
Bitr_destroy(&b);

```
**NOTE**
In all cases, `-1` will be returned if the position is out of bounds. For
example, when attempting to set bit 17 in a 10-bit array, or asking for 8 bits
when at position 5.
