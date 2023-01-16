#ifndef LIBTARP_STACK__
#define LIBTARP_STACK__

#include <stdint.h>

/*
 * Declare a head stack structure that holds / points to stack entries
 * of type TYPE.
 */
#define STACK_HEAD(name, type)             \
struct name { \
    uint32_t count; \
    struct type *top; \
}

#define STACK_HEAD_STATIC_INITIALIZER { .count=0, .top=NULL }

/*
 * You HAVE to use an anonymous struct here since our structure
 * could have arbitrary complexity e.g. contain a .next, .prev, .parent etc
 * pointers, ALL nested under the anonymous struct with name FIELD. Otherwise
 * you would need to use a macro to declare each field and then the user would
 * need to specify each of them in calls to your macros.
 */
#define STACK_ENTRY(type) \
    struct { \
        struct type *prev; \
    }


#define STACK_EMPTY(stack_head)  ( ! (stack_head)->top )
#define STACK_COUNT(stack_head)  ( (stack_head)->count )
#define STACK_TOP(stack_head)    ( (stack_head)->top )
#define STACK_PREV(entry, field) ( (entry)->(field).prev )

#define STACK_PUSH(stack, entry, field) \
    do { \
        (entry)->field.prev = (stack)->top; \
        (stack)->top = entry; \
    } while(0)

#define STACK_POP(stack, entry, field) \
    do { \
        (entry) = (stack)->top; \
        (stack)->top = (stack)->top->(field).prev; \
    } while (0)

#define STACK_RM(stack, field) \
    do { \
        (stack)->top = (stack)->top->(field).prev; \
    } while (0)

#define STACK_CLEAR(stack) (stack)->top = NULL

#define STACK_FOREACH(stack, entry, field) \
    for (entry = STACK_TOP(stack); entry != NULL; entry = (entry)->field.prev)

#define STACK_FOREACH_SAFE(stack, entry, field, temp)  \
    for ( \
            entry = STACK_TOP(stack), temp = (entry)->field.prev ; \
            entry != NULL; \
            entry = temp, temp = ((entry) ? (entry)->field.prev : NULL) \
            )

#if 0
#define STACK_UPEND(stack, field, curr, next, dummy) \
    do { \
        curr = STACK_TOP(stack); \
        next = NULL; \
        dummy = NULL; \
\
        while (curr) { \
            next = (curr)->field.prev; \
            (curr)->field.prev = dummy; \
            dummy = curr; \
            curr = next; \
        } \
        (stack)->top = dummy; \
\
    } while (0)

#endif


#define STACK_UPEND(stack, type, field) \
    do { \
        struct type *curr, *next, *dummy = NULL; \
        curr = STACK_TOP(stack); \
        next = NULL; \
        dummy = NULL; \
\
        while (curr) { \
            next = (curr)->field.prev; \
            (curr)->field.prev = dummy; \
            dummy = curr; \
            curr = next; \
        } \
        (stack)->top = dummy; \
\
    } while (0)





#endif
