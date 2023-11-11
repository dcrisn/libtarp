#ifndef TARP_HEAP_H
#define TARP_HEAP_H

#include <tarp/common.h>
#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------.
 * Heaps and Heap-based priority queues                                     |
 *                                                                          |
 * This header file describes the interface to an implicit array-stored     |
 * heap (see struct heap, struct heapnode) as well as an explicit           |
 * pointer-based heap (see struct xheap, struct xheapnode).                 |
 * The ordering in both cases can be specified as either max or min at      |
 * at initialization time                                                   |
 *                                                                          |
 * Like most other data structures in this library, the                     |
 * implementation is intrusive. See the main README file for general        |
 * notes on using the API.                                                  |
 * As for other data structures, macros are provided to simplify API usage. |
 *                                                                          |
 *                                                                          |
 * === Operations and runtime characteristics ===                           |
 *------------------------------------------------                          |
 * Operation               minheap                  maxheap                 |
 * new                      O(1)                     O(1)                   |
 * init                     O(1)                     O(1)                   |
 * destroy                  O(n)                     O(n)                   |
 * clear                    O(n)                     O(n)                   |
 * count                    O(1)                     O(1)                   |
 * empty                    O(1)                     O(1)                   |
 * top                      O(1)                     O(1)                   |
 * pop                      O(log n)                 O(log n)               |
 * push                     O(log n)                 O(log n)               |
 * remove                   O(log n)                 O(log n)               |
 * update_prio              O(log n)                 O(log n)               |
 *                                                                          |
 * NOTE the names of the operations above don't necessarily match the       |
 * actual names below. They are listed abstractly for comparison purposes.  |
 *                                                                          |
 * NOTE destroy and clear are O(n) only if free_containers=true. Otherwise  |
 * O(1).                                                                    |
 *                                                                          |
 * NOTE the complexity for all operation is the same for heaps and xheaps.  |
 * However, xheapnode's occupy more space and the nature of the             |
 * implementation (pointers) means the explicit heap will be slower than    |
 * the implcit one. In short, about the only reason you might prefer        |
 * the explicit heap to the implicit one is if allocation of large          |
 * blocks of memory at a time (particularly if the heap is very big)        |
 * is undesriable.                                                          |
 *                                                                          |
 * NOTE both heapnodes and xheapnodes are meant to be embedded in           |
 * dynamically-allocated structures. Effectively, even though the implicit  |
 * heap is backed by a vector, it stores not struct heapnodes, but          |
 * pointers to them. This avoids e.g. large objects being copied around     |
 * in the vector and slowing down performance.                              |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
 *                                                                          |
 * Comparators and node destructors                                         |
 * ----------------------------------                                       |
 * See the main README for information on these.                            |
 *                                                                          |
 * Heap priorities/keys                                                     |
 * ---------------------                                                    |
 * The priority/key of a heap element is internal to the (x)heapnode.       |
 * Much like for avl trees (see tarp/avl.h), the user-supplied comparator   |
 * callback guides pairwise comparisons and therefore the user is in        |
 * complete control as to which member fields are used in the comparison    |
 * and what the priority/key value is represented as.                       |
 *                                                                          |
 *=========================================================================*/


/* Implicit Heap (array-stored) */
struct heap;
struct heapnode;

/* Explicit Heap (pointer-based) */
struct xheap;
struct xheapnode;

/*
 * The type of the heap, defined by its heap ordering. */
enum heapOrder {MIN_HEAP, MAX_HEAP};

typedef enum comparatorResult (*heap_comparator)\
    (const struct heapnode *a, const struct heapnode *b);

typedef enum comparatorResult (*xheap_comparator)\
    (const struct xheapnode *a, const struct xheapnode *b);

typedef void (*heapnode_destructor)(struct heapnode *node);
typedef void (*xheapnode_destructor)(struct xheapnode *node);


#include "impl/heap_impl.h"

/*
 * For dynamically-allocated heap handles */
struct heap *Heap_new(
        enum heapOrder type,
        heap_comparator cmp,
        heapnode_destructor dtor);

/*
 * For stack-allocated heap handles */
void Heap_init(
        struct heap *h,
        enum heapOrder type,
        heap_comparator cmp,
        heapnode_destructor dtor);


/*
 * Remove all elements from the non-NULL heap.
 * If free_containers=true, then a destructor *must* have been
 * specified at initialization time and this will be called
 * on each node as it gets removed. */
void Heap_clear(struct heap *h, bool free_containers);

/*
 * Clear the heap then deallocate the dynamically-allocated
 * handle pointed to by the non-NULL reference 'h'.
 *
 * NOTE this function may only be called and *must* be called
 * for dynamically-allocated heap handles created with Heap_new().
 *
 * See Heap_clear and main README fmi. */
void Heap_destroy(struct heap **h, bool free_containers);

