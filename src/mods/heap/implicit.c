#include <assert.h>
#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/vector.h>
#include <tarp/bits.h>

#include <tarp/heap.h>

///////////////////////////////////////
///////  IMPLICIT HEAP CODE ///////////
///////////////////////////////////////

/* define a heap vector that stores struct heapnode pointers;
 * this will store the implicit heap */
define_vector(, struct heapnode *)

/*
 * NOTE
 * The destructor may be NULL but if so, free_container should *never* be
 * passed as true, else crash in the respective function. */
void Heap_init(struct heap *h, enum heapOrder type, heap_comparator cmp,
        heapnode_destructor dtor)
{
    assert(h); assert(cmp);
    check_heap_type(type);

    h->cmp = cmp;
    h->type = type;
    h->v = Vect_new(0);
    h->dtor = dtor;
}

struct heap *Heap_new(enum heapOrder type, heap_comparator cmp,
        heapnode_destructor dtor)
{
    assert(cmp);
    check_heap_type(type);
    struct heap *h = salloc(sizeof(struct heap), NULL);
    Heap_init(h, type, cmp, dtor);
    return h;
}

void Heap_destroy(struct heap **h, bool free_containers){
    assert(h); assert(*h);

    struct heap *heap = *h;
    assert(heap->v);
    Heap_clear(*h, free_containers);

    salloc(0, heap);
    *h = NULL;
}

size_t Heap_count(const struct heap *h){
    assert(h); assert(h->v);
    return Vect_count(h->v);
}

bool Heap_empty(const struct heap *h){
    return (Heap_count(h)==0);
}

/*
 * Bubble down element in min or max heap based on predicate.
 *
 * Given a heap h and a given appropriate comparison predicate (see below[1])
 * guide the parent to its proper spot in the heap by swapping it with
 * its appropriate [2] child as many times as necessary.
 *
 * The arguments p, pi, l, li, r, ri, m, mi should be the names of aready
 * defined variables in the code invoking this macro, where:
 *  p, l, r = parent, left child, right child
 *  pi, li, ri = {parent, left child, right child} index
 *  m is either min or max depending on the predicate (see below). This is
 *  not the global heap max/min but rather the min or max among p, l and r.
 *  mi is the index of this min/max in the vector.
 *
 * [1] the predicate:
 * - for a maxheap, should be a function that when called with 2 arguments
 * (x,y) return true if x>y.
 * - for minheap returns true when x < y
 * Therefore the predicate controls whether this macro bubbles down an element
 * in a min- or max- heap, respectively.
 *
 * [2] in a minheap, m will be min(p,l,r) and if p != m, then swap it
 * with the smaller child. Do this until the ordering is fixed.
 * In a maxheap, m will be max(p,l,r).
 *
 * NOTE:
 * pi should already be initialized to an index value where the bubbling
 * should start from.
 */
#define swap_down_while_unordered(h, p, pi, l, li, r, ri, m, mi, predicate) \
    do {                                                                    \
        size_t heapsz = Heap_count(h);                                      \
        size_t last_pi = parent(heapsz-1); /* parent of last leaf */        \
        while (pi <= last_pi){                                              \
            p = Vect_get(h->v, pi);                                         \
            m = p; mi = pi;                                                 \
                                                                            \
            if (has_left(pi, heapsz)){                                      \
                li = left(pi); l = Vect_get(h->v, li);                      \
                if (predicate(l, m, h->cmp)){                               \
                    m = l; mi = li;                                         \
                }                                                           \
            }                                                               \
                                                                            \
            if (has_right(pi, heapsz)){                                     \
                ri = right(pi); r = Vect_get(h->v, ri);                     \
                if (predicate(r, m, h->cmp)){                               \
                    m = r; mi = ri;                                         \
                }                                                           \
            }                                                               \
                                                                            \
            if (p == m){                                                    \
                assert(pi == mi); return; /* already in right place */      \
            }                                                               \
                                                                            \
            assert(pi != mi);                                               \
            Vect_set(h->v, pi, m);                                          \
            Vect_set(h->v, mi, p);                                          \
            p->idx = mi;                                                    \
            m->idx = pi;                                                    \
            pi = mi;                                                        \
        }                                                                   \
    } while (0)


/*
 * Much like swap_down_while_unordered with the following differences:
 * - c, ci are the child and child index respectively. ci itself should
 *   already be initialized by calling code to the correct index value
 *   bubbling up should start from.
 *
 * The element at child index will be bubbled up to its correct position
 * by swapping it with its parent as many times as needed.
 */
