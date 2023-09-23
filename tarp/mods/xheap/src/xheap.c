#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include <tarp/xheap.h>
#include <tarp/staq.h>

STAQ_DEFINE(trail, heapnode_ref);

struct heapnode_ref {
    struct xheap_node **node;
    STAQ_ENTRY(heapnode__) stack;
};

struct xheap_node *xheap_root(struct xheap *heap){
   if (!heap) return NULL; 
   return heap->root;
}

uint32_t xheap_count(struct xheap *heap){
    return heap->count;
}

static inline bool xheap_ismin(struct xheap *heap){
    return heap->minheap ? true : false;
}

// -1 means find last node
// COUNT+1 means find next insertion spot
struct trail *findpath(struct xheap *heap, int tonode){
    if (!heap) return NULL;
    
    uint32_t dest = tonode > 0 ? tonode : xheap_count(heap);
    struct xheap_node *curr = heap->root;
    struct xheap_node **curr_ref = &heap->root;
    struct trail *trail = calloc(1, sizeof(struct trail));
    STAQ_INITIALIZE(trail);

    if (!trail) return NULL;
    
    uint32_t bits = 0;
    TARP_REVBITS(dest, bits);
    bits >>= 1; /* discard root bit */
    while (*curr_ref){
        struct heapnode__ *node = calloc(1, sizeof(struct heapnode__));
        assert(node); 
        node->node = curr_ref;
        STAQ_PUSH(trail, node, stack);
        bits >>= 1;
        curr_ref = bits & 1 ? &curr->right : &curr->left;
        curr = *curr_ref;
    }

    return trail;
}

static inline void xheap_siftup(struct xheap *heap, struct trail *path, cmpf comparator){
    struct heapnode__ *wrapper = NULL;
    struct xheap_node **p = NULL, **gp = NULL;

    STAQ_POP(path, wrapper, stack);
    p = wrapper->node;
    STAQ_POP(path, wrapper, stack);
    gp = wrapper->node;
    
    if (xheap_ismin(heap)) {
        while (*p && *gp){

            struct xheap_node **smaller = &p->left;
            if (cmpf((*smaller)->container, (*p)->right->container) == 1){
                smaller = &p->right;
            }

            // parent greater, must swap
            if (cmpf((*p)->container, (*smaller)->container) == 1){
                struct xheap_node *temp = NULL;
                if ((*p)->right == *smaller) {
                    temp = (*smaller)->right;
                    (*smaller)->right = *p;
                    (*p)->right = temp;
                    temp = (*smaller)->left;
                    (*smaller)->left = (*p)->left;
                    (*p)->left = temp;
                }
                *p = *smaller;
            }
        }
        
    } 

}

int xheap_push(struct xheap *heap, struct xheap_node *node){
    struct trail *trail = findpath(heap, xheap_count(heap) + 1);
    if (trail) return -1;

    struct heapnode__ *wrapper = STAQ_TOP(trail);
    assert(parent); /* at the very least there should be root */
    struct xheap_node *parent = wrapper->node;

    if (parent->left) {
        parent->right = node;
    } else {
        parent->left = node;
    }

    heap->count++;
    
    /* restore heap property by siftup */
    xheap_siftup(trail);
    STAQ_DESTROY(trail, heapnode__, stack);
}

struct xheap_node *xheap_pop(struct xheap *heap){
    if(!heap) return NULL;
    struct xheap_node *r = heap->root;

}


// replace (NOT pop and push)
// insert
// delete max
// delete node (anywhere)
//size
// sift up
// sift down
// heapify   -- i.e. rearrange nodes to maintain or restore the heap property
