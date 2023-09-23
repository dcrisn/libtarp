#include <tarp/ptrstack.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

bool stack_empty(struct stack *s){
    return (s && s->head) ? false : true;
}

uint32_t stack_count(struct stack *s){
    return s->count;
}

void *stack_top(struct stack *s){
        return s->head->data;
}

void *stack_prev(struct stack_node *n){
    return n->prev->data;
}

void stack_push(struct stack *s, void *n){
    struct stack_node *node = calloc(1,sizeof(struct stack_node));
    assert(node);
    node->data = n;
    node->prev = s->head;
    s->head = node;
    s->count++;
}

void *stack_pop(struct stack *s){
    if (stack_empty(s)){
        return NULL;
    } 
    struct stack_node *node = s->head;
    s->head = node->prev;
    s->count--;
    void *d = node->data;
    free(node);
    return d;
}

