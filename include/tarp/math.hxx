#pragma once

#include <type_traits>
#include <cstdint>
#include <cmath>

namespace tarp {
namespace math {

// Integer square root using binary search
// Returns the largest integer n where n*n <= x
template<typename T>
constexpr T sqrt(T x) {
    static_assert(std::is_unsigned_v<T>, "Type must be unsigned");

    if (x == 0) return 0;
    if (x < 4) return 1;

    T l = 0;

    // Avoids overflow: sqrt(x) <= x/2 for x >= 4
    T r = x / 2 + 1;

    // Changed condition to avoid r==l+1 edge case
    while (r > l + 1) {
        // Avoids overflow vs (l+r)/2
        T m = l + ((r - l) / 2);

        // Avoids overflow vs m*m <= x
        if (m <= x / m) {
            l = m;
        } else {
            r = m;
        }
    }

    // Final check: is r the answer?
    if (r <= x / r) {
        return r;
    }

    return l;
}

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
        // if odd
        if (exp & 0x1) res *= base;
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

}  // namespace math
}  // namespace tarp
