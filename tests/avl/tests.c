#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_avl_insert(void);
extern enum testStatus test_avl_set(void);
extern enum testStatus test_avl_find_or_insert(void);
extern enum testStatus test_avl_find_min_max(void);
extern enum testStatus test_avl_height_getter(void);
extern enum testStatus test_avl_invariant(void);
extern enum testStatus test_avl_level_order_iterator(void);

int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "=================================\n"
                    "-------- AVL tree tests ---------\n"
                    "=================================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    Cohort_add(tests, test_avl_insert, "Avl tree insertion and lookup work");
    Cohort_add(tests, test_avl_find_or_insert, "Avl tree find_or_insert works");
    Cohort_add(tests, test_avl_invariant, "Avl invariants correct through inserts/deletes");
    Cohort_add(tests, test_avl_find_min_max, "Find min/max");
    Cohort_add(tests, test_avl_height_getter, "Height getter");
    Cohort_add(tests, test_avl_level_order_iterator, "level-order iterator");

    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
