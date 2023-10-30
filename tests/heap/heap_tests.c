#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/staq.h>

#define TARP_EXPLICIT_HEAP
#include <tarp/heap.h>


struct testnode {
    unsigned prio;
    unsigned num;
    struct staqnode staqlink;
    struct heapnode heaplink;
    struct xheapnode xheaplink;
};

#define new_testnode() salloc(sizeof(struct testnode), NULL)

enum comparatorResult uintcmp(unsigned int a, unsigned int b){
    if (a < b) return LT;
    else if (a > b) return GT;
    else return EQ;
}

enum comparatorResult heapnodecmp(const struct heapnode *_a, const struct heapnode *_b){
    struct testnode *a = container(_a, struct testnode, heaplink);
    struct testnode *b = container(_b, struct testnode, heaplink);

    if (a->prio > b->prio) return GT;
    else if (a->prio < b->prio) return LT;
    else return EQ;
}

void testnode_heapdtor(struct heapnode *node){
    assert(node);
    salloc(0, container(node, struct testnode, heaplink));
}

gen_heapsort_routine(heapsort, unsigned int)

enum comparatorResult xheapnodecmp(const struct xheapnode *_a, const struct xheapnode *_b){
    struct testnode *a = container(_a, struct testnode, xheaplink);
    struct testnode *b = container(_b, struct testnode, xheaplink);

    if (a->prio > b->prio) return GT;
    else if (a->prio < b->prio) return LT;
    else return EQ;
}

void testnode_xheapdtor(struct xheapnode *node){
    assert(node);
    salloc(0, container(node, struct testnode, xheaplink));
}

/*
 * Tests the basics of either a minheap (type=MIN_HEAP) of maxheap
 * (type=MAX_HEAP), by doing 2 things:
 *
 * 1) supply a list of preselected inputs and compare against expected outputs;
 * A minheap should match from start to end (smallest to greatest) while a
 * maxheap should match the expected outputs in reverse.
 *
 * 2) supply a list of randomly generated inputs and check the outputs are
 * increasing (for a minheap) or decreasing (for a maxheap)
 */
enum testStatus test_minmaxheap_push_pop(enum heapOrder type, bool explicit){
    assert(type == MIN_HEAP || type == MAX_HEAP);

    struct testnode *node;
    struct heap *h = Heap_new(type, heapnodecmp, NULL);
    struct xheap *xh = XHeap_new(type, xheapnodecmp, NULL);

    // test preselected
    unsigned preselected_inputs[] = {
        1, 10, 15, 534, 21, 49, 2898, 110, 123, 1209, 919, 0, 4, 4, 1};

    unsigned expected_outputs[] = {
        0, 1, 1, 4, 4, 10, 15, 21, 49, 110, 123, 534, 919, 1209, 2898 };

    for (size_t i =0; i < ARRLEN(preselected_inputs); ++i){
        node = new_testnode();
        node->prio = preselected_inputs[i];
        if (explicit) XHeap_push(xh, node, xheaplink);
        else Heap_push(h, node, heaplink);
    }

    // test random
    unsigned actual_outputs[ARRLEN(expected_outputs)];

    for (size_t i = 0, j = ARRLEN(expected_outputs)-1; i < ARRLEN(expected_outputs); ++i, --j)
    {
        if (explicit) node = XHeap_pop(xh, struct testnode, xheaplink);
        else          node = Heap_pop(h, struct testnode, heaplink);
        assert(node);
        //debug("deleting %u", node->prio);
        // fill the array min to max left to right
        if (type == MIN_HEAP) actual_outputs[i] = node->prio;
        else                  actual_outputs[j] = node->prio;
        salloc(0, node);
    }

    enum testStatus status = TEST_PASS;
    for (size_t i = 0; i < ARRLEN(actual_outputs); ++i){
        if (expected_outputs[i] != actual_outputs[i]){
            debug("[%zu]: expected %u got %u", i, expected_outputs[i], actual_outputs[i]);
            status = TEST_FAIL;
        }
    }

    if (status == TEST_FAIL) return status;
    assert(Heap_empty(h));
    assert(XHeap_empty(xh));

    //debug("random testing");

    // test random
    time_t t; srand((unsigned) time(&t));
    size_t len = 10000;
    unsigned int *list = salloc(sizeof(unsigned int) * len, NULL);

