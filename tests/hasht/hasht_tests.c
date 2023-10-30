#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <tarp/container.h>
#include <tarp/vector.h>
#include <time.h>

#include <tarp/cohort.h>
#include <tarp/common.h>
#include <tarp/log.h>

#include <tarp/hasht.h>

struct testnode {
    struct hashtnode link;
    uint64_t key;
    unsigned data;
};

#define new_testnode() salloc(sizeof(struct testnode), NULL)

void destructor(struct hashtnode *node){
    assert(node);
    salloc(0, container(node, struct testnode, link));
}

void getkey(const struct hashtnode *node, void **start, size_t *len){
    assert(node);
    assert(start);
    assert(len);

    struct testnode *t = container(node, struct testnode, link);
    //debug("called to get key %lu", t->key);
    *start = &t->key;
    *len = sizeof(t->key);
}

enum testStatus test_basic_hasht(void){
    struct hasht *ht = Hasht_new(NULL, getkey, destructor, 0);
    time_t t; srand((unsigned) time(&t));
    struct testnode *node;

    size_t num = 7 * 1000;
    size_t count = num;
    uint32_t *inputs = salloc(sizeof(uint32_t)*num, NULL);

    for (size_t i = 0; i < num; ++i){
        inputs[i] = rand(); // wraps around on overflow, it's fine
    }

    // insert num elements, each with a unique key
    for (size_t i = 0; i < num; ++i){
        node = new_testnode();
        node->key = inputs[i];
        //debug("setting %lu", node->key);

        // if not unique, generate new keys until getting a unique one
        while (Hasht_maybe_set(ht, node, link) == false){
            node->key = inputs[i] = rand();
        }
    }

    if (Hasht_empty(ht) || Hasht_count(ht) != num){
        debug("expected count %zu, got %zu", num, Hasht_count(ht));
        return TEST_FAIL;
    }

    /* for each node added, verify it can be found; do it backwards just for
     * the sake of it*/
    node = new_testnode();
    for (size_t i = 0; i<num; ++i){
        node->key = inputs[i];
        if (!Hasht_has(ht, node, link)){
            debug("index %zi", i);
            debug("node with key %lu unexpectedly not found in table", node->key);
            return TEST_FAIL;
        }
    }

    /* delete each node in the hashtable; for each delete:
     *  - check that the node removed cannot be found any more;
     *  - check the hash table count is decreased by one
     *  - check all the nodes that have not been removed can still be found.
     */
    count = Hasht_count(ht);
    for (ssize_t i = num-1; i >= 0; --i){
        node->key = inputs[i];

        /*
         * This can not fail since we made sure not to insert duplicates,
         * so a matching entry must always exist */
        if (!Hasht_delete(ht, node, link)){
            debug("Failed to delete node with key %lu", node->key);
            return TEST_FAIL;
        }

        // verify the deletion worked
        if (Hasht_has(ht, node, link)){
            debug("node with key %lu unexpectedly still found in table", node->key);
            return TEST_FAIL;
        }

        --count;
        if (Hasht_count(ht) != count){
            debug("hasht count incorrect: expected %zu got %zu", count, Hasht_count(ht));
            return TEST_FAIL;
        }

        for (ssize_t j = 0; j < i; j++){
            node->key = inputs[j];
            if (!Hasht_has(ht, node, link)){
                debug("node with key %lu unexpectedly not found in table", node->key);
                return TEST_FAIL;
            }
        }
    }

    if (Hasht_count(ht) != 0 || !Hasht_empty(ht)){
        debug("hashtable unexpectedly not empty (count=%zu)", Hasht_count(ht));
        return TEST_FAIL;
    }

    salloc(0, node);
    salloc(0, inputs);
    Hasht_destroy(&ht, true);

    return TEST_PASS;
}

enum testStatus test_hasht_duplicates(void){
    struct hasht *ht = Hasht_new(NULL, getkey, destructor, 0);
    struct testnode *node;

    size_t inputs[] = {1,2,3,4,5,6,7,8,9,10};
    size_t key = 88; // arbitrary

    size_t inserted = 0;
    // if duplicates are disallowed, only one insertion should succeed
    for (size_t i = 0; i < ARRLEN(inputs); i++){
        node = new_testnode();
        node->key = key;
        node->data = inputs[i];

        if (Hasht_maybe_set(ht, node, link)){
            inserted++;
        } else {
            salloc(0, node);
        }
    }

    if (Hasht_count(ht) != inserted || inserted != 1){
        debug("Incorrect number of elements: expected %zu got %zu",
                inserted, Hasht_count(ht));
        return TEST_FAIL;
    }

    struct hashtnode *tmp = Hasht_get(ht, node, link);
    assert(tmp);
    node = container(tmp, struct testnode, link);
    if (node->data != inputs[0]){
        debug("count correct but item invalid: expected %u got %u",
                inputs[0], node->data);
        return TEST_FAIL;
    }

    Hasht_clear(ht, true);

    // if duplicates are allowed, all insertion should succed and looking
    // them out will get them back in stack-like fashion (LIFO)
    for (size_t i = 0; i < ARRLEN(inputs); ++i){
        node = new_testnode();
        node->key = key;
        node->data = inputs[i];
        Hasht_set(ht, node, link);
    }

    if (Hasht_count(ht) != ARRLEN(inputs)){
        debug("incorrect count: expected %zu got %zu", ARRLEN(inputs), Hasht_count(ht));
        return TEST_FAIL;
    }

    node = new_testnode();
    node->key = key;
    node->data = 0;
    for (ssize_t i = ARRLEN(inputs)-1; i >= 0; --i){
        tmp = Hasht_get(ht, node, link);
        assert(tmp);
        struct testnode *tmpcont = container(tmp, struct testnode, link);

        unsigned expected_data = inputs[i]; // LIFO

        if (tmpcont->data != expected_data){
            debug("expected %u got %u", expected_data, tmpcont->data);
            return TEST_FAIL;
        }

        Hasht_remove(ht, tmpcont, link);
    }

    salloc(0, node);
    Hasht_destroy(&ht, true);
    return TEST_PASS;
}


