#ifndef TARP_DLLIST_H
#define TARP_DLLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#include "dllist_impl.h"

/***************************************************************************
 * Dllist - general-purpose doubly-linked list                             |
 *                                                                         |
 * === API ===                                                             |
 *---------------------                                                    |
 *                                                                         |
 * ~ Notes ~                                                               |
 *                                                                         |
 * The dllist is headed by a `struct dllist` structure that can be         |
 * initialized either statically on the stack or dynamically on the heap.  |
 * The dllist, as the name imlplies, is a doubly-linked list of            |
 * `struct dlnode`s intrusively embedded inside the user's own             |
 * dynamically-allocated structures.                                       |
 * See the `staq` documentation FMI on this.                               |
 *                                                                         |
 * See the `staq` documentation FMI on the API: macros are provided        |
 * to make the API more usable and stable and users are expected to        |
 * use these rather than the functions called by the macros. E.g. use      |
 * Dll_nodeswap(), not Dll_swap_list_nodes.                                |
 * Specifically, only functions and macros defined in *this* header file   |
 * only are considered part of the public API and the user should stick    |
 * to using these.                                                         |
 *                                                                         |
 *  ~ Example ~                                                            |
 *                                                                         |
 *    struct mystruct{                                                     |
 *       uint32_t u;                                                       |
 *       struct dlnode node;  // must embed a struct dlnode                |
 *    };                                                                   |
 *                                                                         |
 *    struct dllist *fifo = Dll_new();          // dynamic initialization  |
 *    struct dllist lifo  = DLLIST_INITIALIZER; // static initialization   |
 *                                                                         |
 *    struct mystruct *a, *b, *tmp;                                        |
 *    for (size_t i=0; i< 10; ++i){                                        |
 *       a = malloc(sizeof(struct mystruct));                              |
 *       b = malloc(sizeof(struct mystruct));                              |
 *       a->u = i; b->u = i;                                               |
 *       Dll_pushback(&lifo, a, node);  // stack push                      |
 *       Dll_pushback(fifo, b, node);   // queue enqueue                   |
 *    }                                                                    |
 *                                                                         |
 *   Dll_foreach(&lifo, tmp, struct mystruct){                             |
 *      printf("LIFO value: %u", tmp->u);                                  |
 *   }                                                                     |
 *                                                                         |
 *   for (size_t i=0; i < 10; ++i){                                        |
 *       a = Dll_popback(&lifo, struct mystruct);                          |
 *       printf("popped %u\n", a->u);                                      |
 *       free(a);                                                          |
 *                                                                         |
 *       b = Dll_popfront(fifo, struct mystruct);                          |
 *       printf("dequeued %u\n", b->u);                                    |
 *       free(b);                                                          |
 *   }                                                                     |
 *                                                                         |
 *  Dll_destroy(&fifo, true);  // must destroy dynamic dllist              |
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~        |
 *                                                                         |
 *                                                                         |
 * === RUNTIME COMPLEXITY CHARACTERISTICS ===                              |
 *---------------------------------------------                            |
 * Compared to the staq, the dllist has two link pointers per node. At     |
 * the cost of additional space and pointer-manipulation overhead, the     |
 * structure is more general and allows:                                   |
 * - insertion and deletion at either end (deque)                          |
 * - removal of any node; insertion before and after any node. This is     |
 *   possible even while using the foreach macro iteration helpers.        |
 * - more efficient bidirectional rotation + rotation to specific node     |
 * - bidirectional iteration                                               |
 *                                                                         |
 * Dll_new                      O(1)                                       |
 * Dll_destroy                  O(n) // constant if list empty             |
 * Dll_clear                    O(n)                                       |
 * Dll_count                    O(1)                                       |
 * Dll_empty                    O(1)                                       |
 * Dll_{front,back}             O(1)                                       |
 * Dll_pushfront                O(1)                                       |
 * Dll_pushback                 O(1)                                       |
 * Dll_popfront                 O(1)                                       |
 * Dll_popback                  O(1)                                       |
 * Dll_popnode                  O(1)                                       |
 * Dll_foreach                  O(n)                                       |
 * Dll_foreach_reverse          O(n)                                       |
 * Dll_next                     O(1)                                       |
 * Dll_prev                     O(1)                                       |
 * Dll_delfront                 O(1)                                       |
 * Dll_delback                  O(1)                                       |
 * Dll_delnode                  O(1)                                       |
 * Dll_put_after                O(1)                                       |
 * Dll_put_before               O(1)                                       |
 * Dll_replace                  O(1)                                       |
 * Dll_nodeswap                 O(1)                                       |
 * Dll_listswap                 O(1)                                       |
 * Dll_join                     O(1)                                       |
 * Dll_split                    O(n)                                       |
 * Dll_upend                    O(n)                                       |
 * Dll_rotate                   O(n)                                       |
 * Dll_rotateto                 O(1)                                       |
 * Dll_find_nth                 O(n)                                       |
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ |
 *                                                                         |
 * ~ NOTES ~                                                               |
 *                                                                         |
 * Some of the operations are perhaps more efficient than their big-O      |
 * complexity suggests.                                                    |
 * - Dll_rotate is linear simply because it calls Dll_find_nth. LESS than  |
 *   the length of the list is traversed exactly once. The number of       |
 *   pointers changed for the actual rotation is otherwise constant.       |
 * - Dll_destroy and Dll_clear only take linear time if the list is not    |
 *   empty. This is because it must be traversed to free the nodes.        |
 *   If the nodes need not be freed or if the list is empty, their runtime |
 *   is constant.                                                          |
 * - Dll_split is linear but LESS than the length of the original list is  |
 *   traversed exactly once. Pointer manipulation for the actual split is  |
 *   otherwise constant.                                                   |
 *************************************************************************/

