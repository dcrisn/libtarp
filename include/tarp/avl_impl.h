#ifndef TARP_AVL_IMPL_H
#define TARP_AVL_IMPL_H

#include "avl.h"
#include "bst.h"
#include <tarp/common.h>
#include <tarp/staq.h>

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

#endif
