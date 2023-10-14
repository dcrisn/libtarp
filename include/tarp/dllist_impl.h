#ifndef TARP_DLLIST_IMPL_H
#define TARP_DLLIST_IMPL_H

#include <stdlib.h>
#include <assert.h>

struct dllist{
    size_t count;
    struct dlnode *front, *back;
};

struct dlnode {
    struct dlnode *next, *prev;
    void *container;
};

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
struct dlnode *Dll_nextnode(struct dlnode *node);
struct dlnode *Dll_prevnode(struct dlnode *node);

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

#define container(node, container_type)  ((container_type *)node->container)

/*
 * Wrappers around the peekfront and peekback functions;
 * Evaluates to a pointer not to the staq node, but to its containing
 * structure. This is NULL if the staq is empty (that is, if the
 * embedded staq node would itlself be NULL) */
#define Dll_peekfront_type(list, container_type) \
    ((Dll_empty(list)) ? NULL : container(Dll_peek_front(list), container_type))

#define Dll_peekback_type(list, container_type) \
    (Dll_empty(list) ? NULL : container(Dll_peek_back(list), container_type))

/*
 * Set the .container field of the node to point to <container>.
 * The argument <container> must be a pointer to a structure that has
 * <node> embedded inside it. */
static inline struct dlnode *Dll_prepare_node(
        struct dlnode *node, void *container)
{
    assert(node);
    node->container = container;
    return node;
}

/*
 * Wrappers around pushback, pushfront
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
#define Dll_push_back_type(list, container, dlnode_field) \
    Dll_push_back(list, Dll_prepare_node(&((container)->dlnode_field), container))

#define Dll_push_front_type(list, container, dlnode_field) \
    Dll_push_front(list, Dll_prepare_node(&((container)->dlnode_field), container))

/*
 * Wrapper around popfront.
 * list must be non-NULL but can be empty. If empty, this macro "returns"
 * NULL. Otherwise it "returns" a pointer to the struct containing the
 * dlnode that has been removed from the list
 */
#define Dll_pop_front_type(list, container_type) \
    (Dll_empty(list) ? NULL : container(Dll_pop_front(list), container_type))

#define Dll_pop_back_type(list, container_type) \
    (Dll_empty(list) ? NULL : container(Dll_pop_back(list), container_type))





#endif
