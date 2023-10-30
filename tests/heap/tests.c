#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <tarp/common.h>
#include <tarp/cohort.h>

extern enum testStatus test_heapsort(void);
extern enum testStatus test_implicit_minheap_basics(void);
extern enum testStatus test_implicit_maxheap_basics(void);
extern enum testStatus test_implicit_minheap_priochange(void);
extern enum testStatus test_implicit_maxheap_priochange(void);
extern enum testStatus test_implicit_minheap_delete(void);
extern enum testStatus test_implicit_maxheap_delete(void);

extern enum testStatus test_explicit_minheap_basics(void);
extern enum testStatus test_explicit_maxheap_basics(void);
extern enum testStatus test_explicit_minheap_priochange(void);
extern enum testStatus test_explicit_maxheap_priochange(void);
extern enum testStatus test_explicit_minheap_delete(void);
extern enum testStatus test_explicit_maxheap_delete(void);


int main(int argc, char **argv){
    UNUSED(argv);
    UNUSED(argc);

    fprintf(stderr, "==============================\n"
                    "-------- heap tests ----------\n"
                    "==============================\n"
                    );

    struct cohort *tests = Cohort_init();
    assert(tests != NULL);

    // TODO: hook explicit maxheap into these same tests
    Cohort_add(tests, test_heapsort, "test generic heapsort");
    Cohort_add(tests, test_implicit_minheap_basics, "test minheap basics");
    Cohort_add(tests, test_implicit_maxheap_basics, "test maxheap basics");
    Cohort_add(tests, test_implicit_minheap_priochange, "test implicit minheap priority change");
    Cohort_add(tests, test_implicit_maxheap_priochange, "test implicit maxheap priority change");
    Cohort_add(tests, test_implicit_minheap_delete, "test implicit minheap deletion");
    Cohort_add(tests, test_implicit_maxheap_delete, "test implicit maxheap deletion");

    Cohort_add(tests, test_explicit_minheap_basics, "test explicit minheap basics");
    Cohort_add(tests, test_explicit_maxheap_basics, "test explicit maxheap basics");
    Cohort_add(tests, test_explicit_minheap_priochange, "test explicit minheap priority change");
    Cohort_add(tests, test_explicit_maxheap_priochange, "test explicit maxheap priority change");
    Cohort_add(tests, test_explicit_minheap_delete, "test explicit minheap deletion");
    Cohort_add(tests, test_explicit_maxheap_delete, "test explicit maxheap deletion");

    enum testStatus res = Cohort_decimate(tests);
    Cohort_destroy(tests);
    return res;
}
