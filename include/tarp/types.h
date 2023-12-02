#ifndef TARP_TYPES_H
#define TARP_TYPES_H

/*
 * This header file declares various miscellaneous types.
 * They should be used whenever possible for consistency
 * across the code base.
 */

/*
 * A 2-tuple of strings. */
struct string_pair {
    const char *first;
    const char *second;
};

#endif
