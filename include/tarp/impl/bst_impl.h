#ifndef TARP_BST_IMPL_H
#define TARP_BST_IMPL_H

#include <tarp/container.h>
#include <tarp/dllist.h>
#include <tarp/staq.h>
#include <tarp/log.h>

static inline struct bst initialize_bst(bst_comparator cmp, bstnode_destructor dtor){
    return (struct bst){.root = NULL, .cmp = cmp, .count=0, .dtor = dtor};
}

#define BST_INITIALIZER__(cmp, cast) initialize_bst(cmp, cast)

#define define_bst_count_getter_(vistype, SHORTNAME, TREE_TYPE)     \
    vis(vistype) size_t SHORTNAME##_count(const TREE_TYPE *tree){   \
        assert(tree);                                               \
        return tree->count;                                         \
    }

#define define_bst_empty_predicate_(vistype, SHORTNAME, TREE_TYPE) \
    vis(vistype) bool SHORTNAME##_empty(const TREE_TYPE *tree){    \
        assert(tree);                                              \
        return (tree->count == 0);                                 \
    }

#define define_bst_node_finder_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE) \
    vis(vistype) NODE_TYPE *SHORTNAME##_find_node(                        \
            const TREE_TYPE *tree,                                        \
            const NODE_TYPE *key)                                         \
{                                                                         \
    assert(tree);                                                         \
    assert(key);                                                          \
    if (tree->count == 0) return NULL;                                    \
                                                                          \
    NODE_TYPE *node = tree->root;                                         \
    assert(node);                                                         \
    assert(tree->cmp);                                                    \
    while (node){                                                         \
        switch(tree->cmp(key, node)){                                     \
            case LT: node = node->left; break;                            \
            case EQ: return node;                                         \
            case GT: node = node->right; break;                           \
            default: assert(false);                                       \
        }                                                                 \
    }                                                                     \
    return NULL;                                                          \
}

#define define_bst_boolean_lookup_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE) \
    vis(vistype) bool SHORTNAME##_has_node(                                  \
            const TREE_TYPE *tree,                                           \
            const NODE_TYPE *node)                                           \
{                                                                            \
    return (SHORTNAME##_find_node(tree, node) != NULL);                      \
}

#define define_bst_inorder_successor_finder_(vistype, SHORTNAME, NODE_TYPE)  \
    vis(vistype) NODE_TYPE *SHORTNAME##_find_inorder_successor(              \
            const NODE_TYPE *node)                                           \
{                                                                            \
        assert(node);                                                        \
        node = node->right;                                                  \
        while (node && node->left){                                          \
            node = node->left;                                               \
        }                                                                    \
        return (NODE_TYPE*)node;                                             \
}

#define define_bst_path_tracer_(                                        \
        vistype, SHORTNAME, TREE_TYPE,                                  \
        NODE_TYPE, WAYPOINT_TYPE)                                       \
    vis(vistype) void SHORTNAME##_trace_path_to_node(                   \
        const TREE_TYPE *tree,                                          \
        struct staq *path,                                              \
        const NODE_TYPE *src,                                           \
        const NODE_TYPE *dst)                                           \
{                                                                       \
    assert(tree);                                                       \
    assert(path);                                                       \
    assert(dst);                                                        \
    NODE_TYPE *node = src ? (NODE_TYPE*)src : tree->root;               \
                                                                        \
    Staq_init(path, path->dtor);                                        \
    WAYPOINT_TYPE *p;                                                   \
                                                                        \
    if (tree->count == 0) return;                                       \
                                                                        \
    assert(node);                                                       \
    assert(tree->cmp);                                                  \
                                                                        \
    size_t waypointsz = sizeof(WAYPOINT_TYPE);                          \
                                                                        \
    while (node){                                                       \
        switch(tree->cmp(dst, node)){                                   \
        case LT:                                                        \
            p = salloc(waypointsz, NULL);                               \
            p->ptr = node; p->dir = LEFT;                               \
            Staq_push(path, p, link);                                   \
            node = node->left;                                          \
            break;                                                      \
        case GT:                                                        \
            p = salloc(waypointsz, NULL);                               \
            p->ptr = node; p->dir = RIGHT;                              \
            Staq_push(path, p, link);                                   \
            node = node->right;                                         \
            break;                                                      \
        case EQ:                                                        \
            p = salloc(waypointsz, NULL);                               \
            p->ptr = node; p->dir = EQUAL;                              \
            Staq_push(path, p, link);                                   \
            return;                                                     \
        default: assert(false);                                         \
        }                                                               \
    }                                                                   \
}

