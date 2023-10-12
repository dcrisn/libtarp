#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/staq.h>
#include <tarp/staq_impl.h>

struct testnode {
    uint64_t num;
    struct staq_node sqnode;
};

enum testStatus test_enqdq_pushpop(size_t staqsz, bool stackmode){
    struct staq *sq = Staq_new();

    /* push size elements to the stack one by one, then pop them
    * off and see if the count remains consistent throughout */
    for (size_t i = 1; i <= staqsz; ++i){
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        if (stackmode) Staq_push(sq, node, sqnode);
        else{
            Staq_enq(sq, node, sqnode);
        }
        if (Staq_count(sq) != i){
            debug("expected %zu got %zu", i, Staq_count(sq));
            return TEST_FAIL;
        }
    }

    for (size_t i = 1; i <= staqsz; ++i){
        struct testnode *node;
        if (stackmode){
            node = Staq_pop(sq, struct testnode);
            assert(node);
            if (node->num != staqsz+1-i){
                debug("expected %zu got %zu", staqsz+1-i, node->num);
                return TEST_FAIL;
            }
        }
        else{
            node = Staq_dq(sq, struct testnode);
            assert(node);
            if (node->num != i){
                debug("expected %zu, got %zu", i, node->num);
                return TEST_FAIL;
            }
        }

        if (Staq_count(sq) != staqsz -i){
            debug("expected %zu got %zu", staqsz-i, Staq_count(sq));
            return TEST_FAIL;
        }

        free(node);
    }
    Staq_destroy(&sq, true);
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
        if (test_enqdq_pushpop(size, false) != TEST_PASS){
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
        if (test_enqdq_pushpop(size, true) != TEST_PASS){
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

enum testStatus test_stack_upend__(size_t size){
    struct staq sq = STAQ_INITIALIZER;

    // push elements with values 1...size
    for (size_t i = 1; i <= size; ++i){
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Staq_push(&sq, node, sqnode);
    }

    size_t count_before = Staq_count(&sq);
    Staq_upend(&sq);
    if (Staq_count(&sq) != count_before){
        debug("expected %zu got %zu", count_before, Staq_count(&sq));
        return TEST_FAIL;
    }

    // ensure the elements are popped back in the *same* order they were
    // pushed; in effect, upending turns the LIFO into a FIFO
    for (size_t i = 1; i <= size; ++i){
        struct testnode *node = Staq_pop(&sq, struct testnode);
        assert(node);
        if (node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }
        free(node);
    }
    return TEST_PASS;
}

enum testStatus test_queue_upend__(size_t size){
    struct staq sq = STAQ_INITIALIZER;

    // enqueue size elements
    for (size_t i = 1; i <= size; ++i){
        struct testnode *node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Staq_enq(&sq, node, sqnode);
    }

    size_t count_before = Staq_count(&sq);
    Staq_upend(&sq);
    if (Staq_count(&sq) != count_before){
        debug("expected %zu got %zu", count_before, Staq_count(&sq));
        return TEST_FAIL;
    }

    // ensure the elements are dequeued back in the *reverse* order they were
    // enqueued; in effect, upending turns the FIFO into a LIFO
    for (size_t i = 0; i < size; ++i){
        struct testnode *node = Staq_dq(&sq, struct testnode);
        assert(node);
        if (node->num != size-i){
            debug("expected %zu got %zu", size-i, node->num);
            return TEST_FAIL;
        }
        free(node);
    }
    return TEST_PASS;
}

enum testStatus test_staq_upend(void){
    size_t sizes[] = {1, 2, 3, 10, 100, 1000, 10000};
    for (size_t i = 0; i < ARRLEN(sizes); ++i){
        size_t size = sizes[i];

        if (test_stack_upend__(size) != TEST_PASS){
            return TEST_FAIL;
        }

        if (test_queue_upend__(size) != TEST_PASS){
            return TEST_FAIL;
        }
    }

    return TEST_PASS;
}

enum testStatus test_staq_rotate__(size_t size, size_t rotations, int dir){
    /* Run 'rotations' number of tests. For each test:
     *  - construct a staq of size SIZE with elements that take values
     *    0..size-1. That is, the elements take all the values modulo size.
     *  - rotate the staq 'num_rotations' number of times.
     *  - pop the elements off the stack and verify the elements in the staq
     *    have been rotated by num_rotations positions */
    for (size_t num_rotations = 0; num_rotations <= rotations; ++num_rotations){

        struct staq sq = STAQ_INITIALIZER;
        // insert items with values 0...size-1, i.e. all values mod size
        for (size_t i = 0; i < size; ++i){
            struct testnode *node = salloc(sizeof(struct testnode), NULL);
            node->num = i;
            Staq_push(&sq, node, sqnode);
        }

        //debug("rotating %zu times, staq size=%zu", num_rotations, size);
        Staq_rotate(&sq, dir, num_rotations);

        // count should remain unchanged
        if (Staq_count(&sq) != size){
            debug("expected %zu got %zu", size, Staq_count(&sq));
            return TEST_FAIL;
        }

        size_t modulus = size;
        for (size_t i = 0; i < size; ++i){
            struct testnode *node = Staq_pop(&sq, struct testnode);

            /* discard congruent multiples (a%n == (a+(k*n))%n) */
            size_t numrot = num_rotations % size;

            /*
             * The nodes we inserted were given values (assigned to their .num
             * members) 0...modulus-1, where modulus=size (see assignment
             * above).
             * With no rotation, the items should be in the stack, bottom to top
             * 0, 1, 2, ...size-1. So then proceeding from top to bottom
             * they are normally modulus-1, modulus-2, modulus-3, ...,
             * modulus-(modulus-1), modulus-modulus.
             * Our variable i increases from 0 to modulus-1, so the above can
             * be rewritten to use i: modulus-1-(i=0), modulus-1-(i=1), ...,
             * modulus-1-(i=(modulus-1)).
             *
             * Now, when we perform a right-rotate by e.g. 2 positions, the
             * values are rotated to the right such that the value at
             * modulus-1-i is now the one that would normally be at
             * modulus-1-2-i. The problem is if we continue on, i will
             * eventually grow to be =modulus-1, and then we have
             * modulus-1-2-(modulus-1) i.e. -2, which is a negative number and
             * our size_t will underflow and wrap around. So instead we note that
             * a right rotation by 2 positions is the same as a left rotation
             * by (modulus-2) positions.
             * That is say, modulus-1-2-i == modulus-1+(modulus-2)-i (mod
             * MODULUS of course) i.e. we have (modulus-1-i)
             * (which is guaranteed to be >= 0) + (modulus-2), and the sum is
             * mod MODULUS.
             *
             * So for a right rotation by n positions, we want to get the
             * value of the element n positions to the left which is the same as
             * the value of the element (cur_position+(modulus-n)) mod MODULUS
             * positions to the right. Conversely, for a left rotation by n
             * positions, we simply get the value of the element that is
             * (cur_position + n) mod MODULUS positions to the right.
             * Cur_position is of course given by 'modulus-1-i'.
             */
            size_t expected;
            if (dir == 1){ /* right rotate */
                expected = ((modulus-1-i) + (modulus-numrot))%modulus;
            } else if (dir == -1){ /* left rotate */
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

enum testStatus test_staq_rotate(void){
    size_t sizes[] = {1, 2, 3, 10, 569};

    for (size_t i = 0; i < ARRLEN(sizes); ++i){
        size_t size = sizes[i];
        if (test_staq_rotate__(size, size*2 + 1, 1) != TEST_PASS) return TEST_FAIL;
        if (test_staq_rotate__(size, size*2 + 1, -1) != TEST_PASS) return TEST_FAIL;
    }

    return TEST_PASS;
}

enum testStatus test_peek(void){
    struct testnode *node;
    struct staq sq = STAQ_INITIALIZER;
    size_t len = 6;
    for (size_t i = 0; i < len; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Staq_enq(&sq, node, sqnode);
    }

    for (size_t i = 0 ; i<len; ++i){
        node = Staq_front(&sq, struct testnode);
        assert(node);
        //debug("FRONT: %u (i=%zu)", node->num, i);
        if (node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }

        node = Staq_back(&sq, struct testnode);
        assert(node);
        //debug("BACK: %u (i=%zu)", node->num, i);
        size_t rotations=i;
        /* we never pop, so normally the value of at the back would always be
         * = len-1. So we just need to find the value that would end up at the
         * back when rotated to the left (toward the front) by i rotations */
        size_t expected_back = ((len-1)+rotations) % len;
        if (node->num !=  expected_back){
            debug("expected %zu got %zu", expected_back, node->num);
            return TEST_FAIL;
        }

        Staq_rotate(&sq, 1, 1); // rotate by 1 toward the front
    }

    Staq_clear(&sq, true);
    return TEST_PASS;
}


enum testStatus test_insert_after(void){
    struct staq sq = STAQ_INITIALIZER;
    struct testnode *node;
    size_t magic = 0xff;
    // insert 3 nodes with values 1,2,3
    for (size_t i = 1; i <= 3; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = i;
        Staq_enq(&sq, node, sqnode);
    }

    // insert a node with value 0xff after every node
    for (size_t i = 0; i<3; ++i){
        node = salloc(sizeof(struct testnode), NULL);
        node->num = magic;
        Staq_putafter(&sq, Staq_front(&sq, struct testnode), node, sqnode);
        Staq_rotate(&sq, 1, 2);
    }
#if 0
    foreach(&sq, node, struct testnode){
        info("item with val %zu", node->num);
    }
#endif

    // check that the items, read from the front, are in a sequence
    // of 1,magic,2,magic,3,magic
    for (size_t i = 1; i <= 3; ++i){
        node = Staq_dq(&sq, struct testnode);
        assert(node);
        //debug("FRONT: %zu", node->num);
        if (node->num != i){
            debug("expected %zu got %zu", i, node->num);
            return TEST_FAIL;
        }
        free(node);

        node = Staq_dq(&sq, struct testnode);
        assert(node);
        //debug("FRONT2: %zu", node->num);
        if (node->num != magic){
            debug("expected %zu got %zu", magic, node->num);
            return TEST_FAIL;
        }
        free(node);
    }
    return TEST_PASS;
}
