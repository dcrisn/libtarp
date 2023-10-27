#ifndef TARP_STAQ_IMPL_H
#define TARP_STAQ_IMPL_H

#include <tarp/common.h>
#include <tarp/container.h>

struct staqnode {
    struct staqnode *next;
};

struct staq {
    size_t count;
    struct staqnode *front; /* front of queue, top of (upward-growing) stack */
    struct staqnode *back;  /* back of queue, bottom of stack */
    staqnode_destructor dtor;
};

typedef struct staq staq;
typedef struct staqnode staqnode;


static inline struct staq staq_initializer(staqnode_destructor dtor){
    return (struct staq){
        .count=0,
        .front=NULL,
        .back=NULL,
        .dtor = dtor
    };
}

#define STAQ_INITIALIZER__(dtor) staq_initializer(dtor)

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
struct staqnode *Staq_peekfront(const struct staq *sq);
struct staqnode *Staq_peekback(const struct staq *sq);
void Staq_pushback(struct staq *sq, struct staqnode *node);
void Staq_pushfront(struct staq *sq, struct staqnode *node);
struct staqnode *Staq_popfront(struct staq *sq);
void Staq_dup_back(struct staq *sq);
void Staq_dup_front(struct staq *sq);
void Staq_put_after(struct staq *sq, struct staqnode *x, struct staqnode *node);

/*
 * Wrappers around the peekfront and peekback functions;
 * Evaluates to a pointer not to the staq node, but to its containing
 * structure. This is NULL if the staq is empty (that is, if the
 * embedded staq node would itlself be NULL) */
#define Staq_peekfront_type(staq, container_type, field) \
    ((Staq_empty(staq)) ? NULL : container(Staq_peekfront(staq), container_type, field))

#define Staq_peekback_type(staq, container_type, field) \
    (Staq_empty(staq) ? NULL : container(Staq_peekback(staq), container_type, field))

/*
 * Wrappers around pushback, pushfront
 * These let the user pass a pointer to the container as an argument
 * instead of a pointer to the staq node embedded inside it.
 *
 * --> staq
 *  The staq handle, must be non-NULL.
 *
 * --> container
 * A pointer to a structure that has a staqnode field named sqnode_field.
 */
#define Staq_pushback_type(staq, container, sqnode_field)    \
    do {                                                     \
        assert(container);                                   \
        Staq_pushback(staq, &((container)->sqnode_field));   \
    } while(0)

#define Staq_pushfront_type(staq, container, sqnode_field)   \
    do {                                                     \
        assert(container);                                   \
        Staq_pushfront(staq, &((container)->sqnode_field));  \
    } while(0)

/*
 * Wrapper around popfront.
 * Staq must be non-NULL but can be empty. If empty, this macro "returns"
 * NULL. Otherwise it "returns" a pointer to the struct containing the
 * staq node that has been removed from the list
 */
#define Staq_popfront_type(staq, container_type, field) \
    (Staq_empty(staq) ? NULL : container(Staq_popfront(staq), container_type, field))

/*
 * Wrapper around Staq_put_after. This lets the user pass pointers to
 * containters, just like the macros above.
 * contb and conta must both have a staqnode embedded in them as a field
 * with the specified name.
 *
 * conta must already be linked in the staq. contb is linked in  *after* it.
 */
#define Staq_put_after_type(staq, conta, contb, field)       \
    do {                                                     \
        assert(conta && contb);                              \
        Staq_put_after(staq,                                 \
                &((conta)->field),                           \
                &((contb)->field));                          \
    }while(0)


#endif
