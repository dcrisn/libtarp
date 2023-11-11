#ifndef TARP_HASHT_IMPL_H
#define TARP_HASHT_IMPL_H

#include <tarp/container.h>
#include <tarp/common.h>

struct hashtnode{
    struct hashtnode *next;
};

/*
 * (1) The load factor is expressed as a perfect fraction multiplied by 100,
 * then truncated to an integer.
 * E.g. to specify 0.75, pass 75; to specific 1, pass 100.
 * NOTE this limits the decimal part to 2 digits.
 *
 * (2) users can provide their own hash function that particuarly suits the
 * data type being stored. A default is used otherwise.
 *
 * (3) since the key to hash is embedded somewhere in the container (the
 * user data containing the hashtnode), a callback *must* be provided to
 * extract the key at hash time.
 *
 * (4) the chain in each bucket could be any of various linked data structures,
 * each with their own advantages and disadvantages.
 *  A. singly linked list;
 *   - assuming duplicates are allowed, the most recent insert can be done
 *     at the front of the list in a stack-like fashion. This would allow
 *     for example the implementation of nested scopes as may be used by a
 *     parser.
 *   - only one pointer is needed per hashtnode
 *   - search would be linear in the length of the list so assuming the list
 *     is unsorted lookups and deletion would be slower than insertion.
 *
 *  B. doubly linked list
 *   - deletion would be constant time provided the handle is passed to the
 *     deletion routine. No search needs to be done.
 *   - 2 pointers per hashtnode. More overhead than a singly linked list.
 *
 *  C. Binary search trees
 *   - would make it easy to enforce a uniqueness constraint and disallow
 *     duplicates
 *   - potentially faster lookups
 *   - more complicated code (e.g. for deletion); can still degenerate into
 *     a linked list.
 *
 *  D. Balanced binary search trees
 *  - would improve the worst-case runtime complexity of the hash table from
 *    O(n) to O(log n), offering possibly desirable guarantees.
 *  - significantly more complex (unless an implementation is available) than
 *    the other solutions. Possibly higher overhead than the others. Possibly
 *    *slower* on the average.
 *
 * Typically a singly linked list is perfectly fine and the added complexity
 * brought by the other solutions is unnecessary. Particularly, the load factor
 * should remain well below 1 and so the linked lists would be close to empty
 * on average -- meaning the linked list operations would be quasi-constant
 * time. If this is not the case and the load factor is allowed to go above
 * one, then of course the lists will grow in length, and depending on how
 * high the load factor is allowed to get, hash table operations may take on
 * a more linear runtime characteristic. It's not expected this will be the
 * case and so the current approach is to use a singly linked list for both
 * simplicity and low memory footprint.
 */
struct hasht {
    size_t num_buckets;
    size_t count;
    unsigned lf;                     /* tweakable load factor (1) */
    hashf hash;                      /* tweakable hash function (2) */
    key_getter key;                  /* callback that specifies the key (3) */
    hashtnode_destructor dtor;
    struct hashtnode *buckets;       /* slots for chains (4) */
    struct hashtnode *cached;        /* see avl_impl.h; same idea here */
};

#define Hasht_get_(hash_table, container_ptr, field) \
    Hasht_get_entry(hash_table, &((container_ptr)->field)) \
      ? get_container((hash_table)->cached, typeof(*(container_ptr)), field) \
      : NULL


#endif
