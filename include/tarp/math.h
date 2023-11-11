#ifndef TARP_MATH_H
#define TARP_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "impl/math_impl.h"

#define positive(n) (n >= 0)
#define negative(n) (n < 0)

/*
 * FreeBSD source code  */
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


/*
 * Generate min and max macros for a specified type.
 * For pedantic type safety -- probably pointless so pending removal.
 */
#define define_min(type, postfix) \
    static inline type min##postfix(type a, type b){ \
        return (a < b) ? a : b; \
    }

#define define_max(type, postfix) \
    static inline type max##postfix(type a, type b){ \
        return (a > b) ? a : b; \
    }

#define define_min_and_max(type, postfix) \
    define_min(type, postfix) \
    define_max(type, postfix)

define_min_and_max(uint64_t, u64)
define_min_and_max(uint32_t, u32)
define_min_and_max(uint16_t, u16)
define_min_and_max(uint8_t, u8)
define_min_and_max(signed int, i)
define_min_and_max(unsigned int, u)

/*
 * Find a value a > v such that a ≡  b (mod m)
 */
static inline uint32_t find_congruent_value(uint32_t v, uint32_t b, uint32_t m);

/*
 * Find all primes up to, but excluding, limit.
 * For i < limit set buff[i] to 1 if i is prime and otherwise to 0.
 *
 * buff must be of at least size LIMIT
 */
void find_primes(size_t limit, char *buff);

/* print out all the primes in the range [1, limit). */
void dump_primes(size_t limit);

/*
 * find square root through binary search; x must be > 0;
 * This macro will generate a function that takes, operates, and returns
 * an unsigned integral type specified by UNSIGNED_TYPE e.g. uint64_t.
 */
#define define_sqrt(NAME, UNSIGNED_TYPE)    define_sqrt__(NAME, UNSIGNED_TYPE)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
