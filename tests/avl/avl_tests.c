#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include <tarp/container.h>
#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/avl.h>
#include <tarp/staq.h>
#include <tarp/error.h>


struct testnode {
    size_t key;
    size_t data;
    struct avlnode link;
    struct staqnode list;
};

#define new_testnode() salloc(sizeof(struct testnode), 0)

enum comparatorResult compf(const struct avlnode *a_, const struct avlnode *b_)
{
    const struct testnode *a = get_container(a_, struct testnode, link);
    const struct testnode *b = get_container(b_, struct testnode, link);
    assert(a);
    assert(b);

    //debug("comparing a=%zu b=%zu", a->key, b->key);
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return 0;
}

const char *testnode2string(const struct avlnode *data){
    assert(data);

    struct testnode *node = get_container(data, struct testnode, link);
    size_t len = 1024;
    char *buff = salloc(len, NULL);
    memset(buff, 0, len);
    snprintf(buff, len, "%zu", node->key);
    return buff; // caller must free it
}

void destructor(struct avlnode *node){
    assert(node);
    salloc(0, get_container(node, struct testnode, link));
}

void staq_dtor(struct staqnode *node){
    assert(node);
    salloc(0, get_container(node, struct testnode, list));
}




// test insertion and lookup
enum testStatus test_avl_insert(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node, *k;

    size_t num = 5 * 1000;
    //debug("===== insertion =====");
    for (size_t i = 0; i < num; ++i){
        //debug("inserting node %zu", i);
        node = new_testnode();
        node->key = i;
        Avl_insert(&tree, node, link);
    }

    if (Avl_count(&tree) != num){
        debug("tree count wrong: expected %zu got %zu", num, Avl_count(&tree));
        return TEST_FAIL;
    }

    //debug("print it out, count=%zu ", Avl_count(&tree));
    //Avl_print(&tree, true, testnode2string);

    //debug("====lookup=====");
    // verify each inserted node is found
    for (size_t i = 0; i < num; ++i){
        k = new_testnode();
        k->key = i;
        node = Avl_find(&tree, k, link);
        if (!node || node->key != k->key){
            debug("expected %zu got %zu %p", k->key, node?node->key:0, node ? (void*)node : NULL);
            return TEST_FAIL;
        }
        salloc(0, k);
    }

    Avl_clear(&tree, true);
    return TEST_PASS;
}

/*
 * Insert:
 * - a certain number of sequential values, then remove them;
 * - a certain number of random values, then remove them
 * - a preselected sequence of values known to include double rotations
 * After each insert and after each delete, verify that the balance
 * factors at each node are correct and the heights of the trees match
 * the balance factors
 *
 * NOTE
 * The inputs are read from the inputs staq. Since the testnode contains both
 * a struct staqnode and a struct avlnode and since it can contain duplicates,
 * make sure you ONLY free them from the staq, and only clear without freeing
 * from inputs.
 */
enum testStatus test_avl_invariant_through_inserts_and_deletes(struct staq *inputs)
{
    assert(inputs);
    size_t size = Staq_count(inputs);
    struct testnode *node;
    struct avltree tree = AVL_INITIALIZER(compf, destructor);

    for (size_t i = 0; i < size; ++i){
        Staq_rotate(inputs, 1, 1);
        node = Staq_front(inputs, struct testnode, list);
        assert(node);
        if (!Avl_insert(&tree, node, link)){
            //debug("duplicate value %zu", node->key);
            /* ensure it is actually a duplicate and not some weird insert fail*/
            assert(Avl_has(&tree, node, link));
            continue;
        }

        if (!isavl(&tree)){
            debug("tree in invalid AVL state after insertion of %zu (count=%zu)",
                    i, Avl_count(&tree));
            return TEST_FAIL;
        }
    }

#if 0
    //info("============== tree before deletions ================ ");
    //Avl_print(&tree, true, testnode2string);
    //exit(0);

    size_t height = Avl_height(&tree);
    struct staq q;
    struct testnode *cursor;
    info("\n============ BEFORE DELETE =====================");
    for (size_t j = 1; j <= height; ++j){
        info("===LEVEL %zu (tree count %zu)===", j, tree.count);
        Avl_foreach_node_at_level(&tree, j, &q, cursor, struct testnode, link){
            info("value=%zu bf=%i", cursor->key, cursor->link.bf);
        }
    }
    info("==================================\n");
#endif

