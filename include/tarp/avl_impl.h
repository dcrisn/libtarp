#ifndef TARP_AVL_IMPL_H
#define TARP_AVL_IMPL_H

#include <tarp/staq.h>
#include <tarp/bst.h>


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
    struct avlnode *cached;
    avl_comparator cmp;
    size_t count;
    avlnode_destructor dtor;
};

define_bstnode_ptr_wrapper_structure(avlnode, struct avlnode)

/*
 * Used to store avl nodes in a staq */
struct avlnode_qwrap{
    struct staqnode link;
    struct avlnode *p;
};

static struct avltree initialize_avl_tree(avl_comparator cmp, avlnode_destructor dtor)
{
    assert(cmp);
    return (struct avltree){.root = NULL, .cmp = cmp, .count=0, .dtor=dtor};
}

#define AVL_INITIALIZER__(cmp, dtor) initialize_avl_tree(cmp, dtor)

/*
 * True if the given non-NULL tree observes the AVL properties, else false. */
bool isavl(const struct avltree *tree);


#define Avl_min_(tree, container_type, field) \
    Avl_empty(tree) ? NULL : container(Avl_get_min(tree), container_type, field)

#define Avl_max_(tree, container_type, field) \
    Avl_empty(tree) ? NULL : container(Avl_get_max(tree), container_type, field)

#define Avl_kmin_(tree, k, container_type, field) \
    (Avl_count(tree) < k) ? NULL : container(Avl_get_kth_min(tree, k), container_type, field)

#define Avl_kmax_(tree, k, container_type, field) \
    (Avl_count(tree) < k) ? NULL : container(Avl_get_kth_max(tree, k), container_type, field)

void Avl_get_nodes_at_level(
        const struct avltree *tree,
        size_t level,
        struct staq *queue);

#define Avl_foreach_node_at_level_(tree, level, q, i, container_type, field)   \
    Avl_get_nodes_at_level(tree, level, (q));                                  \
    for (struct avlnode_ptr *pointer=NULL;                                     \
            ((pointer = Staq_dq(q, struct avlnode_ptr, link)) &&               \
             (i = (pointer && pointer->p)                                      \
               ? container(pointer->p, container_type, field) : NULL));        \
             salloc(0, pointer)                                                \
             )


#endif
