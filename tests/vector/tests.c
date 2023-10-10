#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <tarp/common.h>
#include <tarp/error.h>
#include <tarp/log.h>
#include <tarp/vector.h>
#include <tarp/cohort.h>

struct stuff {
    uint64_t a;
    uint8_t b;
    const char *c;
};

define_vector(u8, uint8_t)
define_vector(u16, uint16_t)
define_vector(u32, uint32_t)
define_vector(u64, uint64_t)
define_vector(stuff, struct stuff)
define_vector(voidptr, void *)
define_vector(float, float)
define_vector(double, double)
define_vector(int, int)
define_vector(string, const char *)

enum testStatus test_vect_count(void){
    vector *vstuff = Vect_new_stuff(10);
    size_t reported = 0;

    const size_t num = 100000;
    for (size_t i = 0; i < num; ++i){
        struct stuff stuff;
        stuff.a = i;
        Vect_pushb_stuff(vstuff, stuff);

        reported = Vect_count(vstuff);
        if (reported != i+1){
            debug("expected %zu, got %zu", i+1, reported);
            return TEST_FAIL;
        }
    }

    struct stuff stuff = {.a=1, .b=2, .c=NULL};
    Vect_insert_stuff(vstuff, num, stuff);
    Vect_insert_stuff(vstuff, num, stuff);
    Vect_insert_stuff(vstuff, num, stuff);
    Vect_insert_stuff(vstuff, 500, stuff);
    Vect_insert_stuff(vstuff, 1, stuff);
    Vect_insert_stuff(vstuff, 2, stuff);
    reported= Vect_count(vstuff);
    if (reported != num+6){
        debug("expected %zu, got %zu", num+6, reported);
        return TEST_FAIL;
    }

    Vect_remove_stuff(vstuff, 300);
    Vect_remove_stuff(vstuff, 100);
    reported = Vect_count(vstuff);
    if (reported != num+4){
        debug("expected %zu, got %zu", num+4, reported);
        return TEST_FAIL;
    }

    size_t actual = num+4;
    for (size_t i = 1; i <= actual; ++i){
        Vect_popb_stuff(vstuff);

        reported = Vect_count(vstuff);
        if (reported != actual - i){
            debug("expected %zu, got %zu", actual-i, reported);
            return TEST_FAIL;
        }
    }

    Vect_destroy(&vstuff);

    vector *v = Vect_new_u8(300);
    Vect_pushb_u8(v, 1);
    Vect_clear(v);
    if (Vect_count(v) != 0) return TEST_FAIL;
    Vect_destroy(&v);

    return TEST_PASS;
}

enum testStatus test_vect_pushb_and_popb(void){
    vector *v = Vect_new_u32(0);
    uint32_t reported = 0;

    const size_t width = 500 * 1000;
    for (size_t i = 0; i < width; ++i){
        Vect_pushb_u32(v, i);
    }

    for (size_t i = 0; i < width; ++i){
        reported = Vect_get_u32(v, i);
        if (reported != i){
            debug("expected=%zu got=%zu", i, reported);
            return TEST_FAIL;
        }
    }

    Vect_resize_u32(v, 0, 0);
    for (size_t i = 10; i <= 17; ++i){
        Vect_pushb_u32(v, i);
    }
    for (size_t i=17; i>=10; --i){
        uint32_t val = Vect_popb_u32(v);
        if (val != i){
            debug("expected %u got %u", i, val);
            return TEST_FAIL;
        }
    }

    Vect_destroy(&v);
    return TEST_PASS;
}

enum testStatus test_vect_front_and_back(void){
    vector *v = Vect_new_u64(100);

    size_t num = 8;

    for (size_t i = 1; i <= num; ++i){
        Vect_pushb_u64(v, i);
    }

    uint64_t front = 1;
    uint64_t back = num;

    while(!Vect_isempty(v)){
        if (Vect_front_u64(v) != (front) || Vect_back_u64(v) != (back)){
            debug("expected (front=%llu back=%llu) got (front=%llu back=%llu)",
                    front, back, Vect_front_u64(v), Vect_back_u64(v));
            return TEST_FAIL;
        }

        Vect_remove_u64(v, 0);
        Vect_remove_u64(v, Vect_count(v)-1);
        front++;
        back--;
    }