#define define_bst_cut_down_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE)        \
    vis(vistype) void SHORTNAME##_cut_down(TREE_TYPE *tree, NODE_TYPE *root)  \
{                                                                             \
        if (!root) return;                                                    \
        SHORTNAME##_cut_down(tree, root->left);                               \
        SHORTNAME##_cut_down(tree, root->right);                              \
        tree->dtor(root);                                                     \
}


#define define_bst_rotate_left_(vistype, SHORTNAME, NODE_TYPE)         \
    vis(vistype) NODE_TYPE *SHORTNAME##_rotate_left(                   \
            NODE_TYPE *a, NODE_TYPE *b)                                \
{                                                                      \
    a->right = b->left;                                                \
    b->left = a;                                                       \
    return b;                                                          \
}

#define define_bst_rotate_right_(vistype, SHORTNAME, NODE_TYPE)        \
    vis(vistype) NODE_TYPE *SHORTNAME##_rotate_right(                  \
            NODE_TYPE *a, NODE_TYPE *b)                                \
{                                                                      \
    a->left = b->right;                                                \
    b->right = a;                                                      \
    return b;                                                          \
}

/*
 * Thin wrapper for storing strings in a dllist */
struct string_qlink {
    dlnode link;
    const char *s;
};

/*
 * Helper function to print BACKLOG -- used
 * by BST_graphed_print__() function.
 */
static inline void println__(struct dllist *list){
    assert(list);
    struct string_qlink *sql;
    Dll_foreach(list, sql, struct string_qlink, link){
        printf("%s", sql->s);
    }
}

/*
 * Pretty print of a tree to the console. For example:
 *
 *             .---- (4) bwdad
 *           .---- (3) bde
 *       .---- (2) bd
 *       :   `---- (3) bca
 *   <[(1)] bc
 *       :   .---- (3) bbb
 *       `---- (2) bb
 *           :   .---- (4) bar
 *           `---- (3) ba
 *
 * Parenthesized are the levels. Root is at level one.
 * The function is meant as a visual debugging tool. It immediately gives
 * you an idea of how balanced, or, conversely, degenerate, a tree is,
 * for example.
 *
 * --> parent
 *     The parent of the current node. Calling code should pass this as NULL
 *     since the first node is root, which doesn't have a parent. Then the
 *     function will call itself recursively starting from root.
 *
 * --> backlog
 *     A linked list/ queue of strings (dllist), that the function must print
 *     when reaching a particular node *before* printing the string
 *     representation of the node itself. See below fmi.
 *
 * --> level
 *    The current level the current node is at. Root is at level 1. Calling code
 *    should therefore pass 1 for this parameter. This will then get incremented
 *    recursively.
 *
 * --> node
 *     The current node. Calling code should pass the root of a tree object for
 *     this parameter.
 */