    // try do delete all the nodes
    for (size_t i = 0; i < size; ++i){
        Staq_rotate(inputs, 1, 1);
        node = Staq_front(inputs, struct testnode, list);
        assert(node);

        bool deleted = Avl_unlink(&tree, node, link);

        if (!deleted){
            if (Avl_has(&tree, node, link)){
                debug("failed to delete node with value %zu from tree", node->key);
                return TEST_FAIL;
            }
        }

        if (!isavl(&tree)){
            debug("tree in invalid AVL state after deletion of %zu (count=%zu)",
                    node->key, Avl_count(&tree));
            info("returning fail here");
            return TEST_FAIL;
        }

    }

    Avl_clear(&tree, false);
    if (Avl_count(&tree) != 0){
        return TEST_FAIL;
    }

    return TEST_PASS;
}

enum testStatus test_avl_invariant(void){
    struct testnode *node;

    // =========== random inputs =====================
    time_t t; srand((unsigned) time(&t));
    struct staq list = STAQ_INITIALIZER(staq_dtor);

    /* populate a queue with inputs; then insert all of them into a tree;
     * then delete all of them from a tree */
    size_t size = 5 * 1000;

    for (size_t i = 0; i < size; ++i){
        node = new_testnode();
        node->key = rand() % size;
        //info("node with value %zu", node->key);
        Staq_enq(&list, node, list);
    }

    if (test_avl_invariant_through_inserts_and_deletes(&list) == TEST_FAIL){
        return TEST_FAIL;
    }

    Staq_clear(&list, true);
    list = STAQ_INITIALIZER(staq_dtor);

    // sequential values
    for (size_t i = 0; i <size; ++i){
        node = new_testnode();
        node->key = i;
        Staq_enq(&list, node, list);
    }
    if (test_avl_invariant_through_inserts_and_deletes(&list) == TEST_FAIL){
        return TEST_FAIL;
    }
    Staq_clear(&list, true);
    list = STAQ_INITIALIZER(staq_dtor);

    // preselected values
    size_t inputs[] = {1,41,22,21,37,33,46,39,4,6,15,18,28,17,20,38,0,2,5,48,26,30,9,8,16,24,23,14,29,27,45,49};
    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        //info("\ntrying to insert node %zu", inputs[i]);
        node = new_testnode();
        node->key = inputs[i];
        Staq_enq(&list, node, list);
    }
    if (test_avl_invariant_through_inserts_and_deletes(&list) == TEST_FAIL){
        return TEST_FAIL;
    }

    Staq_clear(&list, true);

    return TEST_PASS;
}

enum testStatus test_avl_find_or_insert(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node;

    size_t num = 5 * 1000;
    for (size_t i = 0; i < num; ++i){
        node = new_testnode();
        node->key = i;

        // all insertions should succeed on the first round
        node = Avl_find_or_insert(&tree, node, link);
        if (node){
            debug("insertion failed; mistaken report of a duplicate (key %zu)",
                    node->key);
            return TEST_FAIL;
        }
    }

    if (Avl_count(&tree) != num){
        debug("tree count wrong: expected %zu got %zu", num, Avl_count(&tree));
        return TEST_FAIL;
    }

    node = new_testnode();
    // make sure all lookups succeed
    for (size_t i = 0 ; i <num; ++i){
        node->key = i;
        if (!Avl_has(&tree, node, link)){
            debug("entry with key %zu incorrectly not found in tree", i);
            return TEST_FAIL;
        }
    }

    for (size_t i = 0; i < num; ++i){
        node->key = i;

        // all insertions should FAIL on the second round as every node
        // has already been inserted and they should be found
        struct testnode *found = Avl_find_or_insert(&tree, node, link);

        if (!found){
            debug("insertion succeeded when it shouldn't have.");
            return TEST_FAIL;
        }
    }

    if (Avl_count(&tree) != num){
        debug("tree count wrong: expected %zu got %zu", num, Avl_count(&tree));
        return TEST_FAIL;
    }

    salloc(0, node);
    Avl_clear(&tree, true);

    return TEST_PASS;
}

