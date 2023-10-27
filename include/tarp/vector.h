#ifndef TARP_VECTOR_H
#define TARP_VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "vector_impl.h"

/***************************************************************************
 * Vector (dynamic array)                                                  |
 *                                                                         |
 * === API ===                                                             |
 *---------------------                                                    |
 * The following functions are provided. Most of them are automatically    |
 * generated when the define_vector() macro is invoked. The arguments to   |
 * the macro are:                                                          |
 *                                                                         |
 *  - TYPE: the full type of the data the vector is logically supposed to  |
 *    store and that its associated functions return or take as arguments. |
 *    E.g. uint32_t, struct mystruct, void * etc.                          |
 *                                                                         |
 *  - SHORTNAME is meant to be a more convenient name (to type out) than   |
 *    the type and will be appended to the functions below in place of     |
 *    <name>. E.g. Vect_front_u8, Vect_pushb_db etc.                       |
 *                                                                         |
 *    Strictly speaking this is only necessary if multiple vector instances|
 *    are defined (geneated) in the same compilation unit. Otherwise the   |
 *    SHORTNAME argument could be left emty e.g. the invocation could be   |
 *    define_vector(,int)                                                  |
 *                                                                         |
 * vector *Vect_new<name>(size_t capacity);                                |
 * void Vect_destroy(vector **v);                                          |
 * void Vect_clear(vector *v);                                             |
 * bool Vect_isempty(const vector *v);                                     |
 * size_t Vect_count(const vector *v);                                     |
 * int Vect_pushb<name>(vector *v, TYPE value);                            |
 * TYPE Vect_popb<name>(vector *v);                                        |
 * TYPE Vect_get<name>(const vector *v, size_t pos);                       |
 * TYPE *Vect_getptr<name>(const vector *v, size_t pos);                   |
 * void Vect_set<name>(vector *v, size_t pos, TYPE value);                 |
 * TYPE Vect_front<name>(const vector *v);                                 |
 * TYPE Vect_back<name>(const vector *v);                                  |
 * TYPE *Vect_begin<name>(const vector *v);                                |
 * TYPE *Vect_end<name>(const vector *v);                                  |
 * void Vect_resize<name>(vector *v, size_t count, TYPE fill);             |
 * int Vect_insert<name>(vector *v, size_t pos, TYPE value);               |
 * void Vect_remove<name>(vector *v, size_t pos);                          |
 *                                                                         |
 * NOTES:                                                                  |
 *  - indexing (as passed through the 'pos' argument to various functions) |
 *    is 0-based. The elements in the vector ar at positions               |
 *    [0 .. Vect_count()-1].                                               |
 *  - out-of-bounds indexing, NULL pointers or other illegal inputs that   |
 *    don't meet the preconditions cause *undefined behavior* and a very   |
 *    likely crash. No error code is returned and inputs are expected to   |
 *    be valid.                                                            |
 *    The logic behind this is that returning an error code not only makes |
 *    the function interfaces more cumbersome to use, but it's likely to   |
 *    be ignored and then kick the problem down the road by returning      |
 *    invalid unchecked outputs. Instead of code to check the error code,  |
 *    the user should write code to validate the inputs.                   |
 *    Don't pass out-of-bounds index values.                               |
 *                                                                         |
 * === RUNTIME COMPLEXITY CHARACTERISTICS ===                              |
 *---------------------------------------------                            |
 * The vector has the characteristics of a normal bounded array with the   |
 * difference that its size grows and shrinks automatically to accomodate  |
 * insertions and deletions.                                               |
 *                                                                         |
 * Vect_new                     O(1)                                       |
 * Vect_destroy                 O(1)                                       |
 * Vect_clear                   O(1)                                       |
 * Vect_isempty                 O(1)                                       |
 * Vect_count                   O(1)                                       |
 * Vect_maxcap                  O(1)                                       |
 * Vect_pushb                   O(1) // amortized                          |
 * Vect_popb                    O(1) // amortized                          |
 * Vect_get                     O(1)                                       |
 * Vect_getptr                  O(1)                                       |
 * Vect_set                     O(1)                                       |
 * Vect_front                   O(1)                                       |
 * Vect_back                    O(1)                                       |
 * Vect_begin                   O(1)                                       |
 * Vect_end                     O(1)                                       |
 * Vect_resize                  O(n)                                       |
 * Vect_insert                  O(n)                                       |
 * Vect_remove                  O(n)                                       |
 *                                                                         |
 *************************************************************************/

#define define_vector(SHORTNAME, TYPE)           \
        define_vector_new(SHORTNAME, TYPE)       \
        define_vector_pushb(SHORTNAME, TYPE)     \
        define_vector_popb(SHORTNAME, TYPE)      \
        define_vector_get(SHORTNAME, TYPE)       \
        define_vector_getptr(SHORTNAME, TYPE)    \
        define_vector_set(SHORTNAME, TYPE)       \
        define_vector_front(SHORTNAME, TYPE)     \
        define_vector_back(SHORTNAME, TYPE)      \
        define_vector_begin(SHORTNAME, TYPE)     \
        define_vector_end(SHORTNAME, TYPE)       \
        define_vector_insert(SHORTNAME, TYPE)    \
        define_vector_remove(SHORTNAME, TYPE)    \
        define_vector_resize(SHORTNAME, TYPE)    \


