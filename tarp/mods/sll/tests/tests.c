#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/sllist.h>
#include "cohort.h"

enum status keeps_count(void);
enum status can_insert_and_delete_items(void);
enum status can_destroy_list(void);

int main(int argc, char **argv){
    struct cohort *tests = Cohort_init();
    assert(tests != NULL);
   
    Cohort_add(tests, keeps_count, "keeps_count");
    Cohort_add(tests, can_insert_and_delete_items, "can_insert_and_delete_items");
    Cohort_add(tests, can_destroy_list, "can_destroy_list");

    enum status res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res; 
}
