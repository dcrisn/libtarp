#ifndef LIBTARP_STAQ__
#define LIBTARP_STAQ__

#include <stdint.h>

typedef int (*cmpf)(void *a, void *b);

#if 0
#define XHEAP_DEFINE(name, type)                \
struct name {                                  \
    uint32_t count;                            \
    struct type *root;                          \
}

#define XHEAP_INITIALIZER { .count=0, .root=NULL }

#define XHEAP_ENTRY(type) \
    struct { \
        struct type *left;  \
        struct type *right; \
    }


#define XHEAP_EMPTY(heap)         ( ! (heap)->root )
#define XHEAP_COUNT(heap)         ( (heap)->count )
#define XHEAP_ROOT(heap)          ( (heap)->root )
#define XHEAP_LEFT(entry, field)  ( (entry)->field.left )
#define XHEAP_RIGHT(entry, field) ( (entry)->field.right )

#endif

#define XHEAP_CONTAINER(node, type) ( (struct type *) (node)->container )


struct xheap;
struct xheap_node;

struct xheap {
    uint32_t count;
    struct xheap_node *root;
    uint8_t minheap: 1; /* else maxheap */
};

struct xheap_node {
    uint32_t id;
    struct xheap_node *left;
    struct xheap_node *right;
    void *container;
};


