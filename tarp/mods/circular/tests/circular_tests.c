#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <tarp/circular.h>
#include "cohort.h"

struct testnode{
    char *name;
    struct circular link;
    uint64_t id;
};

bool printnode(const struct circular *node){
    struct testnode *i = list_entry(node, typeof(*i), link);
    printf("id = %lu\n", i->id);
}

bool findf(const struct circular *node){
    struct testnode *i = list_entry(node, typeof(*i), link);
    return (i->id == 1000);
}


/*
 * Test if the number of items is correctly maintained and 
 * reported through deletions and insertions. 
 */
enum status keeps_count(void){
    CDLL_INIT(mylist);
    if (CDLL_count(&mylist)) return FAILURE;

    struct testnode one = {.name = "one", .id = 111};
    struct testnode two = {.name = "two", .id = 222};
    struct testnode three = {.name = "three", .id = 333};
    struct testnode four = {.name = "four", .id = 444};
    struct testnode five = {.name = "five", .id = 555};
    struct testnode six = {.name = "six", .id = 666};
    struct testnode seven = {.name = "seven", .id = 777};
    
    CDLL_pusht(&mylist, &one.link);
    CDLL_pusht(&mylist, &two.link);
    CDLL_pushh(&mylist, &three.link);
    CDLL_pushh(&mylist, &four.link);
    if (CDLL_empty(&mylist) || CDLL_count(&mylist) != 4) return FAILURE;

    CDLL_pusht(&mylist, &five.link);
    CDLL_pusht(&mylist, &six.link);
    CDLL_pushh(&mylist, &seven.link);
    if (CDLL_empty(&mylist) || CDLL_count(&mylist) != 7) return FAILURE;
    
    CDLL_popt(&mylist);
    CDLL_popt(&mylist);
    if (CDLL_empty(&mylist) || CDLL_count(&mylist) != 5) return FAILURE;

    CDLL_popt(&mylist);
    CDLL_popt(&mylist);
    CDLL_popt(&mylist);
    CDLL_popt(&mylist);
    CDLL_popt(&mylist);
    if (!CDLL_empty(&mylist) || CDLL_count(&mylist) != 0) return FAILURE;
    
    int32_t stress = 351700;
    for (int32_t i = 0; i < stress; ++i){
        //printf("inserting %i\n", stress);
        CDLL_pushh(&mylist, &seven.link);
    }
    if (CDLL_empty(&mylist) || CDLL_count(&mylist) != stress) return FAILURE;
    for (int32_t i = 0; i < stress; ++i) CDLL_poph(&mylist);
    if (!CDLL_empty(&mylist) || CDLL_count(&mylist) != 0) return FAILURE;

    return SUCCESS;
}

enum status can_be_destroyed(void){
    CDLL_INIT(mylist);
    int32_t count = 731000;
    for (int32_t i = 0; i < count; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        CDLL_pusht(&mylist, &node->link);
    }

    if (CDLL_count(&mylist) != count) return FAILURE;
    struct testnode *node = NULL;
    struct circular *temp = NULL;
    CDLL_destroy(node, temp, link, &mylist);
    printf("here :: count = %i\n", CDLL_count(&mylist));
    if (CDLL_count(&mylist) != 0) return FAILURE;

    return SUCCESS;
}

enum status can_insert_after(void){
    CDLL_INIT(mylist);
    int count = 10;
    CDLL_INIT(todestroy);  /* add removed nodes here so we can free them at the end */
    
    for (int i = 0; i < count; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        uint64_t id = i;
        CDLL_pusht(&mylist, &node->link);
    }

    uint64_t flag_val = 13;
    struct circular *current, *next;
    current = CDLL_head(&mylist);
    assert(current);

    for (int i = 0; i < count; ++i){
        next = CDLL_next(&mylist, current, false);

        if (i%2 == 0){
            struct testnode *node = calloc(1, sizeof(struct testnode));
            node->id = flag_val;
            assert(node);
            CDLL_insert_after(&mylist, current, &node->link);
        }
        current = next;
    }
    
    int added = 0; 
    for (int i = 0; i < count; ++i, i%2==0 ? ++added : 0) ;
    CDLL_print(&mylist, printnode);
    if (CDLL_count(&mylist) != count + added) return FAILURE;
    puts("here");