typedef struct dllist dllist;
typedef struct dlnode dlnode;


/* Macro/function initializer for static dllists; they do the same thing */
#define DLLIST_INITIALIZER (struct dllist){ .count=0, .front=NULL, .back=NULL }
void Dll_init(struct dllist *list);

/*
 * Get a dynamically allocated and initialized dllist.
 * The user must call Dll_destroy() on this when no longer needed. */
struct dllist *Dll_new(void);

/*
 * Remove all nodes from the list and optionally free() the containers.
 *  - list must be non-NULL */
void Dll_clear(struct dllist *list, bool free_containers);

/*
 * Remove all nodes from the list and optionally free() the containers;
 * then free the memory that had been allocated for the dllist handle
 * itself as returned by Dll_new, then finally set the list pointer to NULL.
 * - list must not be NULL and must point to a valid dllist handle returned
 *   by Dll_new. */
void Dll_destroy(struct dllist **list, bool free_containers);

/* Return number of elements in the non-NULL list */
size_t Dll_count(const struct dllist *list);

/* True if the non-NULL list is empty else false */
bool Dll_empty(const struct dllist *list);

/*
 * Get the next element after cont or the one previous one before it.
 * cont must be a pointer to structure that contains an embedded
 * dlnode as a field with the specified name, and it must currently
 * exist in a dllist. */
#define Dll_next(cont, dlnode_field) (Dll_nextnode(&((cont)->dlnode_field)))
#define Dll_prev(cont, dlnode_field) (Dll_prevnode(&((cont)->dlnode_field)))

/*
 * Get the front or back, without removing it, of the specified list.
 * The list must be non-NULL but can be empty.
 *
 * --> container_type
 * the type of the structure that has the dlnode embedded inside it
 *
 * <-- return
 * A pointer to the structure containing the dlnode at the front/back of
 * the list. This is of type <container_type>. This is NULL if the list
 * is empty */
#define Dll_front(list, container_type)  Dll_peekfront_type(list, container_type)
#define Dll_back(list, container_type)   Dll_peekback_type(list, container_type)

/*
 * Push a container to the front/back of the specified list.
 * The list must be non-NULL but can be empty.
 * --> container, field
 * container is a pointer to structure that has a dlnode embedded inside it
 * as a field with the specified name. */
#define Dll_pushback(list, container, field)  Dll_push_back_type(list, container, field)
#define Dll_pushfront(list, container, field) Dll_push_front_type(list, container, field)

/*
 * Pop a container from the front/back of the specified list.
 * The list must be non-NULL but can be empty.
 * <-- return
 * A pointer to a structure of type <container_type> that has a dlnode
 * embedded inside it. */
#define Dll_popback(list, container_type)     Dll_pop_back_type(list, container_type)
#define Dll_popfront(list, container_type)    Dll_pop_front_type(list, container_type)

/* Remove the specified container from the list.
 * The list must be non-NULL.
 * cont is a non-NULL pointer to a structure containing a dlnode as a field
 * with the specified name, and *must* exist in the list. */
#define Dll_popnode(list, cont, dlnode_field) Dll_pop_node(list, &((cont)->field))

/*
 * Iterate over each element in the list, front to back.
 * --> list
 * A non-NULL but potentially empty list to traverse.
 *
 * --> i
 * A pointer to a structure of type <container_type> that contains a dlnode.
 * This will be used as a cursor for the iteration. i will be set, in turn,
 * to each container in the list and then finally set to NULL.
 *
 * The current container (i) is *safe to change*, using one of the following:
 *  - Dll_popnode, Dll_delnode, Dll_put_before, Dll_put_after.
 * No other changes should be made to any other parts of the list or the list
 * head. */
