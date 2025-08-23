#ifndef TARP_MATH_H
#define TARP_MATH_H

#ifdef __cplusplus
extern "C" {
#include <cmath>
#include <type_traits>
#endif

#include <stdint.h>
#include <stdlib.h>

#include "impl/math_impl.h"

#define positive(n) (n >= 0)
#define negative(n) (n < 0)

#define even(n)     (!(n & 1))
#define odd(n)      (n & 1)

/*
 * FreeBSD source code  */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/*
 * Generate min and max macros for a specified type.
 * For pedantic type safety -- probably pointless so pending removal.
 */
#define define_min(type, postfix)                     \
    static inline type min##postfix(type a, type b) { \
        return (a < b) ? a : b;                       \
    }

#define define_max(type, postfix)                     \
    static inline type max##postfix(type a, type b) { \
        return (a > b) ? a : b;                       \
    }

#define define_min_and_max(type, postfix) \
    define_min(type, postfix) define_max(type, postfix)

define_min_and_max(uint64_t, u64) define_min_and_max(uint32_t, u32)
  define_min_and_max(uint16_t, u16) define_min_and_max(uint8_t, u8)
    define_min_and_max(signed int, i) define_min_and_max(unsigned int, u)

  /*
   * Find a value a > v such that a ≡  b (mod m)
   */
  static inline uint32_t
  find_congruent_value(uint32_t v, uint32_t b, uint32_t m);

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
#define define_sqrt(NAME, UNSIGNED_TYPE) define_sqrt__(NAME, UNSIGNED_TYPE)

#ifdef __cplusplus
} /* extern "C" */
#endif


#ifdef __cplusplus
namespace tarp::math {

/*
 * Integer exponentiation. Returns base^exp.
 *
 * The algorithm is 'exponentiation by squaring' and is based on
 * the following recurrence:
 *       { x * (x^2)^((n-1)/2),  if n is odd.
 * x^n = |
 *       { x * (x^2)^(n/2),      if n is even.
 *
 * See https://en.wikipedia.org/wiki/Exponentiation_by_squaring
 *
 * The recursive version is also given for refence. This
 * is compiled out, however -- this particular implementation does
 * not use tail recursion and the iterative version is therefore
 * likely faster.
 *
 * NOTE: for large bases and/or large exponents this will end up in
 * wraparound very soon. Use an appropriate-width integer: e.g.
 * uint64_t for the template type parameter if needed.
 *
 * NOTE: does not allow negative indices e.g. x^-n since that entails
 * floating point (unless we use floor division) which is precisely
 * what this function is supposed to avoid.
 */
#if 0
template<typename T = uint32_t>
T intpow(unsigned base, unsigned exp) {
    if (exp == 0) return 1;

    T b = base * base;

    if (even(exp)) {
        return intpow(b, exp >> 1);
    }

    if (odd(exp)) {
        return base * intpow(b, (exp-1) >> 1 );
    }
}
#endif

template<typename T = uint32_t>
T intpow(T base, unsigned exp) {
    T res = 1;
    while (true) {
        if (odd(exp)) res *= base;
        exp >>= 1;
        if (exp == 0) break;
        base *= base;
    }
    return res;
}

// return True if a >= b-epislon, and a <= b+epislon.
// NOTE: T and Eps are commonly integrals and/or floats.
// This function handles any wraparound in this common
// case. The sign bit of epislon is implicitly always
// cleared as epsilon is taken to always be positive.
//
// If T (and/or epislon) is not an arithmetic type,
// then the function performs no checks on the parameters
// (hence it is the responsibility of the caller to do it)
// and does not handle any possible wraparound. The function
// merely return bool(a>=b-epsilon && a<=b+epsilon)
template<class T, class Eps>
constexpr bool equals(T a, T b, Eps epsilon) {
    if constexpr (std::is_arithmetic_v<T>) {
        using CT = std::common_type_t<T, Eps>;
        CT A = static_cast<CT>(a);
        CT B = static_cast<CT>(b);
        CT E = static_cast<CT>(epsilon);

        // Discard sign
        if (E < CT(0)) E = -E;

        if constexpr (std::is_floating_point_v<CT>) {
            if (std::isnan(A) || std::isnan(B) || std::isnan(E)) return false;

            // Optional: handle infinities sensibly (finite vs inf won't match
            // unless both inf) If you want "equal if both infinities (same
            // sign)":
            if (std::isinf(A) || std::isinf(B)) return A == B;

            return !(A < B - E) && !(A > B + E);
        } else if constexpr (std::is_integral_v<CT>) {
            // Pure integer path, computed in a widened unsigned type to avoid
            // UB.
            using U = std::make_unsigned_t<CT>;
            U Au = static_cast<U>(A);
            U Bu = static_cast<U>(B);
            U Eu = static_cast<U>(E);

            // Clamp epsilon to available headroom above and below B
            U head = std::numeric_limits<U>::max() - Bu;  // safe in unsigned
            if (Eu > head) Eu = head;
            if (Au > Bu + Eu) return false;

            // Below-side room: in unsigned, min() is 0, so this is just Bu - 0
            // = Bu
            U tail = Bu - std::numeric_limits<U>::min();
            if (Eu > tail) Eu = tail;
            if (Au < Bu - Eu) return false;

            return true;
        }
    }

    // do not check for wraparound at all; rely on the caller to
    // deal with that
    else {
        return (a >= b - epsilon && a <= b + epsilon);
    }
}


}  // namespace tarp::math
#endif /* __cplusplus */


#endif
