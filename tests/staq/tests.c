#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_count_enqueue_dequeue(void);
extern enum testStatus test_count_push_pop(void);
extern enum testStatus test_staq_upend(void);
extern enum testStatus test_staq_rotate(void);
extern enum testStatus test_peek(void);
extern enum testStatus test_insert_after(void);
extern enum testStatus test_staq_join(void);

int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "==============================\n"
                    "-------- staq tests ----------\n"
                    "==============================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    Cohort_add(tests, test_count_enqueue_dequeue, "Queue ADT enq/dq operations work");
    Cohort_add(tests, test_count_push_pop, "Stack ADT push/pop operations work");
    Cohort_add(tests, test_staq_upend, "queues and stacks can be upended (reversed)");
    Cohort_add(tests, test_staq_rotate, "can staqs be rotated");
    Cohort_add(tests, test_peek, "can peek front and back");
    Cohort_add(tests, test_insert_after, "can insert after node");
    Cohort_add(tests, test_staq_join, "can join staqs");

    //Cohort_add(tests, perf_test, "perf_test");

    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
