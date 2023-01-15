#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "cohort.h"

enum status can_compare_timespecs(void);

int main(int argc, char **argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, can_compare_timespecs, "can_compare_timespecs");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
