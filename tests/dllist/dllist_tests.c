#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/container.h>
#include <tarp/log.h>
#include <tarp/dllist.h>

struct testnode {
    size_t num;
    struct dlnode link;
};

void destructor(struct dlnode *node){
    salloc(0, container(node, struct testnode, link));
}

/*
 * By default:
 * - if stackmode=true, push to the back, pop from the back
 * - if stackmode=false, push to the back, pop from the front
 *
 * if reversed_front_back=true, then:
 *  - if stackmode=true, push to the front, pop from the front
 *  - if stackmode=false, push to front, pop from the back
 */
enum testStatus test_enqdq_pushpop(size_t size, bool reversed_front_back, bool stackmode)
{
    struct dllist *list = Dll_new(destructor);

    /* push size elements to the list one by one, then pop them
    * off and see if the count remains consistent throughout */
    for (size_t i = 1; i <= size; ++i){
        //debug("pushing %s %zu", reversed_front_back ? "front" : "back", i);
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        if (reversed_front_back){
            Dll_pushfront(list, node, link);
        } else {
            Dll_pushback(list, node, link);
        }

        if (Dll_count(list) != i){
            debug("expected %zu got %zu", i, Dll_count(list));
            return TEST_FAIL;
        }
    }

#if 0
    struct testnode *tmp;
    Dll_foreach(list, tmp, struct testnode, link){
        debug("tmp->num %zu", tmp->num);
    }
#endif

    for (size_t i = 1; i <= size; ++i){
        struct testnode *node;

        if (stackmode){
            node = reversed_front_back
                ? Dll_popfront(list, struct testnode, link)
                : Dll_popback(list, struct testnode, link);

            assert(node);
            if (node->num != size+1-i){
                debug("expected %zu got %zu", size+1-i, node->num);
                return TEST_FAIL;
            }
        }
        else{
            node = reversed_front_back
                ? Dll_popback(list, struct testnode, link)
                : Dll_popfront(list, struct testnode, link);

            assert(node);
            if (node->num != i){
                debug("expected %zu, got %zu", i, node->num);
                return TEST_FAIL;
            }
        }

        if (Dll_count(list) != size-i){
            debug("expected %zu got %zu", size-i, Dll_count(list));
            return TEST_FAIL;
        }

        free(node);
    }
    Dll_destroy(&list, true);

    return TEST_PASS;
}

/*
 * Test whether the values we put on the queue and get back from it
 * are consistent with FIFO semantics through a number of enqueue-dequeue
 * operations. Also check the count stays correct throughout. */
