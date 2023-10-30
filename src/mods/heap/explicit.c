#include <assert.h>

#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/vector.h>
#include <tarp/bits.h>

#define TARP_EXPLICIT_HEAP
#include <tarp/heap.h>

///////////////////////////////////////
///////  EXPLICIT HEAP CODE ///////////
///////////////////////////////////////

#define childless(node)           (!(node)->l && !(node)->r)
#define has_parent(node)          ((node)->p != NULL)
#define is_left_child(p, c)       ((p)->l == c)
#define is_right_child(p, c)      ((p)->r == c)

/*
 * Node should never be its own neighbor */
static inline void assert_no_direct_cycle(const struct xheapnode *node){
    assert(node);
    assert(node->p != node);
    assert(node->l != node);
    assert(node->r != node);
}

//
// Swap nodes a and b. A *may* be b's parent or vice versa but
// a and b *must not* be one and the same element.
// a must not be its own neighbor (same is true for b). This invariant
// is maintained by this routine.
//
// NOTE keeping a parent pointer allows the Heap priority change operation
// to run in O(log n) rather than O(n) but clearly the amount of pointer
// manipulation becomes costlier.
//
// NOTE swappping triple-linked nodes is only deceivingly simple: it's
// easy to introduce cycles in the special case where a is b's parent
// or vice-versa.
static inline void swap_nodes(struct xheapnode *a, struct xheapnode *b)
{
    assert_no_direct_cycle(a);
    assert_no_direct_cycle(b);

    struct xheapnode tmp;
    tmp.l = a->l;
    tmp.r = a->r;
    tmp.p = a->p;

    a->l = (b->l == a) ? b : b->l;
    a->r = (b->r == a) ? b : b->r;
    a->p = (b->p == a) ? b : b->p;

    b->l = (tmp.l == b) ? a : tmp.l;
    b->r = (tmp.r == b) ? a : tmp.r;
    b->p = (tmp.p == b) ? a : tmp.p;

    if (a->l) a->l->p = a;
    if (a->r) a->r->p = a;
    if (a->p && a->p != b){ // if a->p == b, already taken care of above
        if (a->p->l == b) a->p->l = a;
        else a->p->r = a;
    }

    if (b->l) b->l->p = b;
    if (b->r) b->r->p = b;
    if (b->p && b->p != a){  //if b->p == a, already taken care of above
        if (b->p->l == a) b->p->l = b;
        else b->p->r = b;
    }

    assert_no_direct_cycle(a);
    assert_no_direct_cycle(b);
}

/*
 * The path to the nth node in a heap can be determined by looking
 * at the bits in n. The bits to be looked at are all the bits
 * from least significant up to, but not including, the leftmost bit
 * that is non-zero. Each of these bits then, from left to right,
 * indicate the path to the nth node. 1 indicates right, 0 left.
 *
 * EXAMPLE:
 * the path to the 6th node is (right, left) because bin(6)=110, and
 * discarding the msbit leaves 10.
 *
 * NOTE:
 * In this context the nodes are counted starting from root=1, down the tree,
 * left to right:
 *                   1
 *                 /  \
 *                2    3
 *               /\
 *              4 5 ......
 *
 * --> nth (0 < nth <= XHeap_count(xh))
 */
static inline struct xheapnode *find_nth_node(
        const struct xheap *xh, size_t nth)
{
    assert(xh);

    if (xh->count == 0 || nth == 0) return NULL;
    if (nth > xh->count) return NULL;

    struct xheapnode *node = xh->root;
    assert(node);
    if (nth == 1){
        return node;
    }

    static_assert(sizeof(size_t) <= sizeof(uint64_t),\
            "sizeof(size_t)>sizeof(uint64_t) !");

    unsigned nbits_needed = (posmsb(nth));

    /* discard most significant '1' bit found; path is given by the remaining
     * bits, left to right, where 0=left, 1=right. */
    --nbits_needed;

    size_t bitpath = get_bits(nth, nbits_needed, nbits_needed);
    for (unsigned i = nbits_needed; i > 0; --i){
        int bit = get_bit(bitpath, i);
        node = (bit==ON_BIT) ? node->r : node->l;
        assert(node);
    }

    return node;
}