/*
 * Get the number of elements in the non-NULL heap. */
size_t Heap_count(const struct heap *h);

/*
 * True if the number of elements in the non-NULL heap
 * is 0, else false. */
bool Heap_empty(const struct heap *h);

/*
 * Get (without removing) or pop (get and remove as well) the root.
 * This is either the max or the min-value element of the heap,
 * depending on whether the heap was initialized with type
 * MIN_HEAP or MAX_HEAP.
 *
 * h must be non-NULL. */
struct heapnode *Heap_get_root(const struct heap *h);
struct heapnode *Heap_pop_root(struct heap *h);

/*
 * Push the non-NULL node pointer onto the heap.
 * The heap handle must be non-NULL.
 *
 * This may trigger the internal vector to grow in size.
 * The allocation could therefore possibly fail. In case of
 * failure, the error code returned by Vect_pushb is returned
 * (see tarp/vector.h), the push does no occur, and the heap
 * is left unchanged.
 */
int Heap_push_node(struct heap *h, struct heapnode *node);

/*
 * Unlink from the heap the given non-NULL node.
 * The heap handle must be non-NULL and the node
 * must currently exist in the heap.
 *
 * If free_container=true, the node destructor *must* have
 * been specified at initialization time and it will be
 * called on the node after unlinking it. */
void Heap_remove_node(struct heap *h, struct heapnode *node, bool free_container);

/*
 * Given a non-NULL pointer to node, which currently exists in
 * the heap accessed through the given non-NULL handle,
 * move it to its correct position inside the heap.
 *
 * NOTE this need only be called if the priority/key of the
 * node has increased/decreased. If unchaged, the position
 * will remain the same. */
void Heap_priochange(struct heap *h, struct heapnode *node);

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The following macros are meant to simplify the API by wrapping
 * around calls to the functions above. They take/return
 * a pointer to the container rather than a pointer to the
 * heapnode member.
 *
 * Refer to the documentation for the corresponding function
 * above where needed.
 */

// get root
#define Heap_top(heap_ptr, container_type, field) \
    _Heap_top__(heap_ptr, container_type, field)

// pop root
#define Heap_pop(heap_ptr, container_type, field) \
    _Heap_pop__(heap_ptr, container_type, field)

#define Heap_push(heap_ptr, container_ptr, field) \
    _Heap_push__(heap_ptr, container_ptr, field)

/*
 * Unlink the node *without* destructing it */
#define Heap_remove(heap_ptr, container_ptr, field) \
    _Heap_remove__(heap_ptr, container_ptr, field)

/*
 * Unlink the node then destruct it. The node destructor
 * must have been specified at initialization time. */
#define Heap_delete(heap_ptr, container_ptr, field) \
    _Heap_delete__(heap_ptr, container_ptr, field)

// priochange
#define Heap_update(heap_ptr, container_ptr, field) \
    _Heap_update__(heap_ptr, container_ptr, field)


/* ===============================================
 * === Explicit (pointer-based) heap API =====
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The usage is the same as for the corresponding
 * implicit heap functions/macros above, unless
 * otherwise indicated. Therefore refer to the
 * documentation above where needed.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

struct xheap *XHeap_new(
        enum heapOrder type,
        xheap_comparator cmp,
        xheapnode_destructor dtor);

void XHeap_init(
        struct xheap *h,
        enum heapOrder type,
        xheap_comparator cmp,
        xheapnode_destructor dtor);

void XHeap_destroy(struct xheap **h, bool free_containers);

size_t XHeap_count(const struct xheap *h);
bool XHeap_empty(const struct xheap *h);

struct xheapnode *XHeap_get_root(const struct xheap *xh);
struct xheapnode *XHeap_pop_root(struct xheap *h);

void XHeap_push_node(struct xheap *h, struct xheapnode *node);
void XHeap_clear(struct xheap *h, bool free_containers);
void XHeap_remove_node(struct xheap *h, struct xheapnode *node, bool free_container);

void XHeap_priochange(struct xheap *h, struct xheapnode *node);

#define XHeap_top(heap_ptr, container_type, field) \
    _XHeap_top__(heap_ptr, container_type, field)

#define XHeap_pop(heap_ptr, container_type, field) \
    _XHeap_pop__(heap_ptr, container_type, field)

#define XHeap_push(heap_ptr, container_ptr, field) \
    _XHeap_push__(heap_ptr, container_ptr, field)

#define XHeap_remove(heap_ptr, container_ptr, field) \
    _XHeap_remove__(heap_ptr, container_ptr, field)

#define XHeap_delete(heap_ptr, container_ptr, field) \
    _XHeap_delete__(heap_ptr, container_ptr, field)

#define XHeap_update(heap_ptr, container_ptr, field) \
    _XHeap_update__(heap_ptr, container_ptr, field)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
