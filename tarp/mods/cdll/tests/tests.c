#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/cdll.h>
#include <tarp/common.h>
#include "cohort.h"

enum status perf_test(void);

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, perf_test, "perf_test");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
