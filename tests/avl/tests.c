#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_avl_insert(void);
extern enum testStatus test_avl_invariant(void);

int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "=================================\n"
                    "-------- AVL tree tests ---------\n"
                    "=================================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    Cohort_add(tests, test_avl_insert, "Avl tree insertion works");
    Cohort_add(tests, test_avl_invariant, "Avl invariants are maintained through insertions and deletions");

    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
