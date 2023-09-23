#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/sllist.h>
#include <tarp/common.h>
#include "cohort.h"

enum status keeps_count(void);
enum status can_insert_and_delete_items(void);
enum status can_destroy_list(void);
enum status perf_test(void);

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
   // Cohort_add(tests, keeps_count, "keeps_count");
   // Cohort_add(tests, can_insert_and_delete_items, "can_insert_and_delete_items");
   // Cohort_add(tests, can_destroy_list, "can_destroy_list");
   Cohort_add(tests, perf_test, "perf_test");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
