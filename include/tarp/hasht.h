#ifndef TARP_HASHT_H
#define TARP_HASHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------------------.
 * Hash table                                                               |
 *                                                                          |
 * This header file describes the interface to a hash table with separate   |
 * chaining. As for other data structures in this library, a user-defined   |
 * structure can be linked into the hash table by embedding an intrusive    |
 * link (struct hashtnode) and feeding that to the API.                     |
 *                                                                          |
 * See the README for general notes on using the API.                       |
 * As for other data structures, macros are provided to simplify API usage. |
 *                                                                          |
 *                                                                          |
 * === Operations and runtime characteristics ===                           |
 *------------------------------------------------                          |
 * new                           O(1)                                       |
 * destroy                       O(n)  // [1]                               |
 * count                         O(1)                                       |
 * empty                         O(1)                                       |
 * has                           O(1)  // amortized                         |
 * get                           O(1)  // amortized                         |
 * insert                        O(1)  // amortized                         |
 * delete                        O(1)  // amortized                         |
 *                                                                          |
 * [1] O(n) only if the elements must be destructed. Otherwise O(1).        |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
 *                                                                          |
 * node destructor and key getter                                           |
 * ----------------------------------                                       |
 * See the README for information on the node destructor.                   |
 *                                                                          |
 * The key getter is a callback that must be specified by the user at       |
 * initialization time. This is required in order to be able to extract the |
 * key to be hashed from a hashtnode. The API cannot independenty do it     |
 * since the nature of the key is arbitrary and can be located anywhere     |
 * in the container.                                                        |
 * Specific information on the key getter is provided in a comment below.   |
 *                                                                          |
 *=========================================================================*/

struct hasht;
struct hashtnode;

/*
 * A function that takes as input a pointer to a byte buffer of the specified
 * length and produces a 64-bit digest of the input. */
typedef uint64_t (*hashf)(const uint8_t *input, size_t len);

/*
 * A function that when called with a pointer to a hashtnode, it derives
 * from it a pointer to the container (see README) from which it then
 * extracts the key used with the hash table.
 *
 * The key is of arbitrary type (string, integer etc) but for the purposes
 * of hashing it's a contiguous buffer of bytes. The key_getter should
 * write the start address of this buffer into <start> and the length
 * (in bytes) into <len>.
 *
 * The hash function used by the hash table will then process this buffer
 * to produce a digest (see hashf above). */
typedef void (*key_getter)(const struct hashtnode *node, void **start, size_t *len);

/*
 * A node destructor: when called with a poitner to a hashtnode,
 * it derives from it a pointer to the container (see README).
 * It then destructs the container in an appropriate way.
 * Any function that requires this to be specified is indicated below. */
typedef void (*hashtnode_destructor)(struct hashtnode *htn);


#include "hasht_impl.h"

/*
 * Dynamically allocate a new hash table handle and initialize it.
 *
 * --> hashfunc
 * A hash function suitable for producing a digest of an input buffer.
 * See comment above the hashf prototype above. If this is NULL, a
 * default hash function is used.
 *
 * --> keyfunc
 * See comment above the key_getter prototype above. Must be non-NULL.
 *
 * --> dtor
 * A node destructor callback. See comment above the hashtnode_destructor
 * prototype above and the main README. This may be NULL but only iff
 * no function is called such that relies on the destructor being set.
 * Otherwise the program will crash. Where this is required, a comment
 * above the respective function will point it out.
 *
 * --> lf
 * Load factor. This is a proper fraction expressed as a number between
 * 0 and 100[1]. That is, a decimal number less than 1 with a precision of
 * only 2 decimal places, multiplied by 100. E.g. 87 = 0.87; 60 = 0.6.
 * This value influences the hash table growth policy. The size of the table
 * should expand whenever the ratio of the current number of elements
 * to the current number of buckets reaches this specified load factor.
 *
 * If 0, the default is used.
 *
 * [1] NOTE that there is no maximum or minimum value (i.e. absurd values are
 * not rejected) for the load factor. If specified as 0, the default is used
 * (recommended).
 */
struct hasht *Hasht_new(
        hashf hashfunc,
        key_getter keyfunc,
        hashtnode_destructor dtor,
        unsigned lf);

/*
 *
 * Remove every element from the hash table. If free_containers=true,
 * then the node destructor *must* have been specified at initialization
 * time, and it will be invoked for every element after its removal.
 *
 * The hasht pointer must be non-NULL and point to a valid dynamically
 * allocated handle obtained through Hasht_new. This will be
 * deallocated and the handle will be set to NULL.
 */
void Hasht_destroy(struct hasht **hasht, bool free_containers);

/*
 * Return the theoretical maximum number of elements that could be stored
 * in the hash table before new insertions are rejected. NOTE failures
 * e.g. due to memory shortfall could occur long before then.
 */
size_t Hasht_maxcap(void);

/*
 * Return the total number of elements currently in the hash table.
 * The ht handle must be non-NULL */
size_t Hasht_count(const struct hasht *ht);

/*
 * True if Hasht_count returns 0, else false */
bool Hasht_empty(const struct hasht *ht);

/*
 * True if a node that matches the specified key exists in the hash table,
 * else false.
 *
 * ht and key must be non-NULL. */
bool Hasht_has_entry(const struct hasht *ht, const struct hashtnode *key);

/*
 * Get a pointer to the element in the hash table whose key matches the
 * specified key. If no such element exists, return NULL
 *
 * ht and key must be non-NULL
 * */
struct hashtnode *Hasht_get_entry(
        const struct hasht *ht,
        const struct hashtnode *key);

/*
 * Remove from the hash table the element whose key matches the one specified.
 * If the removal succeeds, return true.
 *
 * If free_container=true, then the node destructor must have been specified
 * at initialization time and it will be called with the element after its
 * removal.
 *
 * If no such element exists, return false.
 */
bool Hasht_remove_entry(
        struct hasht *ht,
        struct hashtnode *key,
        bool free_container);


/*
 * Insert elem into the hash table.
 *
 * --> allow_duplicates
 * If duplicates are allowed, then if an element with a matching
 * key already exists, this insertion will put elem before
 * the existing one such that a lookup will find it first.
 * If allow_duplicates=false, then FALSE is returned if
 * elem is a duplicate, and no insertion is made.
 *
 * elem and ht must be non-NULL.
 *
 * <-- return
 * True on success, false on failure.
 * The request fails if elem is a duplicate (and allow_duplicates=false)\
 * or if the new element triggers a resizing of the hash table that is
 * beyond allowable limits.
 */
bool Hasht_insert_entry(
        struct hasht *ht,
        struct hashtnode *elem,
        bool allow_duplicates);

// insert even if a duplicate
#define Hasht_set(hash_table, container, field) \
    Hasht_insert_entry(hash_table, &((container)->field), true)

// insert only if not a duplicate
#define Hasht_maybe_set(hash_table, container, field) \
    Hasht_insert_entry(hash_table, &((container)->field), false)

#define Hasht_get(hash_table, container, field) \
    Hasht_get_entry(hash_table, &((container)->field))

#define Hasht_has(hash_table, container, field) \
    Hasht_has_entry(hash_table, &((container)->field))

// remove but do not destruct
#define Hasht_remove(hash_table, container, field) \
    Hasht_remove_entry(hash_table, &((container)->field), false)

// remove and destruct
#define Hasht_delete(hash_table, container, field) \
    Hasht_remove_entry(hash_table, &((container)->field), true)


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