/*
 * Return number of items currently in the vector.
 * v *must not* be NULL. */
size_t Vect_count(const vector *v);

/*
 * Release all vector-associated resources and set the *v pointer to NULL.
 * v *must not* be NULL.
 *
 * NOTE
 * if the user elements are dynamically allocated pointers they are *not*
 * freed. Only the vector slots storing those pointers will be freed.
 * Therefore in this case before calling destroy() the user should iterate
 * over the vector and deallocate dynamically allocated elements as necessary.
 */
void Vect_destroy(vector **v);

/*
 * True if the vector is empty, else False.
 * v *must not* be NULL. */
bool Vect_isempty(const vector *v);

/*
 * Set the count of items currently in the vector to 0.
 *
 * v *must not* be NULL.
 *
 * The capacity of v remains the same, i.e. any internal memory is not freed.
 * Shrinking/growing will happen on insertions and deletions according to the
 * usual algorithm.
 *
 * NOTE
 * if the user elements are pointers to dynamically allocated memory this
 * will make them inaccessible through the vector! IOW their memory is not
 * released potentially causing memory leak. See the note above Vect_destroy
 * FMI.
 */
void Vect_clear(vector *v);

/*
 * Return the theoretical maximum possible number of elements of elemsz each
 * that the vector would be able to store */
size_t Vect_maxcap(size_t elemsz);


/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * These functions are generated by the define_vector macro. See notes above.
 *^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
#if 0
/*
 * Create and return a new vector with the specified initial capacity.
 *
 * If the request cannot be satisfied (due to the excessive values for
 * the capacity and/or the size of each element), NULL is returned.
 */
vector *Vect_new<name>(size_t capacity);

/*
 * Insert the given value at the end of the vector v.
 *
 * v *must not* be NULL.
 *
 * The insertion may cause the vector to grow. This fails if the new
 * capacity is > Vect_maxcap(). In this case ERROR_OUTOFBOUNDS is returned,
 * the element is not pushed and the vector is not grown.
 */
int Vect_pushb<name>(vector *v, TYPE value, int *errnum);

/*
 * Remove the last element in the vector and return it.
 *  - v must be non-NULL and non-empty. */
TYPE Vect_popb<name>(vector *v);

/*
 * Get the element at position pos in vector.
 * - pos is 0-based and must be < Vect_count()
 * - v must not be NULL */
TYPE Vect_get<name>(const vector *v, size_t pos);

/*
 * Get a pointer to the element at position pos in vector.
 *  - pos must be < Vect_count()
 *  - v must not be NULL */
TYPE *Vect_getptr<name>(const vector *v, size_t pos);

/*
 * Set the element at position pos in vector to value.
 * - v must not be NULL
 * - pos must be < Vect_count() */
void Vect_set<name>(vector *v, size_t pos, TYPE value);

/*
 * Return the first element in the non-null and non-empty vector v. */
TYPE Vect_front<name>(const vector *v);

/*
 * Return the last element in the non-null and non-empty vector v */
TYPE Vect_back<name>(const vector *v);

/*
 * Return a pointer to the first element in the vector.
 *  - v must be non-NULL
 *  - if v is empty, returns the same value as Vect_end() */
TYPE *Vect_begin<name>(const vector *v);

/*
 * Return a pointer to *past* the last element in the vector.
 *  - v must be non-NULL */
TYPE *Vect_end<name>(const vector *v);

/*
 * Change the vector capacity such that count elements can be accomodated.
 * - v must be non-NULL
 *
 * NOTE
 * The actual capacity of the vector is only changed if necessary.
 * If count < the current count, the count is reduced to <count>.
 * If count > the current capacity, new elements are inserted, each
 * with value FILL.
 * If count == the current count, nothing is changed. */
void Vect_resize<name>(vector *v, size_t count, TYPE fill);

/*
 * Insert an element with the specified value at position pos in the vector.
 *  - v must be non-NULL
 *  - pos must be <= Vect_count(). If pos==0, the insertion is at the front of
 *    the vector. If pos==Vect_count(), the insertion is at the back of the
 *    vector, equivalent to Vect_pushb, and the same return value is returned
 *    (otherwise the return value is always 0).
 */
int Vect_insert<name>(vector *v, size_t pos, TYPE value);

/*
 * Remove the element at position pos in the vector.
 *  - v must be non-NULL
 *  - pos must be < Vect_count(). The vector must be non-empty. */
void Vect_remove_<name>(vector *v, size_t pos);
#endif



#ifdef __cplusplus
}
#endif

#endif


