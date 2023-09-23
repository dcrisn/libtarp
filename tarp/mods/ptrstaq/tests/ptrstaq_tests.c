#include "cohort.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <tarp/ptrstaq.h>
#include <tarp/timeutils.h>

struct testnode {
    int id;
};


enum status perf_test(void){
    uint64_t time_before = msts(CLOCK_MONOTONIC);

    int32_t num_nodes = 12655555;
    //int32_t num_nodes = 12;

    struct stack s = STAQ_INITIALIZER;

    for (int32_t i = 0; i< num_nodes; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        node->id = i;
        stack_push(&s, node);
    }

    if ((int32_t)stack_count(&s) != num_nodes) return FAILURE;

    struct testnode *node = NULL;
    while ( (node = (struct testnode*)stack_pop(&s)) ){
        free(node);
    }
    
    if (stack_count(&s) != 0) return FAILURE;
    
    uint64_t time_after = msts(CLOCK_MONOTONIC);

    printf(" ** Time elapsed: %lu ms\n", time_after - time_before);
    return SUCCESS;
}


//enum status can_join_stacks(void){

//}
