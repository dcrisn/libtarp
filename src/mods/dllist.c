#include <tarp/common.h>
#include <tarp/dllist.h>
#include <tarp/log.h>

// equivalent to DLLIST_INITIALIZER, used to initialize statically
// declared dllists. DLLIST_INITIALIZER uses c99 designated initializers
// which will not compile with standard C++ (11)
void Dll_init(struct dllist *list){
    assert(list);
    *list = DLLIST_INITIALIZER;
}

struct dllist *Dll_new(void){
    struct dllist *list = salloc(sizeof(struct dllist), NULL);
    *list = DLLIST_INITIALIZER;
    return list;
}

void Dll_clear(struct dllist *list, bool free_containers){
    assert(list);
    if (list->count == 0) return;

    void *container;
    if (free_containers){
        Dll_foreach(list, container, void){
            free(container);
        }
    }

    *list = DLLIST_INITIALIZER;  // reset
}

void Dll_destroy(struct dllist **list, bool free_containers){
    assert(list && *list);

    Dll_clear(*list, free_containers);
    salloc(0, *list);
    *list = NULL;
}

bool Dll_empty(const struct dllist *list){
    assert(list);
    return (list->count == 0);
}

size_t Dll_count(const struct dllist *list){
    assert(list);
    return list->count;
}

struct dlnode *Dll_peek_front(const struct dllist *list){
    assert(list);
    return list->front;
}

struct dlnode *Dll_peek_back(const struct dllist *list){
    assert(list);
    return list->back;
}

struct dlnode *Dll_nextnode(struct dlnode *node){
    assert(node);
    return node->next;
}

struct dlnode *Dll_prevnode(struct dlnode *node){
    assert(node);
    return node->prev;
}

/*
 * The doubly linked nodes allow popping and insertion *anywhere*
 * in the list but in terms of the number of pointers that need
 * adjusting the operation is always more costly than the equivalent
 * operation (where possible) on a singly linked list */
struct dlnode *Dll_pop_front(struct dllist *list){
    assert(list);
    struct dlnode *node = list->front;
    switch (list->count) {
        case 0: return NULL;
        case 1:
            list->front = list->back = NULL;
            break;
        case 2:
            list->front = list->back = node->next;
            list->front->prev = list->front->next = NULL;
            break;
        default:
            list->front = node->next;
            list->front->prev = NULL;
            break;
    }
    list->count--;
    return node;
}

struct dlnode *Dll_pop_back(struct dllist *list){
    assert(list);
    struct dlnode *node = list->back;
    switch (list->count) {
        case 0: return NULL;
        case 1:
            list->front = list->back = NULL;
            break;
        case 2:
            list->front = list->back = node->prev;
            list->front->prev = list->front->next = NULL;
            break;
        default:
            list->back = node->prev;
            list->back->next = NULL;
            break;
    }
    list->count--;
    return node;
}
#if 0
// Alternative code for push/pop.
//  NOTE there is essentially no performance difference between the code
//  above and here for push/pop. However, the switch-based code is
//  clearer as to the different cases that must be considered.
struct dlnode *Dll_pop_back(struct dllist *list){
    assert(list);
    if (list->count == 0) return NULL;

    struct dlnode *back = list->back;
    struct dlnode *prev = back->prev;
    list->back = prev; // 1
    if (prev != NULL) prev->next = NULL;
    if (list->front == back) list->front = prev;

    list->count--;
    return back;
}

struct dlnode *Dll_pop_front(struct dllist *list){
    assert(list);
    if (list->count == 0) return NULL;

    struct dlnode *front = list->front;
    struct dlnode *next = front->next;
    list->front = next; // 1
    if (next != NULL) next->prev = NULL;
    if (list->back == front) list->back = next;

    list->count--;
    return front;
}
#endif

void Dll_push_front(struct dllist *list, struct dlnode *node){
    assert(list);
    assert(node);

    node->prev = NULL;
    node->next = list->front;
    list->front = node;
    if (list->count == 0) list->back = list->front;
    else                  list->front->next->prev = list->front;
    list->count++;
}

void Dll_push_back(struct dllist *list, struct dlnode *node){
    assert(list);
    assert(node);

    node->next = NULL;
    node->prev = list->back;
    list->back = node;
    if (list->count == 0) list->front = list->back;
    else                  list->back->prev->next = list->back;
    list->count++;
}

// take a node out from the list and optionally free() its container
void Dll_remove_node(struct dllist *list, struct dlnode *node, bool free_container)
{
    assert(list);
    assert(node);
    assert(list->count > 0);

    if (node == list->front){
        Dll_pop_front(list);
    } else if (node == list->back){
        Dll_pop_back(list);
    } else {
        node->next->prev = node->prev;
        node->prev->next = node->next;
        list->count--;
    }

    if (free_container) free(node->container);
}

// pop the specified node from the list, but do *not* free() it
void Dll_pop_node(struct dllist *list, struct dlnode *node){
    Dll_remove_node(list, node, false);
}

// pop back AND free it
void Dll_remove_back(struct dllist *list){
    assert(list);
    if (list->count == 0) return;

    Dll_remove_node(list, Dll_peek_back(list), true);
}

// pop front AND free it
void Dll_remove_front(struct dllist *list){
    assert(list);
    if (list->count == 0) return;

    Dll_remove_node(list, Dll_peek_front(list), true);
}

void Dll_put_node_after(struct dllist *list, struct dlnode *a, struct dlnode *b){
    assert(list);
    assert(a); assert(b);

    if (a == list->back){
        Dll_push_back(list, b);
        return;
    }

    b->next = a->next;
    b->next->prev = b;
    b->prev = a;
    a->next = b;
    list->count++;
}