#define swap_up_while_unordered(h, p, pi, c, ci, predicate)                 \
    do {                                                                    \
        while (has_parent(ci)){                                             \
            pi = parent(ci);                                                \
            p = Vect_get(h->v, pi);                                         \
            c = Vect_get(h->v, ci);                                         \
                                                                            \
            if (!predicate(c, p, h->cmp)) return;                           \
                                                                            \
            Vect_set(h->v, pi, c);                                          \
            Vect_set(h->v, ci, p);                                          \
            c->idx = pi;                                                    \
            p->idx = ci;                                                    \
            ci = pi;                                                        \
        }                                                                   \
    } while (0)


static void bubble_down(struct heap *h, size_t index){
    assert(h);
    if (Heap_empty(h)) return;

    struct heapnode *p, *l, *r;
    size_t pi, li, ri;  /* parent, left child, right child indices */

    /* used to determine the local min/max between a parent and its children */
    struct heapnode *m; /* min or max */
    size_t mi;          /* min or max index */

    pi = index;

    switch (h->type) {
    case MIN_HEAP:
        swap_down_while_unordered(h, p, pi, l, li, r, ri, m, mi, lt);
        return;
    case MAX_HEAP:
        swap_down_while_unordered(h, p, pi, l, li, r, ri, m, mi, gt);
        return;
    default: assert(false);
    }
}

static void bubble_up(struct heap *h, size_t index){
    assert(h);
    if (Heap_empty(h)) return;
    struct heapnode *p, *c;
    size_t pi, ci;    /* parent index, child index */

    ci = index;

    switch (h->type) {
    case MIN_HEAP:
        swap_up_while_unordered(h, p, pi, c, ci, lt);
        return;
    case MAX_HEAP:
        swap_up_while_unordered(h, p, pi, c, ci, gt);
        return;
    default: assert(false);
    }
}

struct heapnode *Heap_get_root(const struct heap *h){
    assert(h);
    if (Vect_isempty(h->v)) return NULL;
    return Vect_front(h->v);
}

struct heapnode *Heap_pop_root(struct heap *h){
    assert(h);

    /* pop front, make last item the root, then heapify */
    struct heapnode *top = Vect_front(h->v);
    struct heapnode *last = Vect_popb(h->v);

    if (top == last) return top;  /* only one element */

    Vect_set(h->v, 0, last);
    bubble_down(h, 0);
    return top;
}

int Heap_push_node(struct heap *h, struct heapnode *node){
    assert(h);
    assert(node);

    int rc;
    if ( (rc = Vect_pushb(h->v, node)) == ERRORCODE_SUCCESS){
        node->idx = Vect_count(h->v) - 1;
        bubble_up(h, Heap_count(h)-1);
    }

    return rc;
}

void Heap_clear(struct heap *h, bool free_containers){
    assert(h);

    if (free_containers){
        THROWS(ERROR_MISCONFIGURED, h->dtor==NULL, "missing destructor");
        struct heapnode **s = Vect_begin(h->v), **e = Vect_end(h->v);
        for (; s != e; ++s){
            h->dtor(*s);
        }
    }
    Vect_destroy(&h->v);
}

void Heap_remove_node(struct heap *h, struct heapnode *node, bool free_container)
{
    assert(h);
    assert(node);

    size_t index = node->idx;

    if (isroot(index)){
        Heap_pop_root(h);
    } else {
        Vect_set(h->v, index, Vect_back(h->v));
        Vect_popb(h->v);
        bubble_down(h, index);
    }

    if (free_container){
        THROWS(ERROR_MISCONFIGURED, h->dtor==NULL, "missing destructor");
        h->dtor(node);
    }
}

// bubble up or down to restore the heap property depending on the type
// of heap this is
void Heap_priochange(struct heap *h, struct heapnode *node){
    assert(h);
    assert(node);
    check_heap_type(h->type);

    struct heapnode *p;
    enum comparatorResult cmp;

    size_t index = node->idx;
    if (has_parent(index)){
        p = Vect_get(h->v, parent(index));
        cmp = h->cmp(node, p);
    } else {
        /* if no parent, set cmp in such a way as to call bubble_down in
         * the switch below */
        if (h->type==MIN_HEAP) cmp = GT;
        else                   cmp = LT;
    }

    switch(h->type){

    case MIN_HEAP:
        switch(cmp){
        case EQ: return;
        case LT:
            bubble_up(h, index);
            return;
        case GT:
            bubble_down(h, index);
            return;
        default: assert(false);
        }

    case MAX_HEAP:
        switch(cmp){
        case EQ: return;
        case LT:
            bubble_down(h, index);
            return;
        case GT:
            bubble_up(h, index);
            return;
        default: assert(false);
        }

    default: assert(false);
    }
}

