#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>

#include <tarp/cohort.h>

enum testStatus can_compare_timespecs(void);

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    //Cohort_add(tests, can_compare_timespecs, "can_compare_timespecs");

    //enum testStatus res = Cohort_decimate(tests);
   Cohort_destroy(tests);
   //return res;
}
