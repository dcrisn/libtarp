#ifndef TARP_HEAP_IMPL_H
#define TARP_HEAP_IMPL_H

#include <stdlib.h>

#include <tarp/vector.h>
#include <tarp/container.h>


/*
 * MACRO helpers for implicit (array-based) heap.
 *
 * Given an index i, calculate the parent, left child, or right child
 * index, respectively. i must be inside the array bounds: 0<=i<heap.size
 */
#define parent(i)               (((i)-1) >> 1)
#define left(i)                 (((i)<<1) + 1)
#define right(i)                (((i)<<1) + 2)
#define has_parent(i)           ((i)>0)   /* root is at index 0 */
#define has_left(i, heaplen)    (left(i)  < heaplen)
#define has_right(i, heaplen)   (right(i) < heaplen)
#define childless(i, heaplen)   ((i) == (heaplen-1))
#define isroot(i)               ((i)==0)


#define check_heap_type(type) assert(type==MIN_HEAP || type==MAX_HEAP)

struct heap {
    enum heapOrder type;
    heap_comparator cmp;
    struct vector *v;
    heapnode_destructor dtor;
};

struct xheap {
    enum heapOrder type;
    xheap_comparator cmp;
    xheapnode_destructor dtor;
    struct xheapnode *root;
    size_t count;
};

struct heapnode {
    size_t idx; /* much faster removal and prio change updates */
};

/* the parent pointer introduces overhead but without it changing the priority
 * of a designatated node would cost a linear search through the heap. */
struct xheapnode {
    struct xheapnode *l, *r, *p;
};


#define _Heap_top__(h, container_type, field) \
    (Heap_empty(h) ? NULL : container(Heap_get_root(h), container_type, field))

#define _Heap_pop__(h, container_type, field) \
    (Heap_empty(h) ? NULL : container(Heap_pop_root(h), container_type, field))

#define _Heap_push__(h, container, field) \
    Heap_push_node(h, &((container)->field))

#define _Heap_remove__(heap_ptr, container_ptr, field) \
    Heap_remove_node(heap_ptr, &((container_ptr)->field), false)

#define Heap_delete__(heap_ptr, container_ptr, field) \
    Heap_remove_node(heap_ptr, &((container_ptr)->field), true)

#define _Heap_update__(heap_ptr, container_ptr, field) \
    Heap_priochange(heap_ptr, &((container_ptr)->field))


/*
 * This macro assumes it has been called with correctly initialized
 * values. Specifically, cmp, arr etc must be non-NULL; index must
 * be the 0-based array index of the element that must be bubbled down,
 * and must be of type elem_type. len is the length of the array. */
#define max_heapify_down__(arr, len, elem_type, index, cmp)              \
    do {                                                                 \
        size_t idx = index;                                              \
        size_t maxidx;                                                   \
                                                                         \
        size_t last = parent(len-1); /* parent of last leaf*/            \
        elem_type max, cur;/* max is MAX(cur, cur.left, cur.right) */    \
                                                                         \
        while (idx <= last){                                             \
            max = cur = arr[idx];                                        \
            maxidx = idx;                                                \
                                                                         \
            if (has_left(idx, len)){                                     \
                tmp = arr[left(idx)]; /* compare with left child */      \
                if (lt(max, tmp, cmp)){                                  \
                    max = tmp;                                           \
                    maxidx = left(idx);                                  \
                }                                                        \
            }                                                            \
                                                                         \
            if (has_right(idx, len)){                                    \
                tmp = arr[right(idx)];                                   \
                if (lt(max, tmp, cmp)){                                  \
                    max = tmp;                                           \
                    maxidx = right(idx);                                 \
                }                                                        \
            }                                                            \
                                                                         \
            if (eq(max, cur, cmp)){                                      \
                assert(maxidx == idx);                                   \
                break;  /* already in right position */                  \
            }                                                            \
                                                                         \
            assert(maxidx != idx);                                       \
            arr[idx] = max;                                              \
            arr[maxidx] = cur;                                           \
            idx = maxidx;                                                \
        }                                                                \
                                                                         \
    } while(0)

/*
 * Note this macro does not assume anything about what the elements are
 * (that is they could be simple chars, ints etc rather than
 * struct heapnode's).
 * If used on the struct heapnode vector for example, another loop should
 * go over the elements at the end to set their index members to the
 * correct values
 *
 * array should be an already initialized non-NULL array of the
 * specified length storing elements of the specified type.
 *
 * cmp is a heapnode comparator.
 * */
#define max_heapify(array, len, elem_type, cmp)                     \
    do {                                                            \
        if (len < 2) break; /* trivially sorted */                  \
                                                                    \
        size_t j = parent(len-1); /* parent of last leaf */         \
        for (size_t i = 0; i <= j; i++){  /* from j to 0 */         \
            max_heapify_down__(array, len, elem_type, j-i, cmp);    \
        }                                                           \
    }while(0)

/*
 * A generic heapsort macro to work with any array type;
 * The macro will generate a function with name NAME that takes as
 * arguments an array of the specified length that stores elements
 * of the specified type and a 'cmp' comparator callback.
 *
 * The comparator must behave as described in tarp/common.h, except
 * it doesn't have to cast anything: its arguments a,b are already
 * of type TYPE. See the README fmi.
 */
#define gen_heapsort_routine(NAME, TYPE)                              \
void NAME(                                                            \
        TYPE *arr, size_t len,                                        \
        enum comparatorResult (*cmp)(TYPE a, TYPE b) )                \
{                                                                     \
        assert(arr);                                                  \
        if (len < 2) return;                                          \
                                                                      \
        TYPE tmp;                                                     \
                                                                      \
        /* bottom-up in-place max-heap construction */                \
        max_heapify(arr, len, TYPE, cmp);                             \
                                                                      \
        /* pop the root and fix the heap until sorted */              \
        for (size_t i = len-1; i > 0; --i){                           \
            tmp = arr[i];                                             \
            --len;                                                    \
                                                                      \
            /* move root to just past the end of the heap */          \
            arr[i] = arr[0];                                          \
                                                                      \
            /* put former last leaf at the root */                    \
            arr[0] = tmp;                                             \
            max_heapify_down__(arr, len, TYPE, 0, cmp);               \
        }                                                             \
}

#ifdef TARP_EXPLICIT_HEAP
// undefine implicit macros
#undef childless
#undef has_parent
#endif

#define _XHeap_top__(xh, container_type, field) \
    (XHeap_empty(xh) ? NULL : container(XHeap_get_root(xh), container_type, field))

#define _XHeap_pop__(xh, container_type, field) \
    (XHeap_empty(xh) ? NULL : container(XHeap_pop_root(xh), container_type, field))

#define _XHeap_push__(xh, container, field) \
    XHeap_push_node(xh, &((container)->field))

#define _XHeap_remove__(xh, container_ptr, field) \
    XHeap_remove_node(xh, &((container_ptr)->field), false)

#define _XHeap_delete__(xh, container_ptr, field) \
    XHeap_remove_node(xh, &((container_ptr)->field), true)

#define _XHeap_update__(xh, container_ptr, field) \
    XHeap_priochange(xh, &((container_ptr)->field))



#endif  /* TARP_HEAP_IMPL_H */
