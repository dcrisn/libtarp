#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_count_enqueue_dequeue(void);
extern enum testStatus test_count_push_pop(void);
extern enum testStatus test_list_destruction(void);
extern enum testStatus test_find_nth_node(void);
extern enum testStatus test_list_upend(void);
extern enum testStatus test_list_rotation(void);
extern enum testStatus test_list_rotation_to_node(void);
extern enum testStatus test_foreach_forward(void);
extern enum testStatus test_headswap(void);
extern enum testStatus test_remove_front_and_back(void);
extern enum testStatus test_list_join(void);
extern enum testStatus test_list_split(void);


int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "==============================\n"
                    "-------- dllist tests --------\n"
                    "==============================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    Cohort_add(tests, test_count_enqueue_dequeue, "Queue ADT enq/dq operations work");
    Cohort_add(tests, test_count_push_pop, "Stack ADT push/pop operations work");
    Cohort_add(tests, test_list_destruction, "List can be cleared and destroyed");
    Cohort_add(tests, test_find_nth_node, "Can get nth node in the list");
    Cohort_add(tests, test_list_upend, "Can list be upended");
    Cohort_add(tests, test_list_rotation, "Does list rotation work");
    Cohort_add(tests, test_list_rotation_to_node, "Can rotate list to specified node");
    Cohort_add(tests, test_foreach_forward, "can alter list while in foreach loop");
    Cohort_add(tests, test_headswap, "test that list heads can be swapped");
    Cohort_add(tests, test_remove_front_and_back, "Can remove front and back");
    Cohort_add(tests, test_list_join, "test list concatenation");
    Cohort_add(tests, test_list_split, "test list concatenation");


    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
