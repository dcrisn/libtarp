#include "cohort.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/staq.h>

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
    
    struct testnode *node, *temp = NULL; 
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
        
    STAQ_FOREACH_SAFE(&q, node, list, temp){
        STAQ_DEQUEUE(&q, node, list);  
        free(node);
    }
    
    if (!STAQ_EMPTY(&q) || STAQ_COUNT(&q) != 0) return FAILURE;

    return SUCCESS;
}


