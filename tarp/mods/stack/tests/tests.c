#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "cohort.h"

extern enum status can_push(void);

int main(int argc, char**argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, can_push, "can_push");
    enum status res = Cohort_decimate(tests);
    
    Cohort_destroy(tests);
    return res;
}
