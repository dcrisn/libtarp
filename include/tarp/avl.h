#ifndef TARP_AVL_H
#define TARP_AVL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include <tarp/common.h>

/*--------------------------------------------------------------------------.
 * AVL -- heigt balanced tree                                               |
 *                                                                          |
 * This header file describes the interface to an AVL tree                  |
 * implementation. Like most other data structures in this library, the     |
 * implementation is intrusive. See the main README file for general        |
 * notes on using the API.                                                  |
 * As for other data structures, macros are provided to simplify API usage. |
 *                                                                          |
 *                                                                          |
 * === Operations and runtime characteristics ===                           |
 *------------------------------------------------                          |
 *                                                                          |
 * Avl_{init,new}                     O(1)                                  |
 * Avl_{clear,destroy}                O(n)        // See [1]                |
 * Avl_empty                          O(1)                                  |
 * Avl_count                          O(1)                                  |
 * Avl_height                         O(log n)                              |
 * Avl_find                           O(log n)                              |
 * Avl_has                            O(log n)                              |
 * Avl_insert                         O(log n)                              |
 * Avl_find_or_insert                 O(log n)                              |
 * Avl_delete                         O(log n)                              |
 * Avl_unlink                         O(log n)                              |
 * Avl_{min,max}                      O(log n)                              |
 * Avl_{kmin,kmax}                    O(n)        // See [2]                |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
 *                                                                          |
 * [1] these are O(1) if the nodes must not be destructed/deallocated.      |
 * Otherwise O(n) since the tree must be traversed end to end.              |
 *                                                                          |
 * [2] for small k these are quasi- constant time. However, the             |
 * performance deteriorates linearly as k increases. A speedup could        |
 * be achieved at the cost of an increase in the memory footprint per       |
 * node. Since this implementation tries to stay lean, the slower but       |
 * more lightweight approach has been taken, with the assumption that       |
 * these functions not be used for large k. NOTE: the performance           |
 * graph would be a parabola: the worst performance is not when k is equal  |
 * to the number of nodes in the tree, but when it's **half** that. It      |
 * then improves as k moves toward either the max or the min.               |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  |
 *                                                                          |
 * NOTE on Duplicates                                                       |
 * -------------------                                                      |
 * Duplicates are not allowed. The rationale is that this could be easily   |
 * implemented on an as needed basis [3] as the application requires. OTOH  |
 * it would be undesirable to saddle the user automatically with the        |
 * increased memory usage that that would entail, even when not needed.     |
 *                                                                          |
 * [3] The user container could embed a dlnode or staqnode, for example,    |
 * as well. The find_or_insert function could be put to good use here:      |
 * if a duplicate does not exist, insert it. Otherwise use the pointer      |
 * to the entry found and add the new entry to the embedded linked list     |
 * as a dlnode/staqnode neighbor. A multiset could be trivially implemented |
 * like this, for instance.                                                 |
 *                                                                          |
 * NOTE on Avl_set                                                          |
 * ----------------                                                         |
 * This function is not provided because it is unnecessary. All the data,   |
 * including the key and any values, is internal to the container. The user |
 * can simply look up the required element and then modify it as needed     |
 * (i.e. *set* its value to something else). A word of caution: any values  |
 * can be changed inside the container using the pointer returned           |
 * from a lookup -- except the key that the comparator uses, of course.     |
 * If the key changes the node *must* be removed from the tree first,       |
 * and then reinserted.                                                     |
 *=========================================================================*/

struct avlnode;
struct avltree;

typedef struct avltree avltree;
typedef struct avlnode avlnode;

/*
 * See main README fmi */
typedef void (*avlnode_destructor)(struct avlnode *node);

typedef enum comparatorResult (*avl_comparator)
    (const struct avlnode *a, const struct avlnode *b);

/*
 * This is necessary if Avl_print is to be used.
 * This describes a callback function that when called with an avlnode
 * it returns a (*very brief*) dynamically-allocated string representation
 * of the container. See the README for details on how to get back the
 * container from the avlnode.
 *
 * The API will free the string internally.*/
typedef const char *(*avlnode_printer)(const struct avlnode *node);


#include "avl_impl.h"


/*
 * Initialize a statically declared avltree handle; the macro and Avl_init
 * do the same thing. The handle passed to Avl_init must be non-NULL.
 * See the README fmi on the comparator and destructor arguments. */
#define AVL_INITIALIZER(cmpf, dtor)   AVL_INITIALIZER__(cmpf,dtor)
void Avl_init(struct avltree *tree, avl_comparator cmpf, avlnode_destructor dtor);

/*
 * Get a dynamically allocated and initialized avl tree.
 * The user must call Avl_destroy() on this when no longer needed.
 * See the README fmi on the comparator and destructor arguments.
 * */
struct avltree *Avl_new(avl_comparator cmpf, avlnode_destructor dtor);

/*
 * Reset the given non-NULL tree.
 * All items are removed from the tree but not free()d unless free_containers
 * is specified. If free_containers=true, a destructor *must* have been
 * specified at initialization time. See README fmi.
 * */
void Avl_clear(struct avltree *tree, bool free_containers);

/*
 * Clear the tree and then free() the tree handle itself and set it to NULL.
 * The elements are removed but not freed unless free_containers=true.
 * The handle itself is always free()d and then set to NULL. Both the
 * 'tree' handle and the pointer to it must be non-NULL.
 *
 * See Avl_clear for the case where free_containers=true.
 */
void Avl_destroy(struct avltree **tree, bool free_containers);

/*
 * Return the number of elements currently in the given non-NULL tree. */
size_t Avl_count(const struct avltree *tree);

