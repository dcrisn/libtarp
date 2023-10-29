#ifndef TARP_DLLIST_IMPL_H
#define TARP_DLLIST_IMPL_H

#include <assert.h>
#include <stdbool.h>

#include <tarp/common.h>
#include <tarp/container.h>


struct dllist{
    size_t count;
    struct dlnode *front, *back;
    dlnode_destructor dtor;
};

struct dlnode {
    struct dlnode *next, *prev;
};


static inline struct dllist initialize_dllist(dlnode_destructor dtor){
    return (struct dllist){ .count=0, .front=NULL, .back=NULL, .dtor = dtor };
}

#define DLLIST_INITIALIZER__(dtor) initialize_dllist(dtor)


/*
 * The following functions are the actual implementation of the dll. It only
 * works with struct dlnode's. These are safe to call directly but the user
 * typically cares about the payload (the intrusive container) so a macro
 * based interface is provided in the main header file.
 * The macros mainly just decorate calls to these functions such that
 * the user can pass in their payload (intrusive container pointer) and get
 * it back, as opposed to dealing with struct dlnode's.
 * Specifically: dlnode's must be "prepared" before linking in to the list
 * (see Dll_prepare_node). This would be another hassle for the user,
 * and the macros help not have to deal with that. */
struct dlnode *Dll_nextnode(const struct dlnode *node);
struct dlnode *Dll_prevnode(const struct dlnode *node);

struct dlnode *Dll_peek_front(const struct dllist *list);
struct dlnode *Dll_peek_back(const struct dllist *list);
struct dlnode *Dll_pop_back(struct dllist *list);
struct dlnode *Dll_pop_front(struct dllist *list);
void Dll_pop_node(struct dllist *list, struct dlnode *node);
void Dll_push_front(struct dllist *list, struct dlnode *node);
void Dll_push_back(struct dllist *list, struct dlnode *node);

void Dll_remove_front(struct dllist *list);
void Dll_remove_back(struct dllist *list);
void Dll_remove_node(struct dllist *list, struct dlnode *node, bool free_container);
void Dll_put_node_after(struct dllist *list, struct dlnode *a, struct dlnode *b);
void Dll_put_node_before(struct dllist *list, struct dlnode *b, struct dlnode *a);
void Dll_replace_node(struct dllist *list, struct dlnode *a, struct dlnode *b);

void Dll_swap_list_nodes(struct dllist *list, struct dlnode *a, struct dlnode *b);
void Dll_swap_list_heads(struct dllist *a, struct dllist *b);
struct dllist *Dll_split_list(struct dllist *list, struct dlnode *node);

void Dll_rotate_to_node(struct dllist *list, struct dlnode *node);
struct dlnode *Dll_find_nth_node(const struct dllist *list, size_t n);

/*
 * Wrappers around the peek_front and peek_back functions;
 * Evaluates to a pointer not to the staq node, but to its containing
 * structure. This is NULL if the staq is empty (that is, if the
 * embedded staq node would itlself be NULL) */
#define Dll_front_(list, container_type, field) \
    ((Dll_empty(list)) ? NULL : container(Dll_peek_front(list), container_type, field))

#define Dll_back_(list, container_type) \
    (Dll_empty(list) ? NULL : container(Dll_peek_back(list), container_type, field))

/*
 * Wrappers around push_back, push_front
 * These let the user pass a pointer to the container as an argument
 * instead of a pointer to the dlnode embedded inside it. They also
 * ensure the dlnode has its .container field pointing back to
 * the containing structure.
 *
 * --> staq
 *  The staq handle, must be non-NULL.
 *
 * --> container
 * A pointer to a structure that has a staq_node field named sqnode_field.
 */
#define Dll_pushback_(list, container, dlnode_field) \
    do { \
        assert(container); \
        Dll_push_back(list, &((container)->dlnode_field)); \
    }while(0)

#define Dll_pushfront_(list, container, dlnode_field) \
    do { \
        assert(container); \
        Dll_push_front(list, &((container)->dlnode_field)); \
    }while(0)

