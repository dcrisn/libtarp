#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "cohort.h"

extern enum status keeps_correct_count(void);
extern enum status pop_empty_stack(void);
extern enum status can_pop_top(void);
extern enum status can_pop_nth(void);
extern enum status can_peek(void);
extern enum status can_upend(void);
extern enum status can_copy(void);
extern enum status can_append_whole_stack(void);

int main(int argc, char**argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, keeps_correct_count, "is_counter_updated");
    Cohort_add(tests, pop_empty_stack, "pop_empty_stack");
    Cohort_add(tests, can_pop_top, "pop_top");
    Cohort_add(tests, can_pop_nth, "pop_nth_item");
    Cohort_add(tests, can_peek, "can_peek");
    Cohort_add(tests, can_upend, "can_upend_stack");
    Cohort_add(tests, can_copy, "can_copy_stack");
    Cohort_add(tests, can_append_whole_stack, "can_append_whole_stack");
    enum status res = Cohort_decimate(tests);
    
    Cohort_destroy(tests);
    return res;
}
