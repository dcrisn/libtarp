#include <tarp/staq.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool staq_empty(struct staq *s){
    return (s && s->head) ? false : true;
}

uint32_t staq_count(struct staq *s){
    return s->count;
}

struct staq_node *staq_top(struct staq *s){
        return s->head;
}

struct staq_node *staq_prev(struct staq_node *n){
    return n->prev;
}

void staq_push(struct staq *s, struct staq_node *n){
    n->prev = s->head;
    s->head = n;
    s->count++;
}

struct staq_node *staq_pop(struct staq *s){
    if (staq_empty(s)){
        return NULL;
    } 
    struct staq_node *node = s->head;
    s->head = node->prev;
    s->count--;
    return node;
}

