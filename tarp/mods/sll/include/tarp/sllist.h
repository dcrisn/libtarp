#ifndef __SLLIST_H__
#define __SLLIST_H__

#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include "intrusive.h"

// ===============================================================
// -------- Intrusive singly linked list implementation ----------
// ===============================================================
//
// This is heavily inspired by the Linux Kernel's intrusive linked list
// implementation, the `container_of` macro being an obvious example. 
// Where possible, I've sought to keep the names of the macros consistent 
// with the ones used there (e.g. LIST_HEAD) to improve usability for people 
// already familiar with that Linux kernel code.
// Obviously, there are differences: first of all, the kernel uses a circular
// doubly linked list whereas this is a singly linked list with an altogether
// separate head of a different type. This is to allow the head to store more 
// data (e.g. the count of items in the list) that the nodes themselves do
// not hold, with a view to improving efficiency.
// Second, the liux kernel's macros are not ISO compliant, relying on GCC
// specifics. The macros here on the other hand seek to be standards compliant
// in order to boost portability.
// ===============================================================

/*
 * Singly linked list node.
 * This must be EMBEDDED in any data structure
 * the user wants to link into a linked list.
 * Each node must embed a `struct slnode`.
 */
struct slnode{
    struct slnode *next;
};

/*
 * Singly linked list object.
 * This is the head of the list initialized with SLL_INIT().
 */
struct sllist{
    struct slnode *head;
    struct slnode *tail;
    uint32_t count;
};

/*
 * This is to be assigned to a static struct sllist declaration for initization.
 */
#define SLL_STATIC_INITIALIZER {.count = 0, .head = NULL, .tail = NULL}

/*
 * Iterator over each node object in the singly linked list.
 *
 * --> pos 
 *     A pointer to struct slnode that can be used as an 
 *     iteration variable. Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct sllist. This will have been initialized
 *     with the LIST_HEAD() macro.
 */
#define SLL_foreach(pos, list) \
    for (pos = (list)->head; pos != NULL; pos = pos->next)

/*
 * Iterator over each enclosing structure that has a node
 * object embedded in it. The embedded node is supposed
 * to be part of a singly linked list.
 *
 * --> pos 
 *     A pointer to a type of the enclosing structure that can be used 
 *     as an iteration variable. Must be an lvalue.
 *
 * --> counter
 *     A pointer to an uint32_t that can be used as an iteration variable.
 *
 * --> list
 *     A pointer to a struct sllist that represents the head of the linked 
 *     list. This will have been initialized with the LIST_HEAD() macro.
 *
 * --> member
 *     The variable that identifies the linked list node (struct slnode) within
 *     the enclosing structure.
 *
 * Notes:
 *  + notice how the various variables are parenthesized. This is because the variables
 *    are expected to be pointers, and if the user passed e.g. &myint, the macro would end
 *    up with *&myint; the derefence would fail. Using parentheses avoids this problem and
 *    ensures correct behavior: *(&myint).
 */ 
#define SLL_foreach_entry(pos, counter, list, member) \
    *(counter) = 0; \
    for (\
            pos = list_entry((list)->head, typeof(*pos), member); \
            *(counter) < (list)->count; \
            (*(counter))++, pos = list_entry(pos->member.next, typeof(*pos), member)\
            )

/*
 * Append a node (normally embedded within some sort of containing structure)
 * to the tail end of the list.
 */
void SLL_append(struct sllist *list, struct slnode *node);

/*
 * Prepend a node (normally embedded within some sort of containing structure)
 * to the tail end of the list.
 */
void SLL_prepend(struct sllist *list, struct slnode *node);

/*
 * Return the count of items currently in the list.
 */
uint32_t SLL_count(struct sllist *list);

/*
 * Remove the tail of the list and return it. 
 * The node structure returned is normally embedded within
 * some sort of containing structure. The user can obtain 
 * an address to the container by using the list_entry()
 * or container_of() macros (they're completely equivalent).
 */
struct slnode *SLL_popt(struct sllist *list);

/*
 * Remove the head of the list and return it. 
 * The node structure returned is normally embedded within
 * some sort of containing structure. The user can obtain 
 * an address to the container by using the list_entry()
 * or container_of() macros (they're completely equivalent).
 */
struct slnode *SLL_poph(struct sllist *list);

/*
 * Free() each item in the linked list. An item is 
 * a container (presumably a DYNAMICALLY allocated structure)
 * with a `struct slnode` embedded within it.
 *
 * If the containers making up the linked list have not been
 * dynamically allocated, this function should of course not be
 * called as any call to free() would in that instance be illegal.
 *
 * --> container_ptr 
 *     A pointer to a type of the enclosing structure that can be used 
 *     as an iteration variable. Must be an lvalue.
 *
 * --> node_ptr
 *     A pointer to a struct slnode that can be used as an iteration variable.
 *     Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct sllist that represents the head of the linked 
 *     list. This will have been initialized with the LIST_HEAD() macro.
 *
 * --> type
 *     The type of the enclosing cointainer with an embedded `struct slnode`.
 *
 * --> member
 *     The variable that identifies the linked list node (struct slnode) within
 *     the enclosing structure/container.
 */ 
#define SLL_destroy(container_ptr, node_ptr, list, type, member) \
    node_ptr = (list)->head; \
    while(node_ptr){  \
        container_ptr = list_entry(node_ptr, type, member); \
        node_ptr = node_ptr->next; \
        free(container_ptr); \
        (list)->count--; \
    }
    

#define DANIEL

#endif