    Vect_destroy(&v);
    return TEST_PASS;
}

enum testStatus test_vect_begin_and_end_pointers(void){
    vector *doubles = Vect_new_double(0);
    vector *pointers = Vect_new_voidptr(0);

    size_t num = 800 * 1000;
    for (size_t i = 0; i <= num; ++i){
        Vect_pushb_double(doubles, (double)i);
#if 0 /* DO NOT DO THIS; this is a prime example of the 'iterators' becoming
         invalidated as the vector grows and memory likel moves */
        Vect_pushb_voidptr(pointers, Vect_getptr_voidptr(doubles, i));
#endif
    }

    // for each double in doubles, get a pointer to it and store it in pointers
    for (size_t i = 0; i <= num; ++i){
        Vect_pushb_voidptr(pointers, Vect_getptr_voidptr(doubles, i));
    }

    // confirm the the double we get through the pointer is the same one we
    // get by calling get() on the doubles vector
    double **s = (double **)Vect_begin_voidptr(pointers);
    double **e = (double **)Vect_end_voidptr(pointers);
    for (size_t i = 0; s < e; ++s, ++i){
        //debug("expected %f got %f", Vect_get_double(doubles, i), **s);
        if (**s != Vect_get_double(doubles, i)){
            debug("expected %f got %f", Vect_get_double(doubles, i), **s);
            return TEST_FAIL;
        }
    }

    Vect_destroy(&doubles);
    Vect_destroy(&pointers);
    return TEST_PASS;
}

enum testStatus test_vect_insert_and_remove(void){
    vector *v = Vect_new_u16(6);
    size_t num = 6000;
    for (size_t i = 0; i < num; ++i){
        //debug("inserting %uth", i);
        Vect_insert_u16(v, 0, i);
    }

    // confirm the numbers have been inserted in reverse
    for (size_t i = 1; i<=num; ++i){
        uint16_t val = Vect_front_u16(v);
        if (val != num-i){
            debug("expected %u got %u", num-i, val);
            return TEST_FAIL;
        }
        //debug("removing %uth item", i);
        Vect_remove_u16(v, 0);
    }

    Vect_destroy(&v);
    return TEST_PASS;
}

enum testStatus test_vect_resize(void){
    vector *v = Vect_new_u8(0);
    size_t num = 255;
    for (size_t i = 0; i < num; ++i){
        Vect_pushb_u8(v, i);
    }

    Vect_resize_u8(v, 10, 0);
    if (Vect_count(v) != 10){
        debug("expected %zu got %zu", 10, Vect_count(v));
        return TEST_FAIL;
    }

    size_t size = 500 * 1000;
    uint8_t fill = 103;
    Vect_resize_u8(v, size, fill);
    if (Vect_count(v) != size){
        debug("expected count %zu got %zu", size, Vect_count(v));
        return TEST_FAIL;
    }
    for (size_t i = 11; i < size; ++i){
        if (Vect_get_u8(v, i) != fill){
            debug("expected %u got %u", fill, Vect_get_u8(v, i));
            return TEST_FAIL;
        }
    }

    Vect_destroy(&v);
    return TEST_PASS;
}

int main(int argc, char *argv[]){
    UNUSED(argc);
    UNUSED(argv);

    struct cohort *testlist = Cohort_init();
    Cohort_add(testlist, test_vect_count, "reports count correctly");
    Cohort_add(testlist, test_vect_pushb_and_popb, "tail push/pop ops work correctly");
    Cohort_add(testlist, test_vect_front_and_back, "front/back getters work correctly");
    Cohort_add(testlist, test_vect_begin_and_end_pointers, "begin/end/getptr pointer-getters work ok");
    Cohort_add(testlist, test_vect_insert_and_remove, "insertion and removal");
    Cohort_add(testlist, test_vect_resize, "resizing works");
    Cohort_decimate(testlist);
    Cohort_destroy(testlist);
}

