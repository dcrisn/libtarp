#ifndef TARP_STAQ_IMPL_H
#define TARP_STAQ_IMPL_H

#include <stdlib.h>

struct staq {
    size_t count;
    struct staq_node *front; /* front of queue, top of (upward-growing) stack */
    struct staq_node *back;  /* back of queue, bottom of stack */
};

typedef struct staq staq;

/*
 * Users should use the macro wrappers around these functions;
 *
 * NOTE these functions make reference to the front-back semantics
 * of the underlying data structure. The macros otoh are more oriented
 * to the FIFO/LIFO ADT interfaces. For example, the linked list has
 * a front/head and a back/tail, but the stack is thought of as having
 * a top and a bottom, and the macros reinforce that concept.
 *
 * NOTE
 * The name of the functions have been slightly altered (peekfront,
 * peekback instead of front, back) because their names would conflict
 * with the macros'. Since the macros rather than the functions should
 * be directly used by the user, they should get the more friendly
 * names. */
struct staq_node *Staq_peekfront(const struct staq *sq);
struct staq_node *Staq_peekback(const struct staq *sq);
void Staq_pushback(struct staq *sq, struct staq_node *node);
void Staq_pushfront(struct staq *sq, struct staq_node *node);
struct staq_node *Staq_popfront(struct staq *sq);
void Staq_dup_back(struct staq *sq);
void Staq_dup_front(struct staq *sq);
void Staq_put_after(struct staq *sq,
        struct staq_node *x, struct staq_node *node);


/* Get a pointer to the containing structure of node */
#define container(node, container_type)  ((container_type *)node->container)

/*
 * Wrappers around the peekfront and peekback functions;
 * Evaluates to a pointer not to the staq node, but to its containing
 * structure. This is NULL if the staq is empty (that is, if the
 * embedded staq node would itlself be NULL) */
#define Staq_peekfront_type(staq, container_type) \
    ((Staq_empty(staq)) ? NULL : container(Staq_peekfront(staq), container_type))

#define Staq_peekback_type(staq, container_type) \
    (Staq_empty(staq) ? NULL : container(Staq_peekback(staq), container_type))

/*
 * Set the .container field of the node to point to <container>.
 * The argument <container> must be a pointer to a structure that has
 * <node> embedded inside it. */
static inline struct staq_node *Staq_prepare_node(
        struct staq_node *node, void *container)
{
    assert(node);
    node->container = container;
    return node;
}

/*
 * Wrappers around pushback, pushfront
 * These let the user pass a pointer to the container as an argument
 * instead of a pointer to the staq node embedded inside it. They also
 * ensure the staq node has its .container field pointing back to
 * the containing structure.
 *
 * --> staq
 *  The staq handle, must be non-NULL.
 *
 * --> container
 * A pointer to a structure that has a staq_node field named sqnode_field.
 */
#define Staq_pushback_type(staq, container, sqnode_field) \
    Staq_pushback(staq, Staq_prepare_node(&((container)->sqnode_field), container))

#define Staq_pushfront_type(staq, container, sqnode_field) \
    Staq_pushfront(staq, Staq_prepare_node(&((container)->sqnode_field), container))

/*
 * Wrapper around popfront.
 * Staq must be non-NULL but can be empty. If empty, this macro "returns"
 * NULL. Otherwise it "returns" a pointer to the struct containing the
 * staq node that has been removed from the list
 */
#define Staq_popfront_type(staq, container_type) \
    (Staq_empty(staq) ? NULL : container(Staq_popfront(staq), container_type))

/*
 * Wrapper around Staq_put_after. This lets the user pass pointers to
 * containters, just like the macros above.
 * contb and conta must both have a staq node embedded in them as a field
 * with the specified name.
 * conta must already be linked in the staq. contb is linked in  *after* it.
 * The .container pointer of the staq node inside contb is correctly
 * set before linking into the list.
 */
#define Staq_put_after_type(staq, conta, contb, field)       \
        Staq_put_after(staq,                                 \
                &((conta)->field),                           \
                Staq_prepare_node(&((contb)->field), contb))


#endif
