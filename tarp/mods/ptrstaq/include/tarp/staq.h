#ifndef LIBTARP_stack__
#define LIBTARP_stack__

#include <stdint.h>
#include <stdbool.h>

/*
 * Declare a head stack structure that holds / points to entries
 * of type TYPE.
 */
struct stack {                               
    uint32_t count;                            
    struct stack_node *head; /* top */               
    struct stack_node *tail; /* unused for stacks*/  
};

struct stack_node {
    struct stack_node *prev;
    void *data;
};

#define stack_INITIALIZER { .count=0, .head=NULL, .tail=NULL }

/* disadvantage -- freeing internal data, lack of memory locality, must know what
 * to cast the data to */

bool stack_empty(struct stack *s);
uint32_t stack_count(struct stack *s);
void *stack_top(struct stack *s);
void *stack_prev(struct stack_node *n);
void stack_push(struct stack *s, void *data);
void *stack_pop(struct stack *s);


#endif
