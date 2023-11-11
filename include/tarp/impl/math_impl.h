#ifndef TARP_MATH_IMPL_H
#define TARP_MATH_IMPL_H


static inline uint32_t find_congruent_value(uint32_t v, uint32_t b, uint32_t m)
{
    if (v == b) return v+m; // return next higher
    if (v < b)	return b;

    /* else v > b */
    return v + (m - ((v%m) - (b%m)));
}

#define define_sqrt__(NAME, UNSIGNED_TYPE)            \
UNSIGNED_TYPE NAME(UNSIGNED_TYPE x){                  \
if (x == 0) return 0;                                 \
if (x < 4) return 1;                                  \
                                                      \
UNSIGNED_TYPE l,r,m;                                  \
                                                      \
l = 0; r = x+1;                                       \
                                                      \
while(r != l+1){                                      \
    m = (l+r)>>1;                                     \
    if (m*m <= x) l = m;                              \
    else          r = m;                              \
}                                                     \
                                                      \
return l;                                             \
}

#endif
