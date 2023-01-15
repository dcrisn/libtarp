#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include <tarp/circular.h>

struct circular *CDLL_mknode(void){
    return calloc(1, sizeof(struct circular));
}

struct circular_list *CDLL_mklist(void){
    struct circular_list *list= calloc(1, sizeof(struct circular_list));
    if (list){
        list->mark.next = list->mark.prev = &list->mark;
    }
    return list;
}

bool CDLL_done(const struct circular_list *list, const struct circular *node){
    if (!list || !node){
        return true;
    }
    return (node == &list->mark);
}

struct circular *CDLL_head(const struct circular_list *list){
    if (!list || list->mark.next == &list->mark)
        return NULL;

    return list->mark.next;
}

struct circular *CDLL_tail(const struct circular_list *list){
    if (!list || list->mark.prev == &list->mark)
        return NULL;

    return list->mark.prev;
}


struct circular *CDLL_next(const struct circular_list *list, const struct circular *node, bool loop){
    if (!node || !list){
        return NULL;
    } else if (CDLL_done(list, node)){
        return loop ? CDLL_head(list) : NULL;
    }

    return node->next;
}

struct circular *CDLL_prev(const struct circular_list *list, const struct circular *node, bool loop){
    if (!node || !list){
        return NULL;
    } else if (CDLL_done(list, node)){
        return loop ? CDLL_tail(list) : NULL;
    }

    return node->prev;
}

int32_t CDLL_count(const struct circular_list *list){
    if (list){
        return list->count;
    }
    return -1;
}

bool CDLL_empty(const struct circular_list *list){
    return (!CDLL_count(list));
}

bool CDLL_pusht(struct circular_list *list, struct circular *node){
    if (!list || !node) 
        return false;

    struct circular *dummy = &list->mark;
    node->next = dummy;
    node->prev = dummy->prev;
    dummy->prev = node;
    node->prev->next = node;
    list->count++;

    return true;
}

bool CDLL_pushh(struct circular_list *list, struct circular *node){
    if (!list || !node) 
        return false;

    struct circular *dummy = &list->mark;
    node->prev = dummy;
    node->next = dummy->next;
    dummy->next = node;
    node->next->prev = node;
    list->count++;

    return true;
}

struct circular *CDLL_popt(struct circular_list *list){
    if (!list || CDLL_empty(list))
        return NULL;

    struct circular *dummy = &list->mark;
    struct circular *out   = dummy->prev;   /* tail being removed and returned */
    dummy->prev = out->prev;
    dummy->prev->next = dummy;
    list->count--;
    return out;
}

struct circular *CDLL_poph(struct circular_list *list){
    if (!list || CDLL_empty(list))
        return NULL;

    struct circular *dummy = &list->mark;
    struct circular *out   = dummy->next;   /* head being removed and returned */
    dummy->next = out->next;
    dummy->next->prev = dummy;
    list->count--;
    return out;
}

// todo:
// rotate to (rotate to node such that it's the new head)
// rotate (move one left or right (specify direction))
// swap

/* insert node after head */
bool CDLL_insert_after(struct circular_list *list, struct circular *node, struct circular *new){
    if (!list || !node || !new) return false;
    node->next->prev = new;
    new->next = node->next;
    new->prev = node;
    node->next = new;
    list->count++;
    return true;
}

/* make node the new head which means make head the tail of node */
bool CDLL_insert_before(struct circular_list *list, struct circular *node, struct circular *new){
    if (!list || !node || !new) return false;
    node->prev->next= new;
    new->prev = node->prev;
    new->next = node;
    node->prev = new;
    list->count++;
    return true;
}

struct circular *CDLL_remove(struct circular_list *list, struct circular *a){
    if (!list || !a || a == &list->mark){
        return NULL;
    }
    a->prev->next = a->next;
    a->next->prev = a->prev;
    list->count--;
    return a;
}

struct circular *CDLL_find(const struct circular_list *list, finder f){
    if (f && list){
        struct circular *i = NULL;
        CDLL_foreach(i, list, FORWARD){
            if (f(i)) return i;
        }
    }
    return NULL;
}


#if 0
struct circular CDLL_join(struct circular *a, struct circular *b){
    CDLL_insert_after(a, b);
}

// todo
struct circular CDLL_clone(struct circular *list, cloner f, int *error){
    if (!f) return NULL;

    struct circular *clone = NULL;
    struct circular *i = NULL;
    struct circular *new = NULL;
   CDLL_foreach(i, list, FORWARD){
        if (! (new = f(i)) ){
           if (error) *error = 1; /* callback cloning error; user should tear down the list */
           return clone;
        }
        /* no errors */
       CDLL_append(clone, new);
    }

    return clone;
}

#endif

struct circular CDLL_print(const struct circular_list *list, printer f){
    const struct circular *i = NULL;
    CDLL_foreach(i, list, FORWARD){
        f(i);
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