enum testStatus test_avl_find_min_max(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node;

    size_t num = 8 * 1000;
    for (size_t i = 1; i <= num; ++i){
        node = new_testnode();
        node->key = i;
        Avl_insert(&tree, node, link);

        // each new element is the new max
        node = Avl_max(&tree, struct testnode, link);
        if (!node || node->key != i){
            debug("Failed to find max");
            return TEST_FAIL;
        }

        // min never changes
        node = Avl_min(&tree, struct testnode, link);
        if (!node || node->key != 1){
            debug("Failed to find min");
            return TEST_FAIL;
        }
    }

    if (Avl_count(&tree) != num){
        debug("tree count wrong: expected %zu got %zu", num, Avl_count(&tree));
        return TEST_FAIL;
    }


    //Avl_print(&tree, true, testnode2string);

    // find the kth min for k=0...num and max for k=num...0
    for (size_t i = 1; i<=num; ++i){
        node = Avl_kmin(&tree, i, struct testnode, link);
        assert_not_null(node);
        if (node->key != i){
            debug("failed to find kmin for k=%zu (expected %zu got %zu)",
                    i, i, node->key);
            return TEST_FAIL;
        }

        node = Avl_kmax(&tree, i, struct testnode, link);
        assert_not_null(node);
        if (node->key != num+1 - i){
            debug("failed to find kmax for k=%zu (expected %zu got %zu)",
                    i, num+1-i, node->key);
            return TEST_FAIL;
        }
    }

    Avl_clear(&tree, true);
    return TEST_PASS;
}

enum testStatus test_avl_height_getter(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node;

    if (Avl_height(&tree) != -1){
        debug("expected -1, got %i", Avl_height(&tree));
        return TEST_FAIL;
    }

    size_t num = 800;

    // each insertion increases the height by 0 or 1
    size_t height = -1;
    for (size_t i = 1; i <= num; ++i){
        node = new_testnode();
        node->key = i;
        Avl_insert(&tree, node, link);

        size_t got = Avl_height(&tree);
        if (got != height && got != height+1){
            debug("Erroneous height: expected %i or %i got %i", height, height+1, got);
            return TEST_FAIL;
        }
        height = got;
    }

    if (Avl_count(&tree) != num){
        debug("tree count wrong: expected %zu got %zu", num, Avl_count(&tree));
        return TEST_FAIL;
    }

    Avl_clear(&tree, true);

    if (Avl_height(&tree) != -1){
        debug("expected -1, got %i", Avl_height(&tree));
        return TEST_FAIL;
    }

    return TEST_PASS;
}

enum testStatus test_avl_level_order_iterator(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node;

    size_t num = 8;
    for (size_t i = 1; i <= num; ++i){
        node = new_testnode();
        node->key = i;
        Avl_insert(&tree, node, link);
    }

    size_t num_levels = 4;
    if (Avl_num_levels(&tree) != num_levels){
        debug("expected %zu levels, got %zu", 4, Avl_num_levels(&tree));
        return TEST_FAIL;
    }

    size_t level_nums[] = {1, 2, 4, 1};
    for (size_t i = 0; i < ARRLEN(level_nums); ++i){
        if (AVL_count_nodes_at_level(&tree, i+1) != level_nums[i]){
            debug("Incorrect number of nodes at level %zu: expected %zu got %zu",
                    i+1, level_nums[i], AVL_count_nodes_at_level(&tree, i+1));
            return TEST_FAIL;
        }
    }

    struct staq q;
    size_t expected_order[] = {4, 2, 6, 1,3, 5, 7, 8}; /* in level order */
    size_t actual_order[num];

    size_t n = 0;
    for (size_t i = 1; i <= Avl_num_levels(&tree); ++i){
        Avl_foreach_node_at_level(&tree, i, &q, node, struct testnode, link){
            //debug("got node with key %zu", node->key);
           actual_order[n++] = node->key;
        }
    }

    //Avl_print(&tree, testnode2string);

    for (size_t i = 0; i < ARRLEN(expected_order); ++i){
        if (expected_order[i] != actual_order[i]){
            debug("Incorrect level order traversal: expected %zu got %zu",
                    expected_order[i], actual_order[i]);
            //return TEST_FAIL;
        }
    }

    Avl_clear(&tree, true);
    return TEST_PASS;
}