#define Dll_foreach(list, i, container_type)                                 \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->front,                              \
          *dln_tmp_next = ((dln_tmp) ? dln_tmp->next : NULL);                \
        (i = ((dln_tmp) ? container(dln_tmp, container_type): NULL));        \
        dln_tmp=dln_tmp_next, dln_tmp_next=(dln_tmp ? dln_tmp->next : NULL)  \
            )

/* Iterate over each element in the list, back to front.
 * See Dll_foreach FMI. */
#define Dll_foreach_reverse(list, i, container_type)                         \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->back,                               \
          *dln_tmp_prev = ((dln_tmp) ? dln_tmp->prev : NULL);                \
        (i = ((dln_tmp) ? container(dln_tmp, container_type): NULL));        \
        dln_tmp=dln_tmp_prev, dln_tmp_prev=(dln_tmp ? dln_tmp->prev : NULL)  \
            )

/*
 * Join the non-NULL lists a and b such that b's front is joined to a's back */
void Dll_join(struct dllist *a, struct dllist *b);

/*
 * Split the non-NULL list at cont.
 * --> cont
 * A non-NULL pointer to a structure that contains an embedded
 * dlnode as a field with the specified name. It must exist in the given
 * list.
 *
 * <-- return
 * A dynamically allocated dllist with cont at its front. cont and all
 * the elements after it are cut out from the given list and returned
 * in a new dllist. The handle returned must be destroyed with
 * Dll_destroy when no longer needed. */
#define Dll_split(list, cont, dlnode_field)           \
    Dll_split_list(list, &((cont)->dlnode_field))

/*
 * Rotate the non-NULL list in the specified direction <num_rotations> times.
 * if dir=1, rotate toward the front of the list; if dir=-1, rotate toward
 * the back. */
void Dll_rotate(struct dllist *list, int dir, size_t num_rotations);

/*
 * Reverse the elements of the non-NULL list. */
void Dll_upend(struct dllist *list);

/*
 * Get the nth element in the non-NULL list.
 * --> n (0 < n <= Dll_count(list))
 * The nth element to get. The first element is "1".
 *
 * <-- return
 * a pointer to a structure of type <container_type> that has a
 * dlnode embedded inside it. NULL if list is empty. */
#define Dll_find_nth(list, n, container_type)                 \
    ((n==0 || n > Dll_count(list)) ? NULL                     \
      : container(Dll_find_nth_node(list,n), container_type))

/*
 * Rotate the non-NULL list until <container> ends up at the front.
 * --> container
 * A non-NULL pointer to a structure that contains a dlnode embedded
 * inside it as a field with the specified name. The node must exist
 * in the list. */
#define Dll_rotateto(list, container, dlnode_field)             \
    Dll_rotate_to_node(list, &((container)->dlnode_field))

/*
 * Remove conta from the non-NULL list and replace it with contb.
 * conta and contb are non-NULL pointers to structures of the same
 * type that contain an embedded dlnode as a field with the specified
 * name.
 * conta *must* exist in the list. contb *must not* exist in the list.
 */
#define Dll_replace(list, conta, contb, dlnode_field) \
    Dll_replace_node(list, &((conta)->dlnode_field), Dll_prepare_node(&((contb)->dlnode_field), contb))

/*
 * Replace a with b and b with a in the given non-NULL list.
 * See Dll_replace FMI, but NOTE conta and contb *both* must
 * exist in the list */
#define Dll_nodeswap(list, conta, contb, dlnode_field) \
    Dll_swap_list_nodes(list, &((conta)->dlnode_field), &((contb)->dlnode_field))

/*
 * Swap the non-NULL lists a and b.
 * The contents of a are moved to b and vice versa. */
#define Dll_listswap(a, b) Dll_swap_list_heads(a, b)

/*
 * Insert contb after conta in the specified non-NULL list.
 * conta must exist in the list, contb must *not* exist in the list.
 */
#define Dll_put_after(list, conta, contb, dlnode_field)             \
        Dll_put_node_after(list,                                    \
                &((conta)->dlnode_field),                           \
                Dll_prepare_node(&((contb)->dlnode_field), contb))

/*
 * Insert contb before conta in the specified non-NULL list.
 * See Dll_put_after fmi. */
#define Dll_put_before(list, conta, contb, dlnode_field)           \
        Dll_put_node_before(list,                                  \
                &((conta)->dlnode_field),                          \
                Dll_prepare_node(&((contb)->dlnode_field), contb))

/*
 * Remove *and free()* the specified node cont from the list.
 * - both cont and list must be non-NULL valid pointers and cont
 *   must exist in the list */
#define Dll_delnode(list, cont, dlnode_field)                      \
    Dll_remove_node(list, &((cont)->dlnode_field), true)

/*
 * Remove *and free* the element at the front/back of the non-NULL list. */
#define Dll_delfront(list) Dll_remove_front(list)
#define Dll_delback(list) Dll_remove_back(list)


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