/*
 * True if the given non-NULL tree has a count of 0 elements, else false */
bool Avl_empty(const struct avltree *tree);

/*
 * Return the height of the given non-NULL AVL tree.
 * Possible return values are -1 (empty tree), 0 (single node) or > 0.
 * A leaf node has a height of 0 and the root height is the height of the
 * entire tree. */
int Avl_height(const struct avltree *tree);

/*
 * Look up a matching value in the tree.
 *
 * --> tree
 * non-NULL handle for a tree to perform the lookup in.
 *
 * --> key
 * a non-NULL pointer to a struct avlnode that specifies what to look up.
 * This value is used by the comparator to find the matching value.
 *
 * <-- return
 * A pointer to a matching struct avlnode. If no matching element is found,
 * NULL is returned. */
struct avlnode *Avl_find_node(
        struct avltree *tree, const struct avlnode *key);

/*
 * Look up the node in the tree; if it exists, return a pointer to the existing
 * entry. If it does not exist, insert it.
 *
 * NOTE this is typically faster than first calling lookup and then insert
 * individually.
 *
 * tree and node must be non-NULL.*/
struct avlnode *Avl_find_or_insert_node(struct avltree *tree, struct avlnode *node);

/*
 * True if the node exists in the tree, else false.
 * key will be used by the comparator for the lookup.
 * tree and key must be non-NULL. */
bool Avl_has_node(const struct avltree *tree, const struct avlnode *key);

/*
 * Insert the given node in the non-NULL tree.
 * True is returned if the insertion was successful, otherwise false.
 * The insertion fails if there already exists an entry in the tree
 * that the comparator considers equal to <node> */
bool Avl_insert_node(struct avltree *tree, struct avlnode *node);

/*
 * Remove the given node (if found) from the non-NULL tree.
 * If free_container=true, the node destructor (which *must* have been
 * specified at initialization time -- see README fmi) is invoked after
 * unlinking the node. */
bool Avl_delete_node(struct avltree *tree, struct avlnode *k, bool free_container);

/*
 * Print a graph of the entire tree. See notes at the top of the header
 * file about the point of pf. */
void Avl_print(struct avltree *tree, avlnode_printer pf);

/*
 * Get a pointer to the node with the maximum/minimum value currently
 * in the tree. tree must be non-NULL. If empty, NULL is returned. */
struct avlnode *Avl_get_max(const struct avltree *tree);
struct avlnode *Avl_get_min(const struct avltree *tree);

/*
 * Get a pointer to the node with the the k-th smallest/greatest
 * value currently in the tree. If k is 0 or > the number of elements
 * currently in the tree, NULL is returned.
 * K is 1-based. Therefore a call to get the kth min for k=1 returns
 * the same value as Avl_get_min().
 */
struct avlnode *Avl_get_kth_min(const struct avltree *tree, size_t k);
struct avlnode *Avl_get_kth_max(const struct avltree *tree, size_t k);

/*
 * Return the number of levels in the tree. This is
 * one greater than the height/depth of the tree, since a tree
 * with only the root has 1 level and height 0 */
size_t Avl_num_levels(const struct avltree *tree);

/*
 * Return the number of nodes at the specified level */
size_t AVL_count_nodes_at_level(const struct avltree *tree, size_t level);

/*======================================================================
 * The following macros are meant to simplify API usage; particularly,
 * most will convert the pointer to the avlnode returned by functions
 * above to a pointer to the container. Similarly, they take a pointer
 * to the container as argument instead of a pointer to the avlnode.
 * The container_ptr and field parameters are forwarded to the container
 * macro. See the README fmi. */

#define Avl_insert(tree, cont, field) \
    Avl_insert_node(tree, &((cont)->field))

/*
 * Unlink node AND destruct the container; the destructor *must* have
 * been specified at initialization time. */
#define Avl_delete(tree, cont, field) \
    Avl_delete_node(tree, &((cont)->field), true)

/*
 * Unlink but do not destruct the container */
#define Avl_unlink(tree, container_ptr, field) \
    Avl_delete_node(tree, &((container_ptr)->field), false)

#define Avl_find(tree, container_ptr, field) \
    Avl_find_(tree, container_ptr, field)

#define Avl_find_or_insert(tree, containter_ptr, field) \
    Avl_find_or_insert_(tree, containter_ptr, field)

#define Avl_has(tree, container_ptr, field) \
    Avl_has_node(tree, &((container_ptr)->field))

#define Avl_min(tree, container_type, field) \
    Avl_min_(tree, container_type, field)

#define Avl_max(tree, container_type, field) \
    Avl_max_(tree, container_type, field)

#define Avl_kmin(tree, k, container_type, field) \
    Avl_kmin_(tree, k, container_type, field)

#define Avl_kmax(tree, k, container_type, field) \
    Avl_kmax_(tree, k, container_type, field)

/*
 * Return a pointer to the container of each node at the specified level
 * in the tree. The nodes at the specified level are iterated over left
 * to right.
 * tree must be non NULL.
 *
 * --> q
 * a statically declared staq variable to be used internally. Since it
 * was statically declared, no cleanup is necessary after the loop.
 *
 * --> level
 * The level over whose elements to iterate. Must be > 0. Level is in this
 * context taken to mean 'depth'. Root is at level 1. The tree has as many
 * levels as it has height. Therefore to iterate over all node elements,
 * level by level, this macro can be called with
 * level=1 ..., level=Avl_num_levels().
 */
#define Avl_foreach_node_at_level(tree, level, q, i, container_type, field) \
    Avl_foreach_node_at_level_(tree, level, q, i, container_type, field) \


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