    // populate array with random unsigned integral values
    for (unsigned i = 0; i < len; ++i){
       unsigned int number = rand() % len;
       list[i] = number;
       node = new_testnode();
       node->prio = number;
       if (explicit) XHeap_push(xh, node, xheaplink);
       else          Heap_push(h, node, heaplink);
    }

    unsigned prev = (type==MIN_HEAP) ? 0 : len;
    while (true){
        if (explicit) node = XHeap_pop(xh, struct testnode, xheaplink);
        else          node = Heap_pop(h, struct testnode, heaplink);

        if (!node){
            if (explicit) assert(XHeap_empty(xh));
            else          assert(Heap_empty(h));
            break;
        }

        if (type == MIN_HEAP){
            if (node->prio < prev){
                debug("Found invalid decreasing sequence: %u,%u", prev, node->prio);
                return TEST_FAIL;
            }
        }
        else {
            if (node->prio > prev){
                debug("found invalid increasing sequence: %u, %u", prev, node->prio);
                return TEST_FAIL;
            }
        }

        prev = node->prio;
        //debug("got %u", prev);
        salloc(0, node);
    }

    salloc(0, list);
    Heap_destroy(&h, false);
    XHeap_destroy(&xh, false);

    return TEST_PASS;
}

enum testStatus test_implicit_minheap_basics(void){
    return test_minmaxheap_push_pop(MIN_HEAP, false);
}

enum testStatus test_implicit_maxheap_basics(void){
    return test_minmaxheap_push_pop(MAX_HEAP, false);
}

enum testStatus test_explicit_minheap_basics(void){
    return test_minmaxheap_push_pop(MIN_HEAP, true);
}

enum testStatus test_explicit_maxheap_basics(void){
    return test_minmaxheap_push_pop(MAX_HEAP, true);
}

enum testStatus test_heapsort(void){
    time_t t; srand((unsigned) time(&t));

    size_t len = 1000 * 100;
    unsigned int *list = salloc(sizeof(unsigned int) * len, NULL);

    // populate array with random unsigned integral values
    for (size_t i = 0; i < len; ++i){
       unsigned int number = rand() % len;
       list[i] = number;
    }

#if 0
    info("inputs");
    for (size_t i = 0; i < len; ++i){
        info("%u", list[i]);
    }
#endif

    // sort values, check they are in increasing order
    heapsort(list, len, uintcmp);

#if 0
    info("outputs");
    for (size_t i = 0; i < len; ++i){
        info("%u", list[i]);
    }
#endif

    for (size_t i = 1 ; i < len; ++i){
        if (list[i] < list[i-1]){
            debug("non-decreasing sequence: [%zu]=%u, [%zu]=%u", i, list[i], i-1, list[i-1]);
            return TEST_FAIL;
        }
    }

    salloc(0, list);
    return TEST_PASS;
}

/*
 * Test that that heap entries can dynamically have their priority changed.
 * Do the following:
 *  - create a heap from preselected values
 *  - change the heap priority of 3 elements
 *  - verify the heap values are output in the expected sequence
 */
static enum testStatus test_heap_priochange(enum heapOrder type, bool explicit){
    struct heap h, h2;
    struct xheap xh, xh2;

    assert(type == MIN_HEAP || type == MAX_HEAP);

    Heap_init(&h, type, heapnodecmp, NULL);
    Heap_init(&h2, type, heapnodecmp, NULL);

    XHeap_init(&xh, type, xheapnodecmp, NULL);
    XHeap_init(&xh2, type, xheapnodecmp, NULL);

    struct testnode *node;

    unsigned int inputs[] = {5,1000, 12,344,1234, 3, 8, 4, 9, 35, 9923, 37,
        35, 222, 32902, 920, 9900, 99, 100, 101, 2134, 13, 88};

    struct testnode *a = salloc(sizeof(struct testnode), NULL);
    struct testnode *b = salloc(sizeof(struct testnode), NULL);
    struct testnode *c = salloc(sizeof(struct testnode), NULL);
    struct testnode *d = salloc(sizeof(struct testnode), NULL);
    a->prio = 100;
    b->prio = 768;
    c->prio = 0;
    d->prio = 7;