enum testStatus test_count_enqueue_dequeue(void){
    size_t inputs[] = {1, 2, 3, 10, 100, 1000, 100 * 1000, 1000 * 1000};

    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        size_t size = inputs[i];
        if (test_enqdq_pushpop(size, false, false) != TEST_PASS
                || test_enqdq_pushpop(size, true, false)  != TEST_PASS
            )
        {
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

/*
 * Test whether the values get put on the stack and get back from it
 * are consistent with LIFO semantics through a number of push-pop
 * operations. Also check the count stays correct throughout. */
enum testStatus test_count_push_pop(void){
    size_t inputs[] = {1, 2, 3, 10, 100, 1000, 100 * 1000, 1000 * 1000};

    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        size_t size = inputs[i];

        if (test_enqdq_pushpop(size, false, true) != TEST_PASS
                || test_enqdq_pushpop(size, true, true) != TEST_PASS
           )
        {
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

// test clear and destrog
enum testStatus test_list_destruction(void){
    struct dllist a = DLLIST_INITIALIZER(destructor);
    struct dllist *b = Dll_new(destructor);
    struct testnode *node;

    for (size_t i = 0; i < 500; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushfront(&a, node, link);

        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushfront(b, node, link);
    }

    Dll_clear(&a, true);
    if (Dll_count(&a) != 0 || Dll_empty(&a) == false) return TEST_FAIL;

    Dll_destroy(&b, true);
    return TEST_PASS;
}

// test if the nth node can be found
enum testStatus test_find_nth_node(void){
    struct dllist list = DLLIST_INITIALIZER(destructor);
    struct testnode *node;
    size_t num = 500;
    for (size_t i = 1; i<=num; i++){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Dll_pushback(&list, node, link);
    }

    for (size_t i = 1; i<=num; ++i){
        node = Dll_find_nth(&list, i, struct testnode, link);
        assert(node);
        if(node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }
    }

    node = Dll_find_nth(&list, Dll_count(&list)+1, struct testnode, link);
    if (node != NULL){
        debug("expected NULL, got %zu", node->num);
        return TEST_FAIL;
    }

    Dll_clear(&list, true);
    return TEST_PASS;
}

// test reversing the list nodes
enum testStatus test_list_upend(void){
    struct dllist list = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    //size_t sizes[] = {1, 2, 3, 10, 100, 1000, 10000};
    size_t sizes[] = {14};
    for (size_t i = 0; i < ARRLEN(sizes); ++i){
        size_t size = sizes[i];

        // numbers get pushed so they decrease front-to-back
        for (size_t j = 1; j <= size; ++j){
            node = salloc(sizeof(struct testnode), NULL);
            node->num = j;
            Dll_pushfront(&list, node, link);
        }

        Dll_upend(&list);

        // on upending the whole list, expect value to match j exactly
        for (size_t j = 1; j<=size; ++j){
            node = Dll_popfront(&list, struct testnode, link);
            assert(node);
            if (node->num != j){
                debug("expected %zu got %zu", j, node->num);
                return TEST_FAIL;
            }
            free(node);
        }
    }

    return TEST_PASS;
}

// Note this test is the same as the corresponding staq one.
// NOTE read the comments in the staq test. The test assumes a stack that
// rotates to the top when dir=1, and toward the bottom when dir=-1. The dll
// thinks in terms of front/back not top/bottom. dir=1 means toward the front.
// This means we must have a stack growing out the front so that the semantics
// of dir is as this test expects. In other words, when dir=1, rotate toward
// the front of the list, which we must ensure is also toward the top of the
// stack ==> use pushfront, popfront, NOT pushback, popback.
enum testStatus test_list_rotation__(size_t size, size_t rotations, int dir){
    /* Run 'rotations' number of tests. For each test:
     *  - construct a list of size SIZE with elements that take values
     *    0..size-1. That is, the elements take all the values modulo size.
     *  - rotate the list 'num_rotations' number of times.
     *  - pop the elements from the list and verify the elements in the list
     *    have been rotated by num_rotations positions */
    for (size_t num_rotations = 0; num_rotations <= rotations; ++num_rotations){
        struct dllist list = DLLIST_INITIALIZER(destructor);

        // insert items with values 0...size-1, i.e. all values mod size
        for (size_t i = 0; i < size; ++i){
            struct testnode *node = salloc(sizeof(struct testnode), NULL);
            node->num = i;
            Dll_pushfront(&list, node, link);
        }

        //debug("rotating %zu times to the %s, staq size=%zu", num_rotations, dir == 1 ? "front" : "back", size);
        Dll_rotate(&list, dir, num_rotations);

        // count should remain unchanged
        if (Dll_count(&list) != size){
            debug("expected %zu got %zu", size, Dll_count(&list));
            return TEST_FAIL;
        }

        size_t modulus = size;
        for (size_t i = 0; i < size; ++i){
            struct testnode *node = Dll_popfront(&list, struct testnode, link);

            /* discard congruent multiples (a%n == (a+(k*n))%n) */
            size_t numrot = num_rotations % size;

            // The explanation for this is exactly the same as the one in the
            // in the equivalent staq rotation test. In fact the test is
            // copy-paste (though it may change in the future)
            size_t expected;
            if (dir == 1){ /* rotate to top of stack */
                expected = ((modulus-1-i) + (modulus-numrot))%modulus;
            } else if (dir == -1){ /* rotate to bottom of stack */
                expected = ((modulus-1-i) + numrot)%modulus;
            } else (assert(false));

            /* the position is the same as the actual value that was assigned to
             * node->num */
            assert(node);
            if (expected != node->num){
                debug("expected %zu got %zu", expected, node->num);
                return TEST_FAIL;
            }
            free(node);
        }
    }
    return TEST_PASS;
}

enum testStatus test_list_rotation(void){
    size_t sizes[] = {1, 2, 3, 10, 569};

    for (size_t i = 0; i < ARRLEN(sizes); ++i){
        size_t size = sizes[i];
        if (test_list_rotation__(size, size*2 + 1, 1) != TEST_PASS) return TEST_FAIL;
        if (test_list_rotation__(size, size*2 + 1, -1) != TEST_PASS) return TEST_FAIL;
    }

    return TEST_PASS;
}

// rotate list as many times as neeed to make a specified node the front
enum testStatus test_list_rotation_to_node(void){
    struct dllist list = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    size_t size = 350;
    for (size_t i=1; i<=size; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushback(&list, node, link);
    }

    for (size_t i=1; i<=size; ++i){
        node = Dll_find_nth(&list, i, struct testnode, link);
        assert(node);
        Dll_rotateto(&list, node, link);
        if (Dll_front(&list, struct testnode, link) != node){
            debug("front does not match expected node");
            return TEST_FAIL;
        }
    }

    Dll_clear(&list, true);
    return TEST_PASS;
}

// test that modifying the list while iterating over it is fine
enum testStatus test_foreach_forward(void){
    struct dllist *list = Dll_new(destructor);
    struct testnode *node;

    size_t num = 9;
    for (size_t i = 1; i<=num; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Dll_pushback(list, node, link);
    }

    /* make the following changes:
     * delete all items with values <=2 and >=7
     * replace items with values ==3 and ==4 with nodes with values 0xff
     * insert 0x01 before items with values 5 and 6
     * insert 0x2 before items with values 5 and 6 */
    struct testnode *iter;
    Dll_foreach(list, iter, struct testnode, link){
        if (iter->num <= 2 || iter->num >= 7){
            Dll_delnode(list, iter, link);
        } else if (iter->num ==3 || iter->num == 4){
            node = salloc(sizeof(struct testnode), NULL);
            node->num = 0xff;
            Dll_replace(list, iter, node, link);
            free(iter);
        }
        else if (iter->num == 5 || iter->num==6){
            node = salloc(sizeof(struct testnode), NULL);
            node->num = 0x1;
            Dll_put_before(list, iter, node, link);

            node= salloc(sizeof(struct testnode), NULL);
            node->num = 0x2;
            Dll_put_after(list, iter, node, link);
        }
    }

    // after all this, the sequence shoud be:
    // 0xff 0xff 0x1 5 0x2 0x1 6 0x2
    // and the count should be 8
    if (Dll_count(list) != num-1){
        debug("expected %zu got %zu", num-1, Dll_count(list));
        return TEST_FAIL;
    }

    uint64_t values[] = {0xff, 0xff, 0x1, 0x5, 0x2, 0x1, 0x6, 0x2};
    for (size_t i = 0; i < (num-1); ++i){
        node = Dll_front(list, struct testnode, link);
        assert(node);
        if (values[i] != node->num){
            debug("expected %zu got %zu", values[i], node->num);
            return TEST_FAIL;
        }
        Dll_rotate(list, 1, 1);
    }

    Dll_destroy(&list, true);
    return TEST_PASS;
}

// test that 2 lists can swap heads
enum testStatus test_headswap(void){
    struct dllist a,b;
    a = DLLIST_INITIALIZER(destructor);
    b= DLLIST_INITIALIZER(destructor);

    uint64_t vals[] = {1, 2, 3, 4, 5, 6, 7};
    // create 2 lists, one with all items, one only with the last 3, then
    // swap their heads and verify
    for (size_t i=0; i<ARRLEN(vals);++i){
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = vals[i];
        Dll_pushback(&a, node, link);
    }
    for (size_t i=ARRLEN(vals)-4; i<ARRLEN(vals);++i){
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = vals[i];
        Dll_pushback(&b, node, link);
    }

    Dll_listswap(&a, &b);

    if (Dll_count(&a) != ARRLEN(vals)-3){
        debug("expected %zu got %zu", ARRLEN(vals)-3, Dll_count(&a));
        return TEST_FAIL;
    }
    if (Dll_count(&b) != ARRLEN(vals)){
        debug("expected %zu got %zu", ARRLEN(vals), Dll_count(&b));
        return TEST_FAIL;
    }

    for (size_t i=0; i<ARRLEN(vals);++i){
        struct testnode *node = Dll_popfront(&b, struct testnode, link);
        assert(node);
        if (node->num != vals[i]){
            debug("expected %zu got %zu", vals[i], node->num);
            return TEST_FAIL;
        }
        free(node);
    }
    for (size_t i=ARRLEN(vals)-4; i<ARRLEN(vals);++i){
        struct testnode *node = Dll_popfront(&a, struct testnode, link);
        assert(node);
        if (node->num != vals[i]){
            debug("expected %zu got %zu", vals[i], node->num);
            return TEST_FAIL;
        }
        free(node);
    }

    return TEST_PASS;
}

// can front and back be removed and are they freed correctly
enum testStatus test_remove_front_and_back(void){
    struct dllist a = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    size_t num = 2300;
    // add every time both front and back
    for (size_t i = 0; i< num; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushback(&a, node, link);
        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushfront(&a, node, link);
    }

    // verify after deleting from both sides there are no leaks and such
    // (run with sanitizers) and the count is 0
    for (size_t i = 0; i<num; ++i){
        Dll_delfront(&a);
        Dll_delback(&a);
    }

    if (Dll_count(&a) != 0){
        debug("expected %zu got %zu", 0, Dll_count(&a));
        return TEST_FAIL;
    }

    if (a.front != NULL || a.back != NULL){
        debug("unclean list head after removals");
        return TEST_FAIL;
    }

    return TEST_PASS;
}

enum testStatus test_list_join(void){
    struct dllist a = DLLIST_INITIALIZER(destructor), b = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    size_t len = 7482;
    for (size_t i = 0; i< len; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Dll_pushback(&a, node, link);

        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Dll_pushback(&b, node, link);
    }

    // verify that joining b to a doubles the count and gives a list
    // with the correct concatenation 0...n,0...n
    Dll_join(&a, &b);
#if 0
    Dll_foreach(&a, node, struct testnode, link){
        info("val %zu", node->num);
    }
#endif

    if(Dll_count(&a) != len*2 || Dll_count(&b) !=0){
        debug("expected len(a)=%zu len(b)=%zu ; got len(a)=%zu len(b)=%zu",
                len*2, Dll_count(&a), 0, Dll_count(&b));
        return TEST_FAIL;
    }

    for (size_t i = 0; i<len*2; ++i){
        node = Dll_popfront(&a, struct testnode, link);
        assert(node);
        if (node->num != i%len){
            debug("expected %zu got %zu", i%len, node->num);
            return TEST_FAIL;
        }
        free(node);
    }
    return TEST_PASS;
}

enum testStatus test_list_split(void){
    struct dllist a = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    size_t len = 15;
    for (size_t i = 0; i< len; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Dll_pushback(&a, node, link);
    }

    // split a down the middle; the len/2+1 th node becomes the head of a new
    // list breaking off from a; a should be left with len/2 items; the new
    // list should have len/2 items as well (or (len/2)+1, if the len is odd);
    // e.g. if len=15, len(a) should be 7 and len(b) should be 8
    struct dllist *b = Dll_split_list(&a, Dll_find_nth_node(&a, (len/2)+1));

    // round the length up for b since the lenth was divided in half and it
    // could've been truncated if len is odd;
    size_t b_len = (len % 2 == 0) ? (len/2) : (len/2 + 1);
    if(Dll_count(&a) != len/2 || Dll_count(b) != b_len){
        debug("expected len(a)=%zu len(b)=%zu ; got len(a)=%zu len(b)=%zu",
                len/2, b_len, Dll_count(&a), Dll_count(b));
        return TEST_FAIL;
    }

    for (size_t i = 0; i< (len/2); ++i){
        node = Dll_popfront(&a, struct testnode, link);
        assert(node);
        if (node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }
        free(node);
    }

    for (size_t i = b_len-1; i< len; ++i){
        node = Dll_popfront(b, struct testnode, link);
        assert(node);
        if (node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }
        free(node);
    }

    Dll_destroy(&b, true);
    return TEST_PASS;
}

/*
 * Push massive number of nodes;
 * With each pushed node:
 *  - rotate the list 100 times to the back/bottom
 *  - upend the whole staq
 *  - rotate the staq 100 times to the front/top
 */
extern enum testStatus test_list_performance(void){
    debug("sizeof of dlnode is %zu", sizeof(struct dlnode));
    size_t num = 80 * 1000;
    //size_t num = 80 * 1000 * 1000;
    //size_t num = 87;

    struct dllist q = DLLIST_INITIALIZER(destructor);
    struct testnode *node;

    for (size_t i = 0; i < num; i++){
        node = salloc(sizeof(struct testnode), NULL);
        Dll_pushback(&q, node, link);
        Dll_rotate(&q, 1, 100);
        Dll_upend(&q);
        Dll_rotate(&q, -1, 100);
    }

    for (size_t i = 0; i < num; ++i){
        node = Dll_popfront(&q, struct testnode, link);
        salloc(0, node);
    }

    Dll_clear(&q, true);
    return TEST_PASS;
}
