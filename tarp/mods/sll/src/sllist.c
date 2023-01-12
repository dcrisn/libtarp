#include <tarp/sllist.h>

/*
 * Append node to list i.e. make node the new tail of list.
 */
void SLL_append(struct sllist *list, struct slnode *node){
    assert(list);
    assert(node);
    node->next = NULL;

    if (!list->head && !list->tail){
        list->head = list->tail = node;
    }
    else{
        list->tail->next = node;
        list->tail = node;
    }

    ++list->count;

}

/*
 * Prepend node to list i.e. make node the new head of list.
 */
void SLL_prepend(struct sllist *list, struct slnode *node){
    assert(list);
    assert(node);
    node->next = NULL;

    if (!list->head && !list->tail){
        list->head = list->tail = node;
    }
    else{
        node->next = list->head;
        list->head = node;
    }

    ++list->count;
}

/*
 * Return the count of items in the list ( |list| ).
 */
uint32_t SLL_count(struct sllist *list){
    assert(list);
    return list->count;
}

/*
 * Remove and return the current tail of the list
 * or NULL if the list is empty.
 *
 * Note that removing the tail takes linear time in the 
 * length of the list as the list must be traversed 
 * to find the node that comes before the tail being
 * removed.
 */
struct slnode *SLL_popt(struct sllist *list){
    assert(list);
    struct slnode *tail = list->tail;

    if (tail) list->count--;

    // empty list
    if (!list->count){
        list->head = list->tail = NULL; 
    }
    // only 1 item left: make it both head and tail
    else if(list->count == 1){
        list->tail = list->head;
        list->tail->next = NULL;
    }
    // more than 1 node left
    else{
        struct slnode *node = list->head;
        for (;node->next != tail; node = node->next);
        node->next = NULL;
        list->tail = node;
    }
    return tail;
}

/*
 * Remove and return the current head of the list
 * or NULL if the list is empty.
 *
 * Note that removing the head of the list always takes 
 * O(1) time and is therefore much more efficient than
 * removing the tail (O(n)). 
 */
struct slnode *SLL_poph(struct sllist *list){
    assert(list);


    struct slnode *head = list->head;
    if (head) list->count--;

    // empty list
    if (!list->count){
        list->head = list->tail = NULL; 
    }
    // only 1 item left: make it both head and tail
    else if(list->count == 1){
        list->head = list->tail;
    }
    // more than 1 node left
    else{
        list->head = head->next;
    }

    return head;
}

