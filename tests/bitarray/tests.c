#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <tarp/common.h>
#include <tarp/bitarray.h>

int num_run  = 0;
int num_passed = 0;
bool passed = 0;

/* test runner */
#define run(f, ...) \
    ++num_run;  \
    passed = f(__VA_ARGS__); \
    if (passed) ++num_passed; \
    printf("[ ] test %i %s\n", num_run, (passed) ? "Passed" : "FAILED !!");

enum{FAILED, PASSED};


static bool test_test_bit(uint32_t n, int state){
    assert(state == 1 || state == 0);

    struct bitarray *new = Bitr_new(n, false);
    assert(new);

    bool res = PASSED;
    
    /* set bit manually */
    int idx = n-1;

    // set bit n-1
    if (state == 1){
        new->bits[idx/8] |= (0x80u >> (idx % 8) );
    }

    if (Bitr_get(new, n) != state) res = FAILED;

    free(new);
    return res;
}

/* test a bit array initialized to all 1s indeed
 * has all bits set */
static bool test_all_ones(uint32_t sz){
    struct bitarray *new = Bitr_new(sz, true);

    bool res = PASSED;
    for (uint32_t i = 0; i < Bitr_size(new); ++i){
        if (Bitr_get(new, i+1) != 1){
            res = FAILED;
            break;
        }
    }

    Bitr_destroy(&new);

    return res;
}

/* test a bit array initialized to all 1s indeed
 * has no bits set */
static bool test_all_zeroes(uint32_t sz){
    struct bitarray *new = Bitr_new(sz, false);

    bool res = PASSED;
    for (uint32_t i = 0; i < new->size; ++i){
        if (Bitr_get(new, i+1) != 0){
            res = FAILED;
            break;
        }
    }

    Bitr_destroy(&new);
    return res;
}

static bool test_set_bit(uint32_t n){
    struct bitarray *new = Bitr_new(n, false);
    assert(new);

    bool res = PASSED;

    if (Bitr_set(new, n) == -1) res = FAILED;
    if (Bitr_get(new, n) != 1) res = FAILED;

    free(new);
    return res;
}

static bool test_clear_bit(uint32_t n){
    struct bitarray *new = Bitr_new(n, false);
    assert(new);

    bool res = PASSED;

    if (Bitr_clear(new, n) == -1) res = FAILED;
    if (Bitr_get(new, n) != 0) res = FAILED;

    free(new);
    return res;
}

static bool test_toggle_bit(uint32_t n, int state){
    struct bitarray *new = Bitr_new(n, false);
    assert(new);

    assert(state == 1 || state == 0);

    bool res = PASSED;
    
    if (state == 1){
        if (Bitr_set(new, n) == -1) res = FAILED;
    }
    
    if (Bitr_toggle(new, n) == -1) res = FAILED;

    if (Bitr_get(new, n) != !state) res = FAILED;

    free(new);
    return res;
}

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    puts("");
    puts("#####################################################");
    puts("################ Running C tests #################### ");
    puts("#####################################################");

    printf("\n%s\n", "===== Validating initialization to all 1s ========= ");
    run(test_all_ones, 1111);
    run(test_all_ones, 1);
    run(test_all_ones, 3);
    run(test_all_ones, 33);

    printf("\n%s\n", "===== Validating initialization to all 0s ========= ");
    run(test_all_zeroes, 2112);
    run(test_all_zeroes, 1);
    run(test_all_zeroes, 31);
    run(test_all_zeroes, 133);

    printf("\n%s\n", "===== Validating bit testing ========= ");
    run(test_test_bit, 7, 1);
    run(test_test_bit, 7, 0);
    run(test_test_bit, 1, 0);
    run(test_test_bit, 111, 1);
    run(test_test_bit, 2000, 1);
    run(test_test_bit, 13, 0);
    run(test_test_bit, 77, 1);

    printf("\n%s\n", "===== Testing bit setting ========= ");
    run(test_set_bit, 1);
    run(test_set_bit, 1);
    run(test_set_bit, 7314);
    run(test_set_bit, 7315);
    run(test_set_bit, 313);
    run(test_set_bit, 71);
    run(test_set_bit, 8);

    printf("\n%s\n", "===== Testing bit clearing ========= ");
    run(test_clear_bit, 1);
    run(test_clear_bit, 1);
    run(test_clear_bit, 7314);
    run(test_clear_bit, 7315);
    run(test_clear_bit, 313);
    run(test_clear_bit, 71);
    run(test_clear_bit, 8);

    printf("\n%s\n", "===== Testing bit toggling ========= ");
    run(test_toggle_bit, 1, 0);
    run(test_toggle_bit, 1, 1);
    run(test_toggle_bit, 7314, 1);
    run(test_toggle_bit, 7315, 0);
    run(test_toggle_bit, 313, 1);
    run(test_toggle_bit, 71, 1);
    run(test_toggle_bit, 8, 0);

printf("\n Passed: %i / %i\n", num_passed, num_run);
if (num_passed != num_run) exit(1);

}
