#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <tarp/common.h>

#include "cohort.h"

extern enum status keeps_count(void);

int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, keeps_count, "keeps_count");
    enum status res = Cohort_decimate(tests);
    
    Cohort_destroy(tests);
    return res;
}
