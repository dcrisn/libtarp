#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

enum {FORWARD=1, BACKWARD=-1};

#include "dllist.h"

struct dlnode *DLL_mknode(void){
    return calloc(1, sizeof(struct dlnode));
}

uint32_t DLL_count(const struct dllist *list){
    if (!list) return -1;
    return list->count;
}

bool DLL_empty(const struct dllist *list){
    return list ? !DLL_count(list) : false;
}

struct dlnode *DLL_head(const struct dllist *list){
    return list ? list->head : NULL;
}

struct dlnode *DLL_tail(const struct dllist *list){
    return list ? list->tail : NULL;
}

struct dlnode *DLL_next(const struct dlnode *node){
    return node ? node->next : NULL;
}

struct dlnode *DLL_prev(const struct dlnode *node){
    return node ? node->prev : NULL;
}

bool DLL_append(struct dllist *list, struct dlnode *node){
    if (!list || !node) return false;

    if (DLL_empty(list)){
        list->head = list->tail = node;
        node->prev = NULL;
    }else{
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    node->next = NULL;
    list->count++;
    return true;
}

bool DLL_prepend(struct dllist *list, struct dlnode *node){
    if (!list || !node) return false;

    if (DLL_empty(list)){
        list->head = list->tail = node;
        node->next = NULL;
    }else{
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }

    node->prev = NULL;
    list->count++;
    return true;
}

struct dlnode *DLL_popt(struct dllist *list){
    if (!list || DLL_empty(list)) return NULL;

    struct dlnode *tail = list->tail;
    list->count--;
    list->tail = tail->prev;
    if (list->tail){ 
        list->tail->next = NULL;
    }else{ /* empty list */
        list->head = NULL;
    }

    tail->prev = NULL;
    return tail;
}

struct dlnode *DLL_poph(struct dllist *list){
    if (!list || DLL_empty(list)) return NULL;

    struct dlnode *head = list->head;
    list->count--;
    list->head = head->next;
    if (list->head){ 
        list->head->prev = NULL;
    }else{ /* empty list */
        list->tail = NULL;
    }

    head->next = NULL;
    return head;
}

struct dllist *DLL_join(struct dllist *a, struct dllist *b){
    if (!(a && b)){
        return NULL;
    } else if(DLL_empty(a)){
        return b;
    } else if(DLL_empty(b)){
        return a;
    } else {
        a->tail->next  = b->head;
        b->tail = b->tail; 
        /* disable b */
        b->head = b->tail = NULL;
        b->count = 0;
    }
    return a;
}

struct dllist *DLL_clone(struct dllist *list, cloner f, int *error){
    if (!f) return NULL;

    struct dllist *clone = NULL;
    struct dlnode *i = NULL;
    struct dlnode *new = NULL;
    DLL_foreach(i, list, FORWARD){
        if (! (new = f(i)) ){
           if (error) *error = 1; /* callback cloning error; user should tear down the list */
           return clone;
        }
        /* no errors */
        DLL_append(clone, new);
    }

    return clone;
}

struct dlnode *DLL_find(const struct dllist *list, finder f){
    if (!f) return NULL;

    struct dlnode *i = NULL;
    struct dlnode *new = NULL;
    DLL_foreach(i, list, FORWARD){
        if (f(i)){
           return i;
        }
    }
    return NULL;
}

struct dlnode *DLL_print(const struct dllist *list, printer f){
    const struct dlnode *i = NULL;
    DLL_foreach(i, list, FORWARD){
        f(i);
    }
}

struct dlnode *DLL_get_nth(const struct dllist *list, uint32_t n){
    if (!list) return NULL;

    struct dlnode *node = DLL_head(list);
    uint32_t count = 0;
    while (++count < n){
        node = DLL_next(node);
        if (!node) return NULL;
    }
    return node;
}

struct dlnode *DLL_pop(struct dllist *list, struct dlnode *node){
    if (!list || !node) return NULL;

    if (node == DLL_head(list)){
        return DLL_poph(list);
    } else if (node == DLL_tail(list)){
        return DLL_popt(list);
    } else {
        /* node is not head or tail therfore it must have neighbors either side */
        assert(node->prev && node->next);
        node->prev->next = node->next;
        node->next->prev = node->prev;
        node->next = node->prev = NULL;
        return node;
    }
}

/* intrusive vs non-intrusive 
 *
 * - clone() operations or any sort of functions that would use foreach or
 *   foreach_entry internally are impossible with intrusive lists since you 
 *   would have to know what the field name IS in the cotaining struct and this
 *   is arbitrary and up to the user. 
 *
 */
