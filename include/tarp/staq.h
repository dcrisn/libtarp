#ifndef TARP_STAQ_H
#define TARP_STAQ_H

#include <tarp/common.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/***************************************************************************
 * Staq - a stack (LIFO) - queue (FIFO) hybrid                             |
 *                                                                         |
 * === API ===                                                             |
 *---------------------                                                    |
 *                                                                         |
 * ~ Notes ~                                                               |
 *                                                                         |
 * The staq is headed by a `struct staq` structure that can be initialized |
 * either statically on the stack or dynamically on the heap. The staq     |
 * is a linked list of 'struct staq_node' nodes. These are meant to be     |
 * intrusively embedded inside the user's own dynamically-allocated        |
 * structures. The containing structure can be obtained back from a given  |
 * staq_node as each one has a `container` field meant to point back to    |
 * its containing struct. Strictly speaking, this pointer is unnecessary   |
 * overhead (the famous container_of macro could be used, ubiquitous in    |
 * the Linux kernel's intrusive doubly linked list) but it allows for      |
 * arguably more easily readable and usable code without resorting to      |
 * too much artifice.                                                      |
 *                                                                         |
 * The staq functions (push, pop etc) take and manipulate                  |
 * `struct staq_node`s. However, it would be error prone                   |
 * and inconvenient for the user to have to use these (the contained       |
 * staq_node's must have their `container` pointer set when pushing,       |
 * then the container, which is what the user ultimately wants, must       |
 * be obtained back from the staq_node). Macros are provided instead       |
 * to make the API easier to use.                                          |
 *                                                                         |
 * Specifically, the user should use the following macros:                 |
 *  For stacks: Staq_{dup,top,bottom,push,pop}                             |
 *  For queues: Staq_{front,back,enq,dq}                                   |
 * ... instead of the functions called by the macros. All the other        |
 * functions and macros in this header file are generic can be called      |
 * directly as per their documentation.                                    |
 *                                                                         |
 *  ~ Example ~                                                            |
 *                                                                         |
 *    struct mystruct{                                                     |
 *       uint32_t u;                                                       |
 *       struct staq_node node;  // must embed a struct staq_node          |
 *    };                                                                   |
 *                                                                         |
 *    struct staq *fifo = Staq_new();       // dynamic initialization      |
 *    struct staq lifo  = STAQ_INITIALIZER; // static initialization       |
 *                                                                         |
 *    struct mystruct *a, *b, *tmp;                                        |
 *    for (size_t i=0; i< 10; ++i){                                        |
 *       a = malloc(sizeof(struct mystruct));                              |
 *       b = malloc(sizeof(struct mystruct));                              |
 *       a->u = i; b->u = i;                                               |
 *       Staq_push(&lifo, a, node);   // stack push                        |
 *       Staq_enq(fifo, b, node);    // queue enqueue                      |
 *    }                                                                    |
 *                                                                         |
 *   Staq_foreach(&lifo, tmp, struct mystruct){                            |
 *      printf("LIFO value: %u", tmp->u);                                  |
 *   }                                                                     |
 *                                                                         |
 *   for (size_t i=0; i < 10; ++i){                                        |
 *       a = Staq_pop(&lifo, struct mystruct);                             |
 *       printf("popped %u\n", a->u);                                      |
 *       free(a);                                                          |
 *                                                                         |
 *       b = Staq_dq(fifo, struct mystruct);                               |
 *       printf("dequeued %u\n", b->u);                                    |
 *       free(b);                                                          |
 *   }                                                                     |
 *                                                                         |
 *  Staq_destroy(&fifo, true);  // must destroy dynamic staq               |
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~        |
 *                                                                         |
 *                                                                         |
 * === RUNTIME COMPLEXITY CHARACTERISTICS ===                              |
 *---------------------------------------------                            |
 * Efficient implementation is provided for both FIFO and LIFO ADT         |
 * interface operations.                                                   |
 *                                                                         |
 * Staq_new                            O(1)                                |
 * Staq_destroy                        O(1)                                |
 * Staq_clear                          O(1)                                |
 * Staq_empty                          O(1)                                |
 * Staq_count                          O(1)                                |
 * Staq_{front/bottom,top/back}        O(1)                                |
 * Staq_dup                            O(1)                                |
 * Staq_push                           O(1)                                |
 * Staq_pop                            O(1)                                |
 * Staq_enq                            O(1)                                |
 * Staq_dq                             O(1)                                |
 * Staq_putafter                       O(1)                                |
 * Staq_rotate                         O(n)                                |
 * Staq_upend                          O(n)                                |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |
 *                                                                         |
 * ~ NOTES ~                                                               |
 *                                                                         |
 * The "staq" is a singly linked list, the implications of which are       |
 * well-known:                                                             |
 *  - access at an arbitrary index in the list is O(n)                     |
 *  - traversal is possible only one way                                   |
 *  - given a node, an insertion can be done after it (or before,          |
 *    depending on whether it has a next or prev pointer) but for deletion |
 *    2 pointers are required (a pointer to the neighbor either side)      |
 *  - access at a designated end is O(1)                                   |
 *                                                                         |
 * Assuming a single-pointer head structure, the above is enough for a     |
 * performant pointer-based stack (LIFO). A second pointer in the head     |
 * structure is necessary to be able to implement a Queue (FIFO).          |
 *                                                                         |
 * A standalone stack would be relatively faster but arguably not          |
 * sufficiently so to justify opting against a slightly more generic       |
 * data structure such as the one here aims to be. The 'staq' uses a       |
 * 2-pointer head structure and provides methods that support both the     |
 * FIFO and LIFO interfaces (the name is of course a blend of              |
 * stack and queue).                                                       |
 * Since lookup and access at arbitrary points in the linked list is       |
 * inefficient, these operations as well as others that are superfluous    |
 * wrt to the stack/queue interface are omitted.                           |
 *                                                                         |
 * With a 2-pointer head struct, pushing elements can be done at both      |
 * ends but popping is possible only at one. This structure is             |
 * therefore also commonly known as an output-restricted queue. The        |
 * FreeBSD man page refers to it as a "tail queue".                        |
 * See https://man.freebsd.org/cgi/man.cgi?queue.                          |
 *                                                                         |
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *************************************************************************/

struct staq;

struct staq_node {
    struct staq_node *next;
    void *container;
};

#include "staq_impl.h"

/* Macro/function initializer for static staqs; they do the same thing */
#define STAQ_INITIALIZER (struct staq){ .count=0, .front=NULL, .back=NULL }
void Staq_init(struct staq *sq);

/*
 * Get a dynamically allocated and initialized staq.
 * The user must call Staq_destroy() on this when no longer needed. */
struct staq *Staq_new(void);

/*
 * True if the non-NULL staq sq is empty, else False. */
bool Staq_empty(const struct staq *sq);

/*
 * Remove the node at the top of the stack / the front of the queue.
 * --> sq
 * non-NULL staq to remove the node from
 *
 * --> free_container
 * If true, free() is called on the container. Otherwise the structure is
 * simply unlinked and the user must call free() at some later time.
 *
 * <-- return
 * True if a removal took place. False otherwise (list was empty).
 */
bool Staq_remove(struct staq *sq, bool free_container);

/*
 * Remove each staq element in the given non-NULL staq.
 * If free_containers=True, free() is called on each container as
 * it gets unlinked. */
void Staq_clear(struct staq *sq, bool free_containers);

/*
 * Remove each staq element and then free the staq handle itself.
 * If free_containers=True, free() is called on each container as it
 * gets unlinked.
 *
 * - The sq double pointer must be be non-NULL at both levels of indirection.
 * - The sq pointer will be set to NULL before returning.
 *
 * This function should only be called on staq objects dynamically allocated
 * and initialized via Staq_new. */
void Staq_destroy(struct staq **sq, bool free_containers);

/*
 * Reverse the elements of the given non-NULL staq.
 *
 * If the elements are 1 2 3 4, this function upends them to be 4 3 2 1.
 */
void Staq_upend(struct staq *sq);

/*
 * Rotate the non-NULL staq num_rotations times in the specified direction.
 * if (dir == 1), rotate toward the top of the stack/the front of the queue.
 * if (dir ==-1), rotate toward the bottom of the stack/the back of the queue.
 */
void Staq_rotate(struct staq *sq, int dir, size_t num_rotations);

/*
 * Return the count of items currently stored by the non-NULL staq sq */
size_t Staq_count(const struct staq *sq);

/*
 * Get (peek at) the element at the stack top/bottom or queue front/back.
 * A pointer to the container of the respective staq node is returned.
 *
 * - staq must be non-NULL
 * - container type is the type of the containing structure that the
 *   staq node is embedded in.
 */
#define Staq_top(staq, container_type)    Staq_peekfront_type(staq, container_type)
#define Staq_bottom(staq, container_type) Staq_peekback_type(staq, container_type)
#define Staq_front(staq, container_type)  Staq_peekfront_type(staq, container_type)
#define Staq_back(staq, container_type)   Staq_peekback_type(staq, container_type)

/*
 * Push node onto the top of the stack.
 *
 * --> staq
 * A non-NULL staq handle.
 *
 * --> container
 * A struct variable that has a `struct staq_node` embedded in it.
 * The staq node and by extension this contaning structure will be added
 * onto the stack.
 *
 * --> field
 * The name of the field inside the container that refers to the
 * `struct staq_node`. */
#define Staq_push(staq, container, field) Staq_pushfront_type(staq, container, field)

/*
 * Pop node off the top of the stack.
 *
 * --> staq
 * A non-NULL staq handle.
 *
 * --> container_type
 * The *type* of the structure that contains the `struct staq_node`.
 *
 * <-- return
 * A pointer structure of type <container_type>. This is a pointer
 * to the structure that contains the staq node that was popped off
 * the stack. If the staq is empty, this is a NULL pointer.
 */
#define Staq_pop(staq, container_type)    Staq_popfront_type(staq, container_type)

/*
 * Enqueue or dequeue node.
 * These macros are part of the queue/FIFO interface;
 * see Staq_push, Staq_pop for information on the arguments and
 * return values. */
#define Staq_enq(staq, container, field)  Staq_pushback_type(staq, container, field)
#define Staq_dq(staq, container_type)     Staq_popfront_type(staq, container_type)

/*
 * Insert contb into the list such that it comes *after* conta, i.e.
 * closer to the bottom of the stack/the back of the queue.
 *
 * --> staq
 * a non-NULL, non-empty staq to link contb in to
 *
 * --> conta
 * a structure that contains a staq node as a field with the specified name.
 * conta must already exist in the staq.
 *
 * -->contb
 * a new structure, like conta, that is to be added to the list *after* conta,
 * as described. The macro takes care of correctly setting the <container>
 * field of the staq node embedded inside contb.
 */
#define Staq_putafter(staq, conta, contb, field) \
    Staq_put_after_type(staq, conta, contb, field)

/*
 * Iterate over each element in the queue or stack.
 * Queues are traversed front to back and stacks top to bottom.
 * i must be a pointer to structure of type <container type> that the
 * iterator can use. i will be set, in turn, to a pointer to each container
 * that has been linked into the list.
 *
 * It is safe to change the list during the iteration. Of course, the only
 * change that really makes sense is "putafter". If a new item is added after
 * 'i', the next value of 'i' is the same one it would've had if the insertion\
 * had not occurred. */
#define foreach(staq, i, container_type)                                     \
    for (                                                                    \
        struct staq_node *sqn_tmp = (staq)->front,                           \
          *sqn_tmp_next = ((sqn_tmp) ? sqn_tmp->next : NULL);                \
        (i = ((sqn_tmp) ? container(sqn_tmp, container_type): NULL));        \
        sqn_tmp=sqn_tmp_next, sqn_tmp_next=(sqn_tmp ? sqn_tmp->next : NULL)  \
            )


#ifdef __cplusplus
}  /* extern "C" */
#endif


#endif /* TARP_STAQ_H */