//
// bubble p down the heap by swapping it with its smaller (if MIN_HEAP)
// or greater (if MAX_HEAP) child. Which case it is depends on the predicate.
//
// p must already be initialized to point to the node from where the
// bubbling down is to start.
//
// See the equivalent macro in implicit.c fmi.
//
#define swap_down_while_unordered(xh, p, l, r, m, predicate)                \
    do {                                                                    \
        while (!childless(p)){                                              \
            l = p->l; r = p->r;                                             \
            assert(l!=p);                                                   \
            assert(r!=p);                                                   \
            m = p;                                                          \
                                                                            \
            if (l && predicate(l, m, xh->cmp)) m = l;                       \
            if (r && predicate(r, m, xh->cmp)) m = r;                       \
                                                                            \
            if (p == m) break; /* aready in right place */                  \
            assert(m==l || m==r);                                           \
            swap_nodes(p, m);                                               \
            if (!has_parent(m)) xh->root = m;                               \
        }                                                                   \
    } while (0)

// c must already be initialized to point to the node where the bubbling
// up is to start from. See swap_down_while_unordered above as well as
// the equivalent functions in implicit.c fmi.
#define swap_up_while_unordered(xh, p, c, predicate)                        \
    do {                                                                    \
        p = c->p;                                                           \
        while (p && predicate(c, p, xh->cmp)){                              \
            swap_nodes(p, c);                                               \
            p = c->p;                                                       \
        }                                                                   \
        if (!c->p) xh->root = c;                                            \
    } while (0)

static void bubble_down(struct xheap *xh, struct xheapnode *node){
    assert(xh);
    assert(node);
    assert(xh->count > 0);

    struct xheapnode *p, *l, *r;

    /* used to determine the local min/max between a parent and its children */
    struct xheapnode *m; /* min or max */

    p = node;

    switch (xh->type) {
    case MIN_HEAP:
        swap_down_while_unordered(xh, p, l, r, m, lt);
        return;
    case MAX_HEAP:
        swap_down_while_unordered(xh, p, l, r, m, gt);
        return;
    default: assert(false);
    }
}

static void bubble_up(struct xheap *xh, struct xheapnode *node){
    assert(xh);
    assert(xh->count > 0);

    struct xheapnode *p, *c;
    c = node;

    switch (xh->type) {
    case MIN_HEAP:
        swap_up_while_unordered(xh, p, c, lt);
        return;
    case MAX_HEAP:
        swap_up_while_unordered(xh, p, c, gt);
        return;
    default: assert(false);
    }
}

struct xheapnode *XHeap_get_root(const struct xheap *xh){
    assert(xh);
    return xh->root;
}

struct xheapnode *XHeap_pop_root(struct xheap *xh){
    assert(xh);

    if (xh->count == 0)
        return NULL;

    struct xheapnode *root, *last;
    root = xh->root;

    if (xh->count == 1){
        xh->root = NULL;
    }else {
        last = find_nth_node(xh, xh->count);
        assert(last);
        assert(last->p);
        assert(root && !childless(root));

        swap_nodes(last, root);
        xh->root = last;

        /* the popped root is now the last node; actually remove it */
        last = root;
        assert(root->p);
        if (last == last->p->l) last->p->l = NULL;
        else                    last->p->r = NULL;

        bubble_down(xh, xh->root);
    }

    xh->count--;
    return root;
}

void XHeap_init(struct xheap *xh, enum heapOrder type,
        xheap_comparator cmp,
        xheapnode_destructor dtor)
{
    assert(xh); assert(cmp);
    check_heap_type(type);

    xh->type = type;
    xh->count = 0;
    xh->root = NULL;
    xh->cmp = cmp;
    xh->dtor = dtor;

}