/*
 * Wrapper around pop_front.
 * list must be non-NULL but can be empty. If empty, this macro "returns"
 * NULL. Otherwise it "returns" a pointer to the struct containing the
 * dlnode that has been removed from the list
 */
#define Dll_popfront_(list, container_type, field) \
    (Dll_empty(list) ? NULL : container(Dll_pop_front(list), container_type, field))

#define Dll_popback_(list, container_type, field) \
    (Dll_empty(list) ? NULL : container(Dll_pop_back(list), container_type, field))


/*
 * Many macros in the public header will expand to these ones here;
 * this code would be distracting in the public header.
 * Specifically, many macros there have the same name as the ones here,
 * minus the trailing underscore.
 * */

#define Dll_next_(container, field) (Dll_nextnode(&((container)->dlnode_field)))
#define Dll_prev_(container, field) (Dll_prevnode(&((container)->dlnode_field)))

#define Dll_popnode_(list, cont, dlnode_field) Dll_pop_node(list, &((cont)->field))

#define Dll_foreach_(list, i, container_type, field)                         \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->front,                              \
          *dln_tmp_next = ((dln_tmp) ? dln_tmp->next : NULL);                \
        (i = ((dln_tmp) ? container(dln_tmp, container_type, field): NULL)); \
        dln_tmp=dln_tmp_next, dln_tmp_next=(dln_tmp ? dln_tmp->next : NULL)  \
            )

#define Dll_foreach_reverse_(list, i, container_type, field)                 \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->back,                               \
          *dln_tmp_prev = ((dln_tmp) ? dln_tmp->prev : NULL);                \
        (i = ((dln_tmp) ? container(dln_tmp, container_type, field): NULL)); \
        dln_tmp=dln_tmp_prev, dln_tmp_prev=(dln_tmp ? dln_tmp->prev : NULL)  \
            )

#define Dll_foreach_node_(list, i)                                           \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->front,                              \
          *dln_tmp_next = ((dln_tmp) ? dln_tmp->next : NULL);                \
        (i = ((dln_tmp) ? dln_tmp : NULL));                                  \
        dln_tmp=dln_tmp_next, dln_tmp_next=(dln_tmp ? dln_tmp->next : NULL)  \
            )

#define Dll_foreach_node_reverse_(list, i)                                   \
    for (                                                                    \
        struct dlnode *dln_tmp = (list)->back,                               \
          *dln_tmp_prev = ((dln_tmp) ? dln_tmp->prev : NULL);                \
        (i = ((dln_tmp) ? dln_tmp : NULL));                                  \
        dln_tmp=dln_tmp_prev, dln_tmp_prev=(dln_tmp ? dln_tmp->prev : NULL)  \
            )

#define Dll_find_nth_(list, n, container_type, field)                 \
    ((n==0 || n > Dll_count(list)) ? NULL                             \
      : container(Dll_find_nth_node(list,n), container_type, field))

#define Dll_rotateto_(list, container, dlnode_field)                  \
    do {                                                              \
        assert(container);                                            \
        Dll_rotate_to_node(list, &((container)->dlnode_field));       \
    }while(0)

#define Dll_replace_(list, conta, contb, dlnode_field)                \
    do {                                                              \
        assert(conta); assert(contb);                                 \
        Dll_replace_node(list, &((conta)->dlnode_field),              \
                &((contb)->dlnode_field));                            \
    }while(0)

#define Dll_put_after_(list, conta, contb, dlnode_field)              \
    do {                                                              \
        assert(conta); assert(contb);                                 \
        Dll_put_node_after(list,                                      \
                &((conta)->dlnode_field),                             \
                &((contb)->dlnode_field));                            \
    }while(0)

#define Dll_put_before_(list, conta, contb, dlnode_field)            \
    do{                                                              \
        assert(conta); assert(contb);                                \
        Dll_put_node_before(list,                                    \
                &((conta)->dlnode_field),                            \
                &((contb)->dlnode_field));                           \
    }while(0)


#endif
