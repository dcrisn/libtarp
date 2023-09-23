#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#include <tarp/cdll.h>
#include <tarp/timeutils.h>
#include "cohort.h"

struct testnode {
    uint32_t id;
    struct cdll_node list;
};

enum status perf_test(void){
    uint64_t time_before = msts(CLOCK_MONOTONIC);

    int32_t num = 12655555;
    //int32_t num = 12;
    
    CDLL_INIT(mylist);

    for (int32_t i = 0; i < num; ++i){
        struct testnode *node = calloc(1, sizeof(struct testnode));
        assert(node);
        CDLL_NODE_INIT(node, list);
        node->id = i;

        CDLL_pusht(&mylist, &node->list);
    }
    if (CDLL_count(&mylist) != num) return FAILURE;

    struct cdll_node *link = NULL;
    while ( (link = CDLL_popt(&mylist)) ){
        struct testnode *node = CDLL_CONTAINER(link, testnode);
        free(node);
    };

    if (CDLL_count(&mylist) != 0 ) return FAILURE;

    uint64_t time_after = msts(CLOCK_MONOTONIC);
    printf(" ** Time elapsed: %lu ms\n", time_after - time_before);

    return SUCCESS;
}
