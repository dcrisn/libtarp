#include "cohort.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/staq.h>
#include <tarp/timeutils.h>

STAQ_DEFINE(teststack, testnode);
STAQ_DEFINE(testq, testnode);

struct testnode{
    int id;
    STAQ_ENTRY(testnode) list;
};


bool stack_dump(struct teststack *staq){
    struct testnode *node = NULL;
    STAQ_FOREACH(staq, node, list){
        printf(" => node with id %i\n", node->id);
    }
    return true;
}

bool qdump(struct testq *staq){
    struct testnode *node = NULL;
    STAQ_FOREACH(staq, node, list){
        printf(" => node with id %i\n", node->id);
    }
    return true;
}

enum status keeps_count(void){
    struct testq     q = STAQ_INITIALIZER;
    struct teststack s = STAQ_INITIALIZER; 

    /* ensure empty */
    if (!STAQ_EMPTY(&q) || !STAQ_EMPTY(&s) ||
            STAQ_COUNT(&q) != 0 || STAQ_COUNT(&s) != 0){
        return FAILURE;
    }

    uint32_t stress_value = 530400;
    
    struct testnode *node = NULL; 
    for (uint32_t i = 0; i < stress_value; ++i){
        node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_PUSH(&s, node, list);
    }
    if (STAQ_EMPTY(&s) || STAQ_COUNT(&s) != stress_value){
        return FAILURE;
    }
    
    STAQ_POP(&s, node, list);
    while (node){
        //printf("enqueueing node with id %i, qc = %i, a=%p\n", node->id, STAQ_COUNT(&q), (void *)node);
        //qdump(&q);
        STAQ_ENQUEUE(&q, node, list);
        STAQ_POP(&s, node, list);
        //printf("popped node with id %i, sc = %i, a=%p\n", node ? node->id: -1, STAQ_COUNT(&s), (void *)node);
    }

    if (!STAQ_EMPTY(&s) || STAQ_COUNT(&s) != 0 || STAQ_EMPTY(&q) || STAQ_COUNT(&q) != stress_value){
        return FAILURE;
    }
    
    STAQ_DESTROY(&q, testnode, list);
    
    if (!STAQ_EMPTY(&q) || STAQ_COUNT(&q) != 0) return FAILURE;

    return SUCCESS;
}

enum status can_join_stacks(void){
    struct teststack s1 = STAQ_INITIALIZER;
    struct teststack s2 = STAQ_INITIALIZER;
    
    unsigned int items = 10;
    for (unsigned int i = 0; i < items; i++){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_PUSH(&s1, node, list);
    }
    
    if (STAQ_COUNT(&s1) != items) return FAILURE;

    for (unsigned int i = items; i < items*2; i++){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_PUSH(&s2, node, list);
    }
    STAQ_UPEND(&s2, testnode, list);

    if (STAQ_COUNT(&s2) != items) return FAILURE;
    
    STAQ_JOINS(&s1, &s2, testnode, list);

    //stack_dump(&s1);
    //stack_dump(&s2);
    if (STAQ_COUNT(&s1) != items*2) return FAILURE;
    if (!STAQ_EMPTY(&s2) || STAQ_COUNT(&s2) != 0) return FAILURE;

    /* 
     * We've overlayed a stack with items 10,11,12...19 on top of a stack
     * with items 0,1,2...9. We should now be seeing items 19,18,17, ..., 1, 0,
     * top top bottom.
     */
    struct testnode *node = NULL;
    int item = items*2;
    STAQ_FOREACH(&s1, node, list){
        --item; 
        if (node->id != item){
            return FAILURE;
        }
    }

    STAQ_DESTROY(&s1, testnode, list);
    STAQ_DESTROY(&s2, testnode, list);

    return SUCCESS;
}

enum status can_join_queues(void){
    struct testq q1 = STAQ_INITIALIZER;
    struct testq q2 = STAQ_INITIALIZER;
    
    unsigned int items = 10;
    for (unsigned int i = 0; i < items; i++){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_ENQUEUE(&q1, node, list);
    }
    
    if (STAQ_COUNT(&q1) != items) return FAILURE;

    for (unsigned int i = items; i < items*2; i++){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_ENQUEUE(&q2, node, list);
    }

    if (STAQ_COUNT(&q2) != items) return FAILURE;
    
    STAQ_JOINQ(&q1, &q2, testnode, list);
    
    if (STAQ_COUNT(&q1) != items*2) return FAILURE;
    if (!STAQ_EMPTY(&q2) || STAQ_COUNT(&q2) != 0) return FAILURE;

    /* 
     * We've overlayed a stack with items 10,11,12...19 on top of a stack
     * with items 0,1,2...9. We should now be seeing items 19,18,17, ..., 1, 0,
     * top top bottom.
     */
    struct testnode *node = NULL;
    int item = 0;
    STAQ_FOREACH(&q1, node, list){
        if (node->id != item){
            return FAILURE;
        }
        ++item; 
    }

    STAQ_DESTROY(&q1, testnode, list);
    STAQ_DESTROY(&q2, testnode, list);

    return SUCCESS;
}


enum status perf_test(void){
    uint64_t time_before = msts(CLOCK_MONOTONIC);

    int32_t num_nodes = 12655555;
    struct teststack s = STAQ_INITIALIZER;

    for (int32_t i = 0; i< num_nodes; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        STAQ_PUSH(&s, node, list);
    }

    if ((int32_t)STAQ_COUNT(&s) != num_nodes) return FAILURE;

    struct testnode *node = NULL;
    while ( (node = STAQ_TOP(&s)) ){
        STAQ_POP(&s, node, list);
        free(node);
    }
    
    if (STAQ_COUNT(&s) != 0) return FAILURE;
    
    uint64_t time_after = msts(CLOCK_MONOTONIC);

    printf(" ** Time elapsed: %lu ms\n", time_after - time_before);
    return SUCCESS;
}


//enum status can_join_stacks(void){

//}
