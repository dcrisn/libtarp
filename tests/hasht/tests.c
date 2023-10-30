#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_basic_hasht(void);
extern enum testStatus test_hasht_duplicates(void);


int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "==============================\n"
                    "-------- hasht tests ----------\n"
                    "==============================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    Cohort_add(tests, test_basic_hasht, "test basic hash table functionality");
    Cohort_add(tests, test_hasht_duplicates, "test duplicate behavior");

    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
