#ifndef __dllist_H__
#define __dllist_H__

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "intrusive.h"

struct dlnode{
    struct dlnode *prev;
    struct dlnode *next;
};

struct dllist{
    struct dlnode *head;
    struct dlnode *tail;
    uint32_t count;
};

typedef struct dlnode *(*cloner)(struct dlnode *old);
typedef bool (*finder)(const struct dlnode *node);
typedef bool (*printer)(const struct dlnode *node);

#define DLL_STATIC_INITIALIZER {.head=NULL, .tail=NULL, .count=0}

/*
 * Iterator over each node object in the singly linked list.
 *
 * --> pos 
 *     A pointer to struct dlnode that can be used as an 
 *     iteration variable. Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct dllist. This will have been initialized
 *     with the LIST_HEAD() macro.
 */
#define DLL_foreach(i, list, dir) \
    for (\
            i = (dir < 0) ? DLL_tail((list)) : DLL_head((list)); \
            i != NULL; \
            i = (dir < 0) ? DLL_prev(i) : DLL_next(i) \
            )

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
 *     A pointer to a struct dllist that represents the head of the linked 
 *     list. This will have been initialized with the LIST_HEAD() macro.
 *
 * --> member
 *     The variable that identifies the linked list node (struct dlnode) within
 *     the enclosing structure.
 *
 * Notes:
 *  + notice how the various variables are parenthesized. This is because the variables
 *    are expected to be pointers, and if the user passed e.g. &myint, the macro would end
 *    up with *&myint; the derefence would fail. Using parentheses avoids this problem and
 *    ensures correct behavior: *(&myint).
 */ 

#define DLL_foreach_entry(i, list, member, dir) \
    for ( \
            i = list_entry((dir < 0 ? DLL_tail(list) : DLL_head(list)), typeof(*i), member); \
            i && (&i->member); \
            i = list_entry((dir < 0 ? DLL_prev(&i->member) : DLL_next(&i->member)), typeof(*i), member) \
    )

/*
 * Return the count of items currently in the list.
 */
uint32_t DLL_count(const struct dllist *list);
bool DLL_empty(const struct dllist *list);
struct dlnode *DLL_head(const struct dllist *node);
struct dlnode *DLL_tail(const struct dllist *node);
struct dlnode *DLL_next(const struct dlnode *node);
struct dlnode *DLL_prev(const struct dlnode *node);

/*
 * Append a node (normally embedded within some sort of containing structure)
 * to the tail end of the list.
 */
bool DLL_append(struct dllist *list, struct dlnode *node);

/*
 * Prepend a node (normally embedded within some sort of containing structure)
 * to the tail end of the list.
 */
bool DLL_prepend(struct dllist *list, struct dlnode *node);

/*
 * Remove the tail of the list and return it. 
 * The node structure returned is normally embedded within
 * some sort of containing structure. The user can obtain 
 * an address to the container by using the list_entry()
 * or container_of() macros (they're completely equivalent).
 */
struct dlnode *DLL_popt(struct dllist *list);

/*
 * Remove the head of the list and return it. 
 * The node structure returned is normally embedded within
 * some sort of containing structure. The user can obtain 
 * an address to the container by using the list_entry()
 * or container_of() macros (they're completely equivalent).
 */
struct dlnode *DLL_poph(struct dllist *list);

struct dllist *DLL_join(struct dllist *a, struct dllist *b);
struct dllist *DLL_clone(struct dllist *list, cloner, int *error);
struct dlnode *DLL_find(const struct dllist *list, finder);

struct dlnode *DLL_get_nth(const struct dllist *list, uint32_t n);
struct dlnode *DLL_pop(struct dllist *list, struct dlnode *node);

struct dlnode *DLL_print(const struct dllist *list, printer);

/*
 * Free() each item in the linked list. An item is 
 * a container (presumably a DYNAMICALLY allocated structure)
 * with a `struct dlnode` embedded within it.
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
 *     A pointer to a struct dlnode that can be used as an iteration variable.
 *     Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct dllist that represents the head of the linked 
 *     list. This will have been initialized with the LIST_HEAD() macro.
 *
 * --> type
 *     The type of the enclosing cointainer with an embedded `struct dlnode`.
 *
 * --> member
 *     The variable that identifies the linked list node (struct dlnode) within
 *     the enclosing structure/container.
 */ 
#define DLL_destroy(i, list, member, freeheader) \
    do { \
        DLL_foreach_entry(i, list, member, 1){ \
            free( list_entry(DLL_next(&i->member), typeof(*i), member) ); \
        }; \
        if (freeheader) free(list); \
    } while(0);

#endif
