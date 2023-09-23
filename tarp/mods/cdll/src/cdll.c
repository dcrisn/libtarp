#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <tarp/cdll.h>

#if 0
struct cdll_node {
    struct cdll_node *next;
    struct cdll_node *prev;
    void *container;
};

struct cdll {
    uint32_t count; 
    struct cdll_node *head;
    struct cdll_node *tail;
};

#endif

/*
 * For all functions here node and tail must be non null; note it's the same
 * either way. If you have a potentially invalid input, you either check it
 * before passing it to these functions OR you would have to check the error 
 * code RETURNED by these functions AND the function would've had to check
 * the input for validity before returning an error -- so it's no less hassle
 * and it's added overhead too.
 */


int32_t CDLL_count(const struct cdll *list){
    return list->count;
}

// must be non null
bool CDLL_empty(const struct cdll *list){
    return (!list->head);
}


bool CDLL_pusht(struct cdll *list, struct cdll_node *node){
    if (!node) return false;

    CDLL_PUSH(list, node);
    list->tail = node;
    list->count++;

    return true; 
}

bool CDLL_pushh(struct cdll *list, struct cdll_node *node){
    if (!node) return false;

    CDLL_PUSH(list, node);
    list->head = node;
    list->count++;

    return true; 
}

struct cdll_node *CDLL_tail(const struct cdll *list){
    return list ? list->tail: NULL;
}

struct cdll_node *CDLL_head(const struct cdll *list){
    return list ? list->head : NULL;
}

struct cdll_node *CDLL_popt(struct cdll *list){
    if (list && !(CDLL_empty(list))) {
        struct cdll_node *ret = list->tail;
        if (CDLL_SOLE_ENTRY(ret)) {
            list->head = list->tail = NULL;
        } else {
            ret->prev->next = ret->next; 
            list->tail = ret->prev;
        }
        
        list->count--;
        return ret;
    }

    return NULL;
}

struct cdll_node *CDLL_poph(struct cdll *list){
    if (list && !(CDLL_empty(list))) {
        struct cdll_node *ret = list->head;
        if (CDLL_SOLE_ENTRY(ret)) {
            list->head = list->tail = NULL;
        } else {
            ret->next->prev = ret->prev; 
            list->head = ret->next;
        }
        
        list->count--;
        return ret;
    }

    return NULL;
}

struct cdll_node *CDLL_next(const struct cdll_node *node){
    return node->next;
}

struct cdll_node *CDLL_prev(const struct cdll_node *node){
    return node->prev;
}

#if 0
bool CDLL_done(const struct cdll *list, const struct cdll_node *node);
bool CDLL_insert_before(struct cdll *list, struct cdll_node *node, struct cdll_node *new);
bool CDLL_insert_after(struct cdll *list, struct cdll_node *node, struct cdll_node *new);
struct cdll_node *CDLL_find(const struct cdll *list, finder f);
struct cdll_node *CDLL_remove(struct cdll *list, struct cdll_node *a);
struct cdll_node *CDLL_mknode(void);
struct cdll *CDLL_mklist(void);
void CDLL_print(const struct cdll *list, printer f);
#endif



/* intrusive vs non-intrusive 
 *
 * - clone() operations or any sort of functions that would use foreach or
 *   foreach_entry internally are impossible with intrusive lists since you 
 *   would have to know what the field name IS in the cotaining struct and this
 *   is arbitrary and up to the user. 
 *
 */

/* head vs headless lists
 *
 * - headed lists can keep head-specific details to make various operations
 *   constant e.g. finding the count of items, getting the tail or the head
 * - headed lists have a fixed idea of where the tail and head are so enqueing 
 *   and dequeuing operation have clear semantics. A list header has a head and 
 *   a tail while nodes have next and previous pointers. Conversely, with circular
 *   listt each node has a tail and a head -- which is the same as saying they
 *   have a next and previous pointer.
 * - with circular lists you only have one type of node so function don't need
 *   to take or return separate parameters
 * - you can build any data structure on top of headless lists you don't already
 *   have a header of any certain type.
 * - with headless circular lists you can start iterating from any point and
 *   lists are easy to merge or split
 * - headless lists must maintain a dummy empty node if they want to actually
 *   have a fixed head
 * - it's hard to say what it means for a headless circular linked lists to be
 *   empty since a single node is in fact a list. but if you try to pop the tail
 *   of a single node then what? It would just return itself since it _IS_ both 
 *   the tail and the head as both pointers point to itself. And it would keep
 *   returning itself every time you do dequeue!
 * - note that both a chealess circular linked list and a headed NON-cicular
 *   linked list get both to the tail and head in constant time. But
 *   non-circular headless linked list takes linear time to get to the tail.
 */