    if (explicit){
        XHeap_push(&xh, a, xheaplink);
        XHeap_push(&xh, b, xheaplink);
        XHeap_push(&xh, c, xheaplink);
        XHeap_push(&xh, d, xheaplink);
    } else {
        Heap_push(&h, a, heaplink);
        Heap_push(&h, b, heaplink);
        Heap_push(&h, c, heaplink);
        Heap_push(&h, d, heaplink);
    }
    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        node = new_testnode();
        node->prio = inputs[i];
        if (explicit) XHeap_push(&xh, node, xheaplink);
        else Heap_push(&h, node, heaplink);
    }

    // priorities unchanged, expect the following sequence
    unsigned int outputs[] = {0, 3, 4, 5, 7, 8, 9, 12, 13, 35, 35, 37,
        88, 99, 100, 100, 101, 222,
        344, 768, 920, 1000, 1234, 2134, 9900, 9923, 32902};

    for (size_t i = 0, j=(ARRLEN(outputs)-1); i < ARRLEN(outputs); ++i, --j){
        if (explicit) node = XHeap_pop(&xh, struct testnode, xheaplink);
        else node = Heap_pop(&h, struct testnode, heaplink);
        assert(node);
        //debug("got %u", node->prio);
        size_t idx = (type == MIN_HEAP) ? i : j;
        if (node->prio != outputs[idx]){
            debug("expected %u got %u", outputs[idx], node->prio);
            return TEST_FAIL;
        }

        // push onto second heap for an eventual second round
        if (explicit) XHeap_push(&xh2, node, xheaplink);
        else Heap_push(&h2, node, heaplink);
    }

    // change priorities of a,b,c,d,e and check the outputs sequence
    // changes as expected
    unsigned int adjusted_outputs[] = {0, 3, 4, 5, 6, 8, 9, 12, 13, 35, 35, 37,
        88, 99, 100, 101, 222,
        300, 344, 920, 1000, 1234, 2134, 9900, 9923, 32902, 40400};
    assert(ARRLEN(outputs) == ARRLEN(adjusted_outputs));

    if (explicit){
        a->prio = 0;
        XHeap_update(&xh2, a, xheaplink);
        b->prio = 6;
        XHeap_update(&xh2, b, xheaplink);
        c->prio = 300;
        XHeap_update(&xh2, c, xheaplink);
        d->prio = 40400;
        XHeap_update(&xh2, d, xheaplink);
    }
    else{
        a->prio = 0;
        Heap_update(&h2, a, heaplink);
        b->prio = 6;
        Heap_update(&h2, b, heaplink);
        c->prio = 300;
        Heap_update(&h2, c, heaplink);
        d->prio = 40400;
        Heap_update(&h2, d, heaplink);
    }

    for (size_t i = 0, j = (ARRLEN(adjusted_outputs)-1); i < ARRLEN(adjusted_outputs); ++i, --j)
    {
        if (explicit) node = XHeap_pop(&xh2, struct testnode, xheaplink);
        else node = Heap_pop(&h2, struct testnode, heaplink);
        assert(node);
        //debug("got %u", node->prio);
        size_t idx = (type == MIN_HEAP) ? i : j;
        if (node->prio != adjusted_outputs[idx]){
            debug("expected %u got %u", adjusted_outputs[idx], node->prio);
            return TEST_FAIL;
        }
        salloc(0, node);
    }

    Heap_clear(&h, false);
    Heap_clear(&h2, false);
    XHeap_clear(&xh, false);
    XHeap_clear(&xh2, false);
    return TEST_PASS;
}

enum testStatus test_implicit_minheap_priochange(void){
    return test_heap_priochange(MIN_HEAP, false);
}

enum testStatus test_implicit_maxheap_priochange(void){
    return test_heap_priochange(MAX_HEAP, false);
}

enum testStatus test_explicit_minheap_priochange(void){
    return test_heap_priochange(MIN_HEAP, true);
}

enum testStatus test_explicit_maxheap_priochange(void){
    return test_heap_priochange(MAX_HEAP, true);
}

/*
 * Test that deletion of a specified element (that may or may not be root)
 * works as expected. Do the following:
 * - build a heap from preselected elements
 * - verify expected output sequence
 * - delete certain elements
 * - verify expected output sequence (deleted elements missing)
 */
static enum testStatus test_heap_delete(enum heapOrder type, bool explicit){
    struct heap h, h2;
    struct xheap xh, xh2;

    assert(type == MIN_HEAP || type == MAX_HEAP);

    Heap_init(&h, type, heapnodecmp, NULL);
    Heap_init(&h2, type, heapnodecmp, NULL);

    XHeap_init(&xh, type, xheapnodecmp, NULL);
    XHeap_init(&xh2, type, xheapnodecmp, NULL);

    struct testnode *node;

