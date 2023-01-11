#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sllist.h>
#include "cohort.h"

enum status keeps_count(void);

int main(int argc, char **argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, keeps_count, "keeps_count");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
