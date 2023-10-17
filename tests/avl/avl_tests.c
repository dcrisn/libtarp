#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <tarp/container.h>
#include <time.h>

#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>
#include <tarp/avl.h>
#include <tarp/staq.h>

struct testnode {
    size_t num;
    struct avlnode link;
    struct staqnode list;
};


#define new_testnode() salloc(sizeof(struct testnode), 0)

enum comparatorResult compf(const struct avlnode *a_, const struct avlnode *b_)
{
    const struct testnode *a = container(a_, struct testnode, link);
    const struct testnode *b = container(b_, struct testnode, link);
    assert(a);
    assert(b);

    //debug("comparing a=%zu b=%zu", a->num, b->num);
    if (a->num < b->num) return -1;
    if (a->num > b->num) return 1;
    return 0;
}

const char *testnode2string(const struct avlnode *data){
    assert(data);

    struct testnode *node = container(data, struct testnode, link);
    size_t len = 1024;
    char *buff = salloc(len, NULL);
    memset(buff, 0, len);
    snprintf(buff, len, "%zu", node->num);
    return buff; // caller must free it
}

void destructor(struct avlnode *node){
    assert(node);
    salloc(0, container(node, struct testnode, link));
}

void staq_dtor(struct staqnode *node){
    assert(node);
    salloc(0, container(node, struct testnode, list));
}

enum testStatus test_avl_insert(void){
    struct avltree tree = AVL_INITIALIZER(compf, destructor);
    struct testnode *node, *k;

    size_t num = 5 * 1000;
    debug("===== insertion =====");
    for (size_t i = 0; i < num; ++i){
        //debug("inserting node %zu", i);
        node = new_testnode();
        node->num = i;
        Avl_insert(&tree, node, link);
    }

    //debug("print it out, count=%zu ", Avl_count(&tree));
    //Avl_print(&tree, true, testnode2string);

    debug("====lookup=====");
    // verify each inserted node is found
    for (size_t i = 0; i < num; ++i){
        k = new_testnode();
        k->num = i;
        struct avlnode *n = Avl_find_node(&tree, &k->link);
        node = n ? container(n, struct testnode, link) : NULL;
        if (!node || node->num != k->num){
            debug("expected %zu got %zu %p", k->num, node?node->num:0, node ? (void*)node : NULL);
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
 * - a cerrtain number of random values, then remove them
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
            //debug("duplicate value %zu", node->num);
            /* ensure it is actually a duplicate and not some weird insert fail*/
            assert(Avl_has(&tree, node, struct testnode, link));
            continue;
        }

        if (!isavl(&tree)){
            debug("tree in invalid AVL state after insertion of %zu (count=%zu)",
                    i, Avl_count(&tree));
            return TEST_FAIL;
        }
    }

    //info("============== tree before deletions ================ ");
    //Avl_print(&tree, true, testnode2string);
    //exit(0);

#if 0
    size_t height = Avl_height(&tree);
    struct staq q;
    struct testnode *cursor;
    info("\n============ BEFORE DELETE =====================");
    for (size_t j = 1; j <= height; ++j){
        info("===LEVEL %zu (tree count %zu)===", j, tree.count);
        Avl_foreach_node_at_level(&tree, j, &q, cursor, struct testnode){
            info("value=%zu bf=%i", cursor->num, cursor->link.bf);
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
            if (Avl_has(&tree, node, testnode, link)){
                debug("failed to delete node with value %zu from tree", node->num);
                return TEST_FAIL;
            }
        }

    #if 0
        // dump each tree level between removals
        size_t height = Avl_height(&tree);
        struct testnode *cursor;
        struct staq q = STAQ_INITIALIZER;
        for (size_t j = 1; j <= height; ++j){
            info("===LEVEL %zu (tree count %zu)===", j, tree.count);
            Avl_foreach_node_at_level(&tree, j, &q, cursor, struct testnode){
                info("value=%zu bf=%i", cursor->num, cursor->link.bf);
            }
        }
    #endif
        if (!isavl(&tree)){
            debug("tree in invalid AVL state after deletion of %zu (count=%zu)",
                    node->num, Avl_count(&tree));
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
        node->num = rand() % size;
        //info("node with value %zu", node->num);
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
        node->num = i;
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
        node->num = inputs[i];
        Staq_enq(&list, node, list);
    }
    if (test_avl_invariant_through_inserts_and_deletes(&list) == TEST_FAIL){
        return TEST_FAIL;
    }

    Staq_clear(&list, true);

    return TEST_PASS;
}

