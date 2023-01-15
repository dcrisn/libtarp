#ifndef __dllist_H__
#define __dllist_H__

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include <intrusive.h>
#include <iterator.h>

// cdll = circular doubly linked list

struct circular {
    struct circular *next;
    struct circular *prev;
};

struct circular_list {
    struct circular mark;
    uint32_t count; 
};

//typedef struct circular *(*cloner)(struct circular *old);
typedef bool (*finder)(const struct circular *node);
typedef bool (*printer)(const struct circular *node);

#define CDLL_INIT(name) \
    struct circular_list name = {.mark.next = &name.mark, .mark.prev = &name.mark, .count = 0 }

/*
 * Iterator over each node object in the singly linked list.
 *
 * --> pos 
 *     A pointer to struct circular that can be used as an 
 *     iteration variable. Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct circular. This will have been initialized
 *     with the LIST_HEAD() macro.
 */
#define CDLL_foreach(i, list, dir) \
    for (                                                                                  \
            i = (dir == BACKWARD ? CDLL_tail(list) : CDLL_head(list));                     \
            !CDLL_done(list, i);                                                           \
            i = (dir == BACKWARD ? CDLL_prev(list, i, false) : CDLL_next(list, i, false))  \
            )

#define CDLL_foreach_entry(i, cursor, member, list, dir) \
    for (\
            cursor = (dir == BACKWARD ? CDLL_tail(list) : CDLL_head(list)),      \
            i = (container_of(cursor, typeof(*i), member)); \
            !CDLL_done(list, cursor);                                                 \
            cursor = (dir == BACKWARD ? CDLL_prev(list, cursor, false) : CDLL_next(list,cursor, false)), \
            i = (container_of(cursor, typeof(*i), member)) \
            )

// node = container_of(start, struct testnode, link);

/*
 * Return the count of items currently in the list.
 */
int32_t CDLL_count(const struct circular_list *list);
bool CDLL_empty(const struct circular_list *list);
bool CDLL_pusht(struct circular_list *list, struct circular *node);
bool CDLL_pushh(struct circular_list *list, struct circular *node);
struct circular *CDLL_popt(struct circular_list *list);
struct circular *CDLL_poph(struct circular_list *list);
struct circular *CDLL_next(const struct circular_list *list, const struct circular *node, bool loop);
bool CDLL_done(const struct circular_list *list, const struct circular *node);
struct circular *CDLL_prev(const struct circular_list *list, const struct circular *node, bool loop);
struct circular *CDLL_tail(const struct circular_list *list);
struct circular *CDLL_head(const struct circular_list *list);
bool CDLL_insert_before(struct circular_list *list, struct circular *node, struct circular *new);
bool CDLL_insert_after(struct circular_list *list, struct circular *node, struct circular *new);
struct circular *CDLL_find(const struct circular_list *list, finder f);
struct circular *CDLL_remove(struct circular_list *list, struct circular *a);
struct circular *CDLL_mknode(void);
struct circular_list *CDLL_mklist(void);
struct circular CDLL_print(const struct circular_list *list, printer f);

/*
struct circular CDLL_join(struct circular *a, struct circular *b);
struct circular *CDLL_find(const struct circular *list, finder f);
struct circular *CDLL_remove(struct circular *a);
bool CDLL_insert_after(struct circular *head, struct circular *node);
bool CDLL_insert_before(struct circular *head, struct circular *node);
*/

/*
 * Free() each item in the linked list. An item is 
 * a container (presumably a DYNAMICALLY allocated structure)
 * with a `struct circular` embedded within it.
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
 *     A pointer to a struct circular that can be used as an iteration variable.
 *     Must be an lvalue.
 *
 * --> list
 *     A pointer to a struct circular that represents the head of the linked 
 *     list. This will have been initialized with the LIST_HEAD() macro.
 *
 * --> type
 *     The type of the enclosing cointainer with an embedded `struct circular`.
 *
 * --> member
 *     The variable that identifies the linked list node (struct circular) within
 *     the enclosing structure/container.
 */ 
#define CDLL_destroy(i, node, member, list) \
    do{ \
        while ((node = CDLL_popt(list))){ \
            i = list_entry((node), typeof(*i), member); \
            free(i); \
        } \
    }while(0);

#endif