    for (int i = 0; i < count; ++i){
        struct testnode *entry = NULL;
        struct circular *node  = NULL;
        node = CDLL_poph(&mylist);
        CDLL_pusht(&todestroy, node);

        if (i%2) continue;

        node = CDLL_poph(&mylist);
        CDLL_pusht(&todestroy, node);
        entry = list_entry(node, typeof(*entry), link);
        if (entry->id != flag_val) return FAILURE;
    }
    
    struct circular *node = NULL;
    struct testnode *entry = NULL;
    CDLL_destroy(entry, node, link, &todestroy);

    return SUCCESS;
}

enum status can_insert_before(void){
    CDLL_INIT(mylist);
    CDLL_INIT(todestroy);
    int count = 15;
    
    for (int i = 0; i < count; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        uint64_t id = i;
        CDLL_pusht(&mylist, &node->link);
    }

    uint64_t flag_val = 13;
    struct circular *current, *next;
    current = CDLL_head(&mylist);
    assert(current);

    for (int i = 0; i < count; ++i){
        next = CDLL_next(&mylist, current, false);

        if (i%3 == 0){
            struct testnode *node = calloc(1, sizeof(struct testnode));
            node->id = flag_val;
            assert(node);
            CDLL_insert_before(&mylist, current, &node->link);
        }
        current = next;
    }
    
    int added = 0; 
    for (int i = 0; i < count; ++i, i%3==0 ? ++added : 0) ;
    CDLL_print(&mylist, printnode);
    if (CDLL_count(&mylist) != count+added) return FAILURE;
    puts("here");

    for (int i = 0; i < count; ++i){
        struct testnode *entry = NULL;
        struct circular *node  = NULL;
        node = CDLL_poph(&mylist);
        CDLL_pusht(&todestroy, node);

        if (i%3) continue;

        entry = list_entry(node, typeof(*entry), link);
        if (entry->id != flag_val) return FAILURE;
        node = CDLL_poph(&mylist);
        CDLL_pusht(&todestroy, node);
    }

    struct circular *node = NULL;
    struct testnode *entry = NULL;
    CDLL_destroy(entry, node, link, &todestroy);

    return SUCCESS;
}

enum status can_find(void){
    CDLL_INIT(mylist);
    int count = 15;
    
    struct testnode *node = calloc(1, sizeof(struct testnode));
    assert(node);
    node->id = 1000;
    CDLL_pusht(&mylist, &node->link);

    for (int i = 0; i< 137000; i++){
        node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = 55;
        CDLL_pushh(&mylist, &node->link);
    }

    if (!CDLL_find(&mylist, findf)) return FAILURE;
    struct testnode *entry = node;
    struct circular *embedded = NULL;
    CDLL_destroy(entry, embedded, link, &mylist);

    return SUCCESS;
}

enum status can_remove(void){
    CDLL_INIT(mylist);
    CDLL_INIT(todestroy);
    uint64_t flag_val = 1000;
    
    for (int i = 0; i < 10; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        uint64_t id = i;
        CDLL_pusht(&mylist, &node->link);

        if (i%2==0){
            node = calloc(1,sizeof(struct testnode));
            assert(node);
            node->id = flag_val;
            CDLL_pusht(&mylist, &node->link);
        }
    }
    
    CDLL_print(&mylist, printnode);

    int added = 0;
    for (int i = 0; i < 10; ++i, i%2==0 ? ++added : 0) ;
    puts("here2");
    if (CDLL_count(&mylist) != 10+added) return FAILURE;
    puts("here3");

    struct circular *node;
    while ((node = CDLL_find(&mylist, findf))) {
        CDLL_remove(&mylist, node);
        CDLL_pusht(&todestroy, node);
    }
    CDLL_print(&mylist, printnode);
    
    puts("todestroy");
    CDLL_print(&todestroy, printnode);

    puts("here");
    printf("count mylist=%i, todestroy=%i\n", CDLL_count(&mylist), CDLL_count(&todestroy));
    if (CDLL_count(&mylist) != 10) return FAILURE;
    if (CDLL_count(&todestroy) != added) return FAILURE;

    struct testnode *entry;
    CDLL_destroy(entry, node, link, &todestroy);

    return SUCCESS;
}


