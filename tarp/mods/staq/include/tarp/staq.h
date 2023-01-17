#ifndef LIBTARP_STAQ__
#define LIBTARP_STAQ__

#include <stdint.h>

/*
 * Declare a head staq structure that holds / points to entries
 * of type TYPE.
 */
#define STAQ_DEFINE(name, type)                \
struct name {                                  \
    uint32_t count;                            \
    struct type *head; /* top */               \
    struct type *tail; /* unused for stacks*/  \
}

#define STAQ_INITIALIZER { .count=0, .head=NULL, .tail=NULL }

/*
 * You HAVE to use an anonymous struct here since our structure
 * could have arbitrary complexity e.g. contain a .next, .prev, .parent etc
 * pointers, ALL nested under the anonymous struct with name FIELD. Otherwise
 * you would need to use a macro to declare each field and then the user would
 * need to specify each of them in calls to your macros.
 */
#define STAQ_ENTRY(type) \
    struct { \
        struct type *prev; \
    }


#define STAQ_EMPTY(staq)        ( ! (staq)->head )
#define STAQ_COUNT(staq)        ( (staq)->count )
#define STAQ_TOP(staq)          ( (staq)->head )
#define STAQ_PREV(entry, field) ( (entry)->field.prev )

#define STAQ_PUSH(staq, entry, field)         \
    do {                                      \
        (entry)->field.prev = (staq)->head;   \
        (staq)->head = (entry);                 \
        (staq)->count++;                      \
    } while(0)

#define STAQ_POP(staq, entry, field)                     \
    do {                                                 \
        if (STAQ_EMPTY(staq)) {                       \
            entry = NULL; \
        } else { \
            entry = (staq)->head;                      \
            (staq)->head = STAQ_PREV( entry, field);    \
            (staq)->count--;                             \
        } \
    } while (0)

#define STAQ_RM(staq, field)                        \
    do {                                            \
        (staq)->head = (staq)->head->field.prev;  \
        (staq)->count--;                            \
    } while (0)

#define STAQ_CLEAR(staq)                          \
    do {                                          \
        (staq)->head  = NULL;                     \
        (staq)->count = 0;                        \
        (staq)->tail  = NULL; /* for queues */    \
    } while (0)

/*
 * Stacks are traversed top to bottom, and queues therefore
 * left to right i.e. head to tail.
 */
#define STAQ_FOREACH(staq, entry, field)                                      \
    for (entry = STAQ_TOP(staq); (entry != NULL); entry = (entry)->field.prev)

#define STAQ_FOREACH_SAFE(staq, entry, field, temp)                           \
    for (                                                                     \
            entry = STAQ_TOP(staq), temp = ((entry) ? (entry)->field.prev : NULL);   \
            entry != NULL;                                                    \
            entry = temp, temp = ((entry) ? (entry)->field.prev : NULL)       \
            )

/*
 * Given a staq of items n1, n2, n3, n4, .. , nN with the staq
 * top at nN, reverse the staq orientation such that nN becomes
 * the bottom of a staq growing from nN, .. , through to
 * n4, n3, n2, n1, with the new staq top at n1.
 */
#define STAQ_UPEND(staq, type, field)                        \
    do {                                                     \
        struct type *curr=NULL, *next=NULL, *dummy=NULL;   \
        curr = STAQ_TOP(staq);                     \
                                                             \
        while (curr) {                                       \
            next = (curr)->field.prev;                       \
            (curr)->field.prev = dummy;                      \
            dummy = curr;                                    \
            curr = next;                                     \
        }                                                    \
                                                             \
        (staq)->tail = (staq)->head; /* for queues */             \
        (staq)->head = dummy;   /* stack top */              \
                                                             \
    } while (0)


/*
 * MOVE all the elements from b on top of a.
 * --> todo, not done, not tested
 */
#define STAQ_JOINS(a, b, type, field)        \
    do {  \
        struct type *curr = NULL;   \
        STAQ_POP(b, curr, field); \
        while ( curr ) {   \
            STAQ_PUSH(a, curr, field);      \
            STAQ_POP(b, curr, field);       \
        } \
        (a)->count += (b)->count;           \
        (b)->count = 0;                     \
        (b)->head  = NULL;                  \
    } while (0)

/*
 * Queue Semantics 
 */
#define STAQ_HEAD(staq)     STAQ_TOP(staq)
#define STAQ_TAIL(staq)     ( (staq)->tail )

#define STAQ_ENQUEUE(staq, entry, field)      \
    do {                                      \
        (entry)->field.prev = NULL;           \
        if (STAQ_EMPTY(staq)) {                \
            (staq)->head = entry; \
        } else { \
            (staq)->tail->field.prev = entry; \
        } \
        (staq)->tail = entry; \
        (staq)->count++;                      \
    } while(0)


#define STAQ_DEQUEUE(staq, entry, field) STAQ_POP(staq, entry, field)
#define STAQ_REVERSE(staq, type, field)  STAQ_UPEND(staq, type, field)

// add head rather than tail
#define STAQ_ENQUEUE_HEAD
/* o(1) not o(n) unlike stacks -- just use tail and head pointers */

#define STAQ_JOINQ(a, b, type, field) \
    do { \
        if (STAQ_EMPTY(a)) { \
            (a)->head = (b)->head; \
            (a)->tail = (b)->tail; \
        } else { \
            (a)->tail->field.prev = (b)->head; \
        } \
        (a)->count += (b)->count;    \
        (b)->head = (b)->tail = NULL; \
        (b)->count = 0; \
    } while (0)

#define STAQ_DESTROY(staq, type, field) \
    do { \
        struct type *curr=NULL, *next=NULL; \
        STAQ_FOREACH_SAFE(staq, curr, field, next){ \
            free(curr); \
        } \
        (staq)->count = 0; \
        (staq)->head = NULL; \
        (staq)->tail = NULL; \
    } while (0)

/*
 * todo
 * staq_peek()
 * staq_concat ie staq_append  (b to a, make b empty)
 *
 *
 * + add 'n' argument to staq_peek, staq_pop, staq_push etc such that you either
 * pop the NTH item from the top downward, or you pop off n items altogether etc
 */
#endif