    unsigned int inputs[] = {5,1000, 12,344,1234, 3, 8, 4, 9, 35, 9923, 37,
        35, 222, 32902, 920, 9900, 99, 100, 101, 2134, 13, 88};

    struct testnode *a = salloc(sizeof(struct testnode), NULL);
    struct testnode *b = salloc(sizeof(struct testnode), NULL);
    struct testnode *c = salloc(sizeof(struct testnode), NULL);
    struct testnode *d = salloc(sizeof(struct testnode), NULL);
    a->prio = 100;
    b->prio = 768;
    c->prio = 0;
    d->prio = 7;

    if (explicit){
       XHeap_push(&xh, a, xheaplink);
       XHeap_push(&xh, b, xheaplink);
       XHeap_push(&xh, c, xheaplink);
       XHeap_push(&xh, d, xheaplink);
    }else{
        Heap_push(&h, a, heaplink);
        Heap_push(&h, b, heaplink);
        Heap_push(&h, c, heaplink);
        Heap_push(&h, d, heaplink);
    }
    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        node = new_testnode();
        node->prio = inputs[i];
        if (explicit) XHeap_push(&xh, node, xheaplink);
        else Heap_push(&h, node, heaplink);
    }

    // priorities unchanged, expect the following sequence
    unsigned int outputs[] = {0, 3, 4, 5, 7, 8, 9, 12, 13, 35, 35, 37,
        88, 99, 100, 100, 101, 222,
        344, 768, 920, 1000, 1234, 2134, 9900, 9923, 32902};

    for (size_t i = 0, j=(ARRLEN(outputs)-1); i < ARRLEN(outputs); ++i, --j){
        if (explicit) node = XHeap_pop(&xh, struct testnode, xheaplink);
        else node = Heap_pop(&h, struct testnode, heaplink);
        assert(node);
        //debug("got %u", node->prio);
        size_t idx = (type == MIN_HEAP) ? i : j;
        if (node->prio != outputs[idx]){
            debug("expected %u got %u", outputs[idx], node->prio);
            return TEST_FAIL;
        }

        // push onto second heap for an eventual second round
        if (explicit) XHeap_push(&xh2, node, xheaplink);
        else Heap_push(&h2, node, heaplink);
    }

    // change priorities of a,b,c,d,e and check the outputs sequence
    // changes as expected
    unsigned int adjusted_outputs[] = {3, 4, 5, 8, 9, 12, 13, 35, 35, 37,
        88, 99, 100, 101, 222,
        344, 920, 1000, 1234, 2134, 9900, 9923, 32902};

    if (explicit){
        XHeap_remove(&xh2, a, xheaplink);
        XHeap_remove(&xh2, b, xheaplink);
        XHeap_remove(&xh2, c, xheaplink);
        XHeap_remove(&xh2, d, xheaplink);
    }else{
        Heap_remove(&h2, a, heaplink);
        Heap_remove(&h2, b, heaplink);
        Heap_remove(&h2, c, heaplink);
        Heap_remove(&h2, d, heaplink);
    }
    salloc(0, a);
    salloc(0, b);
    salloc(0, c);
    salloc(0, d);

    for (size_t i = 0, j = (ARRLEN(adjusted_outputs)-1); i < ARRLEN(adjusted_outputs); ++i, --j)
    {
        if (explicit) node = XHeap_pop(&xh2, struct testnode, xheaplink);
        else node = Heap_pop(&h2, struct testnode, heaplink);
        assert(node);
        //debug("got %u", node->prio);
        size_t idx = (type == MIN_HEAP) ? i : j;
        if (node->prio != adjusted_outputs[idx]){
            debug("expected %u got %u", adjusted_outputs[idx], node->prio);
            return TEST_FAIL;
        }
        salloc(0, node);
    }

    Heap_clear(&h, false);
    Heap_clear(&h2, false);

    XHeap_clear(&xh, false);
    XHeap_clear(&xh2, false);

    return TEST_PASS;
}


enum testStatus test_implicit_minheap_delete(void){
    return test_heap_delete(MIN_HEAP, false);
}

enum testStatus test_implicit_maxheap_delete(void){
    return test_heap_delete(MAX_HEAP, false);
}

enum testStatus test_explicit_minheap_delete(void){
    return test_heap_delete(MIN_HEAP, true);
}

enum testStatus test_explicit_maxheap_delete(void){
    return test_heap_delete(MAX_HEAP, true);
}