struct xheap *XHeap_new(enum heapOrder type, xheap_comparator cmp,
        xheapnode_destructor dtor)
{
    struct xheap *xh = salloc(sizeof(struct xheap), NULL);
    XHeap_init(xh, type, cmp, dtor);
    return xh;
}

size_t XHeap_count(const struct xheap *xh){
    assert(xh);
    return xh->count;
}

bool XHeap_empty(const struct xheap *xh){
    assert(xh);
    return xh->count == 0;
}

static void xheap_destroy__(struct xheap *h, struct xheapnode *node){
    if (!h || !node) return;

    xheap_destroy__(h, node->l);
    xheap_destroy__(h, node->r);
    h->dtor(node);
}

void XHeap_clear(struct xheap *h, bool free_containers){
    if (free_containers){
        THROWS(ERROR_MISCONFIGURED, h->dtor==NULL, "missing destructor");

        /* complete tree, so no risk of exceeding the recursion depth limit */
        xheap_destroy__(h, h->root);
    }

    h->root = NULL;
    h->count = 0;
}

void XHeap_destroy(struct xheap **xh, bool free_containers){
    assert(xh); assert(*xh);

    XHeap_clear(*xh, free_containers);

    salloc(0, *xh);
    *xh = NULL;
}

// the node pointer *is* the locator/handle of the client inside the heap.
// The key is intrinsic to the client and only maintained by the
// client.
void XHeap_push_node(struct xheap *xh, struct xheapnode *node){
    assert(xh);
    assert(node);

    xh->count++;

    if (xh->count == 1){  /* root of empty heap  */
        xh->root = node;
        node->r = node->l = node->p = NULL;
        return;
    }

    /* find the *parent* that points to the spot where the new
     * node (which will become the last leaf) must be inserted */
    struct xheapnode *p = find_nth_node(xh, (xh->count>>1));

    assert(p);

    node->p = p;
    node->r = node->l = NULL;

    if (p->l) p->r = node;
    else      p->l = node;

    bubble_up(xh, node);
}

// O(log n) because the node is found in O(1) thanks to the parent
// pointer (i.e. we don't have to start a linear search to trace
// the path to the node); else it would be O(n)
void XHeap_remove_node(
        struct xheap *xh, struct xheapnode *node, bool free_container)
{
    assert(xh); assert(node);

    if (xh->count == 0) return;

    struct xheapnode *last = find_nth_node(xh, xh->count);
    assert(last);

    if (node == xh->root){
        XHeap_pop_root(xh);
    } else {
        swap_nodes(node, last);
        /* node is now the last leaf: actually remove it */
        if (is_left_child(node->p, node)) node->p->l = NULL;
        else node->p->r = NULL;

        bubble_down(xh, last);
        xh->count--;
    }

    if (free_container){
        THROWS(ERROR_MISCONFIGURED, xh->dtor==NULL, "missing destructor");
        xh->dtor(node);
    }
}

// update the heap after a priority change at node
void XHeap_priochange(struct xheap *xh, struct xheapnode *node){
    assert(xh); assert(node);
    check_heap_type(xh->type);

    enum comparatorResult cmp;

    if (has_parent(node)){
        cmp = xh->cmp(node, node->p);
    } else {
        /* if no parent, set cmp in such a way as to call bubble_down in
         * the switch below; you can only go down */
        if (xh->type==MIN_HEAP) cmp = GT;
        else                    cmp = LT;
    }

    switch(xh->type){

    case MIN_HEAP:
        switch(cmp){
        case EQ: return;
        case LT:
            bubble_up(xh, node);
            return;
        case GT:
            bubble_down(xh, node);
            return;
        default: assert(false);
        }

    case MAX_HEAP:
        switch(cmp){
        case EQ: return;
        case LT:
            bubble_down(xh, node);
            return;
        case GT:
            bubble_up(xh, node);
            return;
        default: assert(false);
        }

    default: assert(false);
    }
}
