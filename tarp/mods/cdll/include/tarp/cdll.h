#ifndef __dllist_H__
#define __dllist_H__

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include <tarp/intrusive.h>
#include <tarp/iterator.h>

// cdll = circular doubly linked list

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

#if 0
//typedef struct cdll_node *(*cloner)(struct cdll_node *old);
typedef bool (*finder)(const struct cdll_node *node);
typedef bool (*printer)(const struct cdll_node *node);
#endif

#define CDLL_INIT(name) \
    struct cdll name = {.head = NULL, .tail = NULL, .count = 0 }

#if 0
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
#endif

#ifndef CDLL_PUSH__
#define CDLL_PUSH__

#define CDLL_PUSH(list, node) \
    do { \
        if (CDLL_empty(list)) { \
            (node)->next = (node)->prev = node; \
            (list)->head = (list)->tail = node; \
        } else { \
            (node)->next = (list)->head; \
            (node)->prev = (list)->tail; \
        } \
    } while (0)

#define CDLL_SOLE_ENTRY(node) ( (node)->next == node )
#define CDLL_CONTAINER(node, type) ( (struct type *) (node)->container)

#define CDLL_NODE_INIT(node, field) \
    do { \
    (node)->field.next = NULL; \
    (node)->field.prev = NULL; \
    (node)->field.container  = node; \
    } while (0)


#endif

int32_t CDLL_count(const struct cdll *list);
bool CDLL_empty(const struct cdll *list);
bool CDLL_pusht(struct cdll *list, struct cdll_node *node);
bool CDLL_pushh(struct cdll *list, struct cdll_node *node);
struct cdll_node *CDLL_tail(const struct cdll *list);
struct cdll_node *CDLL_head(const struct cdll *list);
struct cdll_node *CDLL_popt(struct cdll *list);
struct cdll_node *CDLL_poph(struct cdll *list);
struct cdll_node *CDLL_next(const struct cdll_node *node);
struct cdll_node *CDLL_prev(const struct cdll_node *node);


#if 0
#define CDLL_destroy(i, node, member, list) \
    do{ \
        while ((node = CDLL_popt(list))){ \
            i = list_entry((node), typeof(*i), member); \
            free(i); \
        } \
    }while(0);
#endif

#endif