void Dll_put_node_before(struct dllist *list, struct dlnode *a, struct dlnode *b){
    assert(list);
    assert(a); assert(b);

    if (a == list->front){
        Dll_push_front(list, b);
        return;
    }

    b->prev = a->prev;
    b->prev->next = b;
    b->next = a;
    a->prev = b;
    list->count++;
}

/* replace a with b in the list; a must be in the list, b must not be*/
void Dll_replace_node(struct dllist *list, struct dlnode *a, struct dlnode *b){
    assert(list);
    assert(a); assert(b);
    b->next = a->next;
    b->prev = a->prev;

    if (b->next) b->next->prev = b;
    else list->back = b;

    if (b->prev) b->prev->next = b;
    else list->front = b;
}

// a and b must be in the same list and not be the same node
void Dll_swap_list_nodes(struct dllist *list, struct dlnode *a, struct dlnode *b){
    assert(list);
    assert(a); assert(b);

    struct dlnode *before_a = a->prev;
    Dll_remove_node(list, a, false);
    Dll_replace_node(list, b, a);

    if (before_a) Dll_put_node_after(list, before_a, b);
    else Dll_push_front(list, b);
}

// swap the contents of the lists a and b
void Dll_swap_list_heads(struct dllist *a, struct dllist *b){
    assert(a); assert(b);
    struct dllist tmp = {
        .count = a->count,
        .front = a->front,
        .back = a->back
    };

    a->front = b->front;
    a->back = b->back;
    a->count = b->count;

    b->front = tmp.front;
    b->back  = tmp.back;
    b->count = tmp.count;
}

// append b to a; b is reset
void Dll_join(struct dllist *a, struct dllist *b){
    assert(a);
    assert(b);

    if (a->back){ /* non-empty list */
        a->back->next = b->front;
        b->front->prev = a->back;
    } else { /* empty list */
        a->front = b->front;
        a->back = b->back;
    }

    a->count += b->count;
    *b = DLLIST_INITIALIZER; /* reset */
}

static inline size_t count_list_nodes(struct dllist *list){
    assert(list);
    struct dlnode *node = list->front;
    size_t count = 0;
    while(node){
        node = node->next;
        ++count;
    }
    return count;
}

/* split list on node just that node becomes the head of a new list.
 * Essentially:
 * a = list[:n] ; b = list[n:];
 * list = a;
 * return b;
 */
struct dllist *Dll_split_list(struct dllist *list, struct dlnode *node){
    assert(list);
    assert(node);

    size_t orig_len = list->count;
    struct dllist *b = Dll_new();

    if (node == list->front){
        Dll_swap_list_heads(list, b);
        return b;
    }

    b->front = node;
    if (!node->next) b->back = node;

    if (node == list->back){
        Dll_pop_back(list);
    } else {
        list->back = node->prev;
        node->prev->next = NULL;
    }

    node->prev = NULL;
    b->count = count_list_nodes(b);
    list->count = (orig_len - b->count);
    return b;
}

/* rotate the list so that node ends up at the front */
void Dll_rotate_to_node(struct dllist *list, struct dlnode *node){
    assert(list); assert(node);
    if (node == list->front) return;

    /* link front and back */
    list->back->next = list->front;
    list->front->prev = list->back;

    /* move head to node and tail to node->prev */
    list->front= node;
    list->back = node->prev;

    /* unlink front and back */
    list->front->prev = NULL;
    list->back->next = NULL;
}

struct dlnode *Dll_find_nth_node(const struct dllist *list, size_t n){
    assert(list);
    assert(n);
    struct dlnode *node = NULL;
    if (n == 0 || n > list->count) return NULL;

    /* start from the end closer to the node */
    if (n <= list->count/2){
        node = list->front;
        for (size_t i = 1; i < n && node != NULL; ++i, node=node->next);
        return node;
    }

    n = list->count - n; /* start from the back */
    node = list->back;
    for (size_t i = 0; i < n && node != NULL; ++i, node=node->prev);
    return node;
}

// rotate to the front if dir==1, rotate to the back if dir==-1;
// since traversal can be made either way, this is pretty efficient.
// Also the number of pointers adjusted for the actual rotation is constant.
// Essentially: 1) find node to rotate to, searching from the end closer to it
// 2) make this new node the head of the list
void Dll_rotate(struct dllist *list, int dir, size_t num_rotations){
    assert(list);

    assert(dir == 1 || dir == -1);
    if (dir == 0) return; // invalid

    if (list->count < 2 || num_rotations == 0) return; // nothing to do

    size_t remainder = num_rotations % list->count;

    /* sq->count rotations does nothing as the stack ends up unchanged;
     * so we can discount multiples of sq->count and only need to do
     * (num_rotations mod sq->count). Since a mod n == (a+kn) mod n. */
    num_rotations = remainder;
    /* no effect, stack would be left unchanged */
    if (num_rotations == 0) return;

    /* n rotations one way is the same as list->count-n rotations the other
     * way; always rotate toward the front by appropriate amount even when
     * asked to rotate toward the back */
    num_rotations = (dir==1) ? num_rotations : (list->count - num_rotations);
    //debug("final num_rotations %zu", num_rotations);
    Dll_rotate_to_node(list, Dll_find_nth_node(list, num_rotations+1));
}

void Dll_upend(struct dllist *list){
    assert(list);

    if (list->count < 2) return;
    assert(list->front);

    struct dlnode *a, *b, *c;
    a = b = c = list->front;
    while (a && a->next){
        b = a->next;
        a->next = b->next;
        b->next = c;
        if (c) c->prev = b;
        c = b;
    }

    a->next = NULL;
    b->prev = NULL;
    list->back = a;
    list->front = b;
}

