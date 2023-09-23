#ifndef LIBTARP_STAQ__
#define LIBTARP_STAQ__

#include <stdint.h>
#include <stdbool.h>

/*
 * Declare a head staq structure that holds / points to entries
 * of type TYPE.
 */
struct staq {                               
    uint32_t count;                            
    struct staq_node *head; /* top */               
    struct staq_tail *tail; /* unused for staqs*/  
};

struct staq_node {
    struct staq_node *prev;
    void *container;
};

#define STAQ_INITIALIZER { .count=0, .head=NULL, .tail=NULL }

#define STAQ_NODE_INITIALIZE(node, field) \
    do { \
        (node)->field.prev = NULL; \
        (node)->field.container = node; \
    } while(0)

#define STAQ_CONTAINER(node, type) ( (struct type *) (node)->container )

bool staq_empty(struct staq *s);
uint32_t staq_count(struct staq *s);
struct staq_node *staq_top(struct staq *s);
struct staq_node *staq_prev(struct staq_node *n);
void staq_push(struct staq *s, struct staq_node *n);
struct staq_node *staq_pop(struct staq *s);


#endif
