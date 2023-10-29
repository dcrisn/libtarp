#include <tarp/common.h>
#include <time.h>
#include <stdlib.h>

#include <tarp/dllist.h>
#include <tarp/heap.h>

/*
 * (1) the list_link member can be used to link a `udata` into a `dllist`.
 * (2) the heap_link member can be used to link a `udata` into a `heap`.
 * (3) use this as the heap keyrity key
 */
struct udata {
    struct dlnode    list_link;    /* (1) */
    struct heapnode  heap_link;    /* (2) */
    uint64_t key;
};

#define new_udata() calloc(1, sizeof(struct udata))



void udata_list_destructor(struct dlnode *node){
    assert(node);
    struct udata *s = container(node, struct udata, list_link);
    free(s);
}

void udata_heap_destructor(struct heapnode *node){
    assert(node);
    struct udata *s = container(node, struct udata, heap_link);
    free(s);
}

enum comparatorResult udata_heap_cmp(
    const struct heapnode *_a, const struct heapnode *_b)
{
    struct udata *a = container(_a, struct udata, heap_link);
    struct udata *b = container(_b, struct udata, heap_link);

    if (a->key > b->key) return GT;
    else if (a->key < b->key) return LT;
    else return EQ;
}

void process(const struct udata *u){
    assert(u);
    printf("u->key = %lu\n", u->key);
}


int main(int argc, char **argv){
    UNUSED(argc); UNUSED(argv);

    struct dllist list = DLLIST_INITIALIZER(udata_list_destructor); /* (1) */
    struct heap *h     = Heap_new(MIN_HEAP, udata_heap_cmp, udata_heap_destructor);

    struct udata *ud;
    time_t t; srand((unsigned) time(&t));

    /* get a list of udata elements sorted in order of 'key' */

    for (size_t i = 0; i < 10; ++i){
        ud = new_udata();
        ud->key = rand();

        Heap_push(h, ud, heap_link);
    }

    // populate list in sorted order
    while ((ud = Heap_pop(h, struct udata, heap_link))){
        Dll_pushback(&list, ud, list_link);
    }

    struct udata *i;
    Dll_foreach(&list, i, struct udata, list_link){
        process(i);   /* some arbitrary code */
    }

    Dll_clear(&list, true);
    Heap_destroy(&h, true);
}
