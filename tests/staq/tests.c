#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus perf_test(void);

int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "==============================\n"
                    "-------- staq tests ----------\n"
                    "==============================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    //Cohort_add(tests, keeps_count, "keeps_count");
    //Cohort_add(tests, can_join_stacks, "can_join_stacks");
    //Cohort_add(tests, can_join_queues, "can_join_queues");
    Cohort_add(tests, perf_test, "perf_test");
    
    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
