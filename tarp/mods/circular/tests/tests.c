#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/circular.h>
#include "cohort.h"

enum status keeps_count(void);
enum status can_be_destroyed(void);
enum status can_insert_after(void);
enum status can_insert_before(void);
enum status can_find(void);
enum status can_remove(void);

int main(int argc, char **argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, keeps_count, "keeps_count");
    Cohort_add(tests, can_be_destroyed, "can_be_destroyed");
    Cohort_add(tests, can_insert_after, "can_insert_after");
    Cohort_add(tests, can_insert_before,"can_insert_after");
    Cohort_add(tests, can_find, "can_find");
    Cohort_add(tests, can_remove, "can_remove");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
