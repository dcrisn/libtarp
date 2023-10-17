#ifndef TARP_AVL_H
#define TARP_AVL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include <tarp/common.h>
#include <tarp/staq.h>
#include <tarp/bst.h>

/*
 * Duplicates -- higher-level code should do it
 *  - not allowed because they are rarely needed and so applications that don't
 *  need that would be forced to pay the overhead for every node. Instead, the
 *  embedded structure could maintain a list so that duplicates can be stored,
 *  or a counter to count occurences. This would make it trivial to implement
 *  counters or multisets.
 */
struct avlnode;
typedef void (*avlnode_destructor)(struct avlnode *node);

typedef enum comparatorResult (*avl_comparator)\
    (const struct avlnode *a, const struct avlnode *b);

typedef const char *(*avlnode_printer)(const struct avlnode *node);

struct avlnode {
    struct avlnode *left;
    struct avlnode *right;

    int8_t bf;  /* balance factor */
#ifdef AVL_NODES_WITH_HEIGHT_FIELD
    unsigned int height;
#endif
};

struct avltree{
    struct avlnode *root;
    avl_comparator cmp;
    size_t count;
    avlnode_destructor dtor; 
};

typedef struct avltree avltree;
typedef struct avlnode avlnode;


#include "avl_impl.h"

/*
 * Return the height of the given non-NULL AVL tree.
 * Possible return values are -1 (empty tree), 0 (only root) or >0.
 * A leaf note has a height of 0 and the root height is the height of the
 * entire tree. */
int Avl_height(const struct avltree *tree);


/*
 * Initialize a statically declared avltree handle; the macro and Avl_init
 * do the same thing. The handle passed to Avl_init must be non-NULL. */
#define AVL_INITIALIZER(cmpf, dtor)   AVL_INITIALIZER__(cmpf,dtor)
void Avl_init(struct avltree *tree, avl_comparator cmpf, avlnode_destructor dtor);

/*
 * Get a dynamically allocated and initialized avl tree.
 * The user must call Avl_destroy() on this when no longer needed. */
struct avltree *Avl_new(avl_comparator cmpf, avlnode_destructor dtor);

/*
 * Reset the given non-NULL tree.
 * All items are removed from the tree but not free()d unless free_containers
 * is specified. */
void Avl_clear(struct avltree *tree, bool free_containers);

/*
 * Clear the tree and then free() the tree handle itself and set it to NULL.
 * The elements are removed but not freed unless free_containers=true.
 * The handle itself is always free()d and then set to NULL. Both the
 * 'tree' handle and the pointer to it must be non-NULL. */
void Avl_destroy(struct avltree **tree, bool free_containers);

/*
 * Return the number of elements currently in the given non-NULL tree. */
size_t Avl_count(const struct avltree *tree);

/*
 * True if the given non-NULL tree has a count of 0 elements, else false */
bool Avl_empty(const struct avltree *tree);

/*
 * Lookup a matching value inside the tree.
 *
 * --> tree
 * non-NULL handle for a tree to perform the lookup in.
 *
 * --> key
 * a non-NULL pointer to a struct avlnode that specifies what to look up.
 * This value is used used by the comparator to find the matching value.
 *
 * <-- return
 * A pointer to a matching struct avlnode. If no matching element is found,
 * NULL is returned. */
struct avlnode *Avl_find_node(const struct avltree *tree, const struct avlnode *key);
bool Avl_has_node(const struct avltree *tree, const struct avlnode *key);


struct avlnode *Avl_set(struct avltree *tree, struct avlnode *node);
bool Avl_insert_node(struct avltree *tree, struct avlnode *node);
bool Avl_delete_node(struct avltree *tree, struct avlnode *k, bool free_container);

/*
 * Print a dump of the entire tree.
 * If graphed=true, a graph is printed, otherwise simply a left-to-right
 * level-by-level list of nodes from tree root to leaves is printed */
void Avl_print(struct avltree *tree, bool graphed, avlnode_printer pf);

define_bstnode_ptr_wrapper_structure(avlnode, struct avlnode)

#define Avl_insert(tree, cont, field) \
    Avl_insert_node(tree, &((cont)->field))

#define Avl_delete(tree, cont, field) \
    Avl_delete_node(tree, &((cont)->field), true); \

#define Avl_unlink(tree, cont, field) \
    Avl_delete_node(tree, &((cont)->field), false)

#define Avl_find(tree, cont, container_type, field) \
    Avl_find_node(tree, &((cont)->field))

#define Avl_has(tree, cont, container_type, field) \
    Avl_has_node(tree, &((cont)->field))

    //container(Avl_find_node(tree, &((cont)->field)), container_type, field)


void Avl_get_nodes_at_level(const struct avltree *tree, size_t level, struct staq *queue);

#define Avl_foreach_node_at_level(tree, level, q, i, container_type, field) \
    Avl_get_nodes_at_level(tree, level, q); \
    for (struct avlnode_ptr *pointer=NULL; \
            ((pointer = Staq_dq(q, struct avlnode_ptr)) && \
             (i = (pointer && pointer->p) ? container(pointer->p, container_type, field):NULL)) ; \
             salloc(0, pointer) \
             )

#if 0
TODO
depth, height
max, min

void AVL_show_level_order(const struct bst_tree* tree, const char *treename);

ssize_t AVL_count_nodes_at_level(const struct bst_tree *tree, const size_t level);
Queue AVL_to_queue(struct bst_tree *tree);

#endif



#ifdef __cplusplus
} /* extern "C" */
#endif


#endif
