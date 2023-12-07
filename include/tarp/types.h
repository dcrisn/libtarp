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

/*
 * tuples of integers */
struct int_pair {
    int first;
    int second;
};

struct int_triple {
    int first;
    int second;
    int third;
};

/*
 * tuples of pointers */
struct ptr_pair {
    void *first;
    void *second;
};

struct ptr_triple {
    void *first;
    void *second;
    void *third;
};

struct ptr_4tup {
    void *first;
    void *second;
    void *third;
    void *fourth;
};

#endif
