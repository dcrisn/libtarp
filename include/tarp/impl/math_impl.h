#ifndef TARP_MATH_IMPL_H
#define TARP_MATH_IMPL_H


static inline uint32_t find_congruent_value(uint32_t v, uint32_t b, uint32_t m)
{
    if (v == b) return v+m; // return next higher
    if (v < b)	return b;

    /* else v > b */
    return v + (m - ((v%m) - (b%m)));
}

#endif