#define define_bst_graphic_dump_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE,   \
        PRINTER_TYPE)                                                        \
    vis(vistype) void SHORTNAME##_dump_tree_graph(                           \
            TREE_TYPE *tree,                                                 \
            NODE_TYPE *node,                                                 \
            PRINTER_TYPE node2string,                                        \
            NODE_TYPE *parent,                                               \
            struct dllist *backlog,                                          \
            size_t level)                                                    \
{                                                                            \
    if (!tree) return;                                                       \
    if (!node) return;                                                       \
                                                                             \
    char *level_trace = ":   ";                                              \
    char *padding     = "    ";                                              \
    const size_t sz = 30;                                                    \
    char str[sz];  memset(str, 0, ARRLEN(str));                              \
    char *right_fill = NULL;                                                 \
    char *left_fill = NULL;                                                  \
    level = level > 0 ? level : 1;                                           \
    struct string_qlink sql; /* static, kept in scope by recursion frames */ \
                                                                             \
                                                                             \
     /* the decision of what string to and what to pass to the children */   \
     /* is made at each node and it depends on whether the current node */   \
     /* does NOT have a parent(is root), or is a left or right child ... */  \
    if (!parent){                                                            \
        /* root */                                                           \
        const char *s = node2string(node);                                   \
        snprintf(str, sz-1, "<[(%zu)] %-6s\n", level, s);                    \
        free((void*)s);                                                      \
                                                                             \
        right_fill = left_fill = padding;                                    \
        /* info("root; fill right and left: '%s'", right_fill); */           \
    }                                                                        \
    else if (node == parent->right){                                         \
        const char *s = node2string(node);                                   \
        snprintf(str, sz-1, ".---- (%zu) %-6s\n", level, s);                 \
        free((void*)s);                                                      \
                                                                             \
        left_fill = level_trace;                                             \
        right_fill = padding;                                                \
    }                                                                        \
    else if (node == parent->left){                                          \
        const char *s = node2string(node);                                   \
        snprintf(str, sz-1, "`---- (%zu) %-6s\n", level, s);                 \
        free((void*)s);                                                      \
                                                                             \
        right_fill = level_trace;                                            \
        left_fill = padding;                                                 \
    }                                                                        \
                                                                             \
    /* print right children first (recursively), then current node, then */  \
    /* left child; since the print must be line by line, hence rightmost */  \
    /* to leftmost                                                       */  \
                                                                             \
    /* ... but the decision of whether the string constructed by the  */     \
    /* current node is passed on to the child so that they can append */     \
    /* their own string part depends on whether the node HAS a right  */     \
    /* and/or left child, respectively                                */     \
    if (node->right){                                                        \
        sql.s = right_fill;                                                  \
        Dll_pushback(backlog, &sql, link);                                   \
        SHORTNAME##_dump_tree_graph(tree,                                    \
                node->right, node2string, node, backlog, level+1);           \
        Dll_popback(backlog, struct string_qlink, link); /*pop child filler*/\
    }                                                                        \
                                                                             \
    /* root does not have a backlog */                                       \
    if (parent){                                                             \
        println__(backlog);                                                  \
    }                                                                        \
    printf("%s", str);                                                       \
                                                                             \
    if (node->left){                                                         \
        sql.s = left_fill;                                                   \
        Dll_pushback(backlog, &sql, link);                                   \
        SHORTNAME##_dump_tree_graph(tree,                                    \
                node->left, node2string, node, backlog, level+1);            \
        Dll_popback(backlog, struct string_qlink, link);/*pop child filler*/ \
    }                                                                        \
                                                                             \
    return;                                                                  \
}

#define define_bst_height_getter_(vistype, SHORTNAME, NODE_TYPE)            \
    vis(vistype) int SHORTNAME##_find_node_height(const NODE_TYPE *node){   \
        if(!node) return -1;                                                \
        return 1 + MAX(                                                     \
                SHORTNAME##_find_node_height(node->left),                   \
                SHORTNAME##_find_node_height(node->right)                   \
                );                                                          \
    }

#define define_get_nodes_at_level_(                                          \
        vistype, SHORTNAME, NODE_TYPE, PTR_WRAPPER_TYPE)                     \
    vis(vistype) void SHORTNAME##_get_nodes_at_level(                        \
            const NODE_TYPE *root,                                           \
            size_t level,                                                    \
            struct staq *list, size_t *count                                 \
            )                                                                \
{                                                                            \
    if (!root) return;                                                       \
                                                                             \
    /* reached desired level */                                              \
    if (level == 1){                                                         \
        if (list){                                                           \
            PTR_WRAPPER_TYPE *p = salloc(sizeof(NODE_TYPE), NULL);           \
            p->p = (NODE_TYPE *)root;                                        \
            Staq_enq(list, p, link);                                         \
        }                                                                    \
                                                                             \
        if (count) *count = *count+1;                                        \
        return;                                                              \
    }                                                                        \
                                                                             \
    SHORTNAME##_get_nodes_at_level(root->left, level-1, list, count);        \
    SHORTNAME##_get_nodes_at_level(root->right, level-1, list, count);       \
}

#define define_bst_max_getter_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE)   \
/* the max is the rightmost leaf */                                        \
vis(vistype) NODE_TYPE *SHORTNAME##_get_max(const TREE_TYPE *tree)         \
{                                                                          \
    assert(tree);                                                          \
    if (tree->count == 0) return NULL;                                     \
                                                                           \
    NODE_TYPE *node = tree->root;                                          \
    while (node && node->right){                                           \
        node = node->right;                                                \
    }                                                                      \
    return node;                                                           \
}

#define define_bst_min_getter_(vistype, SHORTNAME, TREE_TYPE, NODE_TYPE)    \
/* the min is the leftmost leaf */                                          \
vis(vistype) NODE_TYPE *SHORTNAME##_get_min(const TREE_TYPE *tree){         \
    assert(tree);                                                           \
    if (tree->count == 0) return NULL;                                      \
                                                                            \
    NODE_TYPE *node = tree->root;                                           \
    while (node && node->left){                                             \
        node = node->left;                                                  \
    }                                                                       \
    return node;                                                            \
}



#endif
