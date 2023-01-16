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

/*
 * Given a stack of items n1, n2, n3, n4, .. , nN with the stack
 * top at nN, reverse the stack orientation such that nN becomes
 * the bottom of a stack growing from nN, .. , through to
 * n4, n3, n2, n1, with the new stack top at n1.
 */
#define STACK_UPEND(stack, type, field)                      \
    do {                                                     \
        struct type *curr, *next, *dummy = NULL;             \
        curr  = STACK_TOP(stack);                            \
                                                             \
        while (curr) {                                       \
            next = (curr)->field.prev;                       \
            (curr)->field.prev = dummy;                      \
            dummy = curr;                                    \
            curr = next;                                     \
        }                                                    \
                                                             \
        (stack)->top = dummy;                                \
                                                             \
    } while (0)


/*
 * todo
 * Stack_peek()
 * Stack_concat ie stack_append  (b to a, make b empty)
 *
 *
 * + add 'n' argument to stack_peek, stack_pop, stack_push etc such that you either
 * pop the NTH item from the top downward, or you pop off n items altogether etc
 */
#endif
