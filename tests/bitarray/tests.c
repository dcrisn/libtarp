#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdlib.h>
#include <string.h>
#include <tarp/common.h>
#include <tarp/bitarray.h>
#include <tarp/cohort.h>
#include <tarp/log.h>
#include <tarp/bits.h>


typedef struct bitarray *ba;

static enum testStatus test_standard_bitarray_creation(
        size_t width, bool all_ones)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_new(width, all_ones);
    if (a->width != width) status = TEST_FAIL;
    if (a->size != bits2bytes(width, true)) status = TEST_FAIL;

    size_t byte = all_ones ? FULL_BYTE : NULL_BYTE;
    for (size_t i = 0; i < a->size; ++i){
        if (a->bytes[i] != byte) status = TEST_FAIL;
    }

    free(a);
    return status;
}

static enum testStatus test_bitarray2string(
        size_t width, bool all_ones, int split_every,
        const char *sep, const char *const expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_new(width, all_ones);
    char *s = Bitr_tostring(a, split_every, sep);

    if (strcmp(s, expected)) status = TEST_FAIL;
    cond_test_debug_print(status, "result='%s' expected='%s'", s, expected);

    free(s);
    Bitr_destroy(&a);
    return status;
}

/*
 * If create_separate_bitarray=true, then a bitarray is created as specified,
 * and passed to the Bitr_fromstring() function instead of passing it NULL. */
static enum testStatus test_bitarray_fromstring(
        bool create_separate_bitarray, size_t width, bool all_ones,
        const char *const initializer, const char *expected)
{
    enum testStatus status = TEST_PASS;

    ba a = NULL;
    if (create_separate_bitarray){
        a = Bitr_new(width, all_ones);
        ba b = Bitr_fromstring(a, initializer);

        if (!b) {
            Bitr_destroy(&a);
            cond_test_debug_print(TEST_FAIL, "Bitr_fromstring() returned NULL");
            status = TEST_FAIL;
        }

        a = b; /* b was simply the same as a if not NULL */
    } else {
        a = Bitr_fromstring(NULL, initializer);
        if (!a){
            cond_test_debug_print(TEST_FAIL, "Bitr_fromstring() returned NULL");
            status = TEST_FAIL;
        }
    }

    if (status == TEST_PASS){
        char *s = Bitr_tostring(a, 0, NULL);
        if (!match(s, expected)){
            status = TEST_FAIL;
            cond_test_debug_print(status, "result='%s' expected='%s'", s, expected);
        }
        free(s);
    }

    Bitr_destroy(&a);
    return status;
}

static enum testStatus test_bitarray_clone(const char *initializer){
    enum testStatus status = TEST_PASS;
    const char *expected = initializer;
    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
    } else {
        char *s = Bitr_tostring(a, 0, NULL);
        if (!match(s, expected)){
            cond_test_debug_print(TEST_FAIL, "expected: '%s' | result: '%s'",
                    expected, s);
            status = TEST_FAIL;
        }
        free(s);
    }

    Bitr_destroy(&a);
    return status;
}

static enum testStatus test_bitarray_repeat(size_t times,
        const char *initializer, const char *expected)
{
    enum testStatus status = TEST_PASS;

    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
    }
    else {
        ba b = Bitr_repeat(a, times);
        if (!b){
            status = TEST_FAIL;
        }
        else {
            char *s = Bitr_tostring(b, 0, NULL);
            if (!match(s, expected)){
                cond_test_debug_print(TEST_FAIL, "expected: '%s' | result: '%s'",
                        expected, s);
                status = TEST_FAIL;
            }
            free(s);
            Bitr_destroy(&b);
        }
    }

    Bitr_destroy(&a);
    return status;
}

static enum testStatus test_bitarray_reverse(size_t times,
        const char *initializer, const char *expected)
{
    enum testStatus status = TEST_PASS;

    ba a = Bitr_fromstring(NULL, initializer);
    char *s = "";
    if (a){
        for (unsigned int i = 0; i < times; ++i){
            a = Bitr_reverse(a);
        }
        s = Bitr_tostring(a, 0, NULL);
        assert(s);
        if (!match(s, expected)){
            status = TEST_FAIL;
            cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
        }
        free(s);
    } else {
        status = TEST_FAIL;
    }

    Bitr_destroy(&a);
    return status;
}

static enum testStatus test_bitarray_join(
        const char *initializer_a,
        const char *initializer_b,
        const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer_a);
    ba b = Bitr_fromstring(NULL, initializer_b);
    if (!a || !b){
        status = TEST_FAIL;
    } else {
        ba c = Bitr_join(a, b);
        if (!c){
            status = TEST_FAIL;
        } else {
            char *s = Bitr_tostring(c, 0, NULL);
            if (!match(s, expected)){
                cond_test_debug_print(TEST_FAIL, "expected: '%s' | result: '%s'",
                        expected, s);
                status = TEST_FAIL;
            }
            free(s);
            Bitr_destroy(&c);
        }
    }

    Bitr_destroy(&a);
    Bitr_destroy(&b);
    return status;
}

/*
 * Copy the bytes from 'bytes' to a buffer, in little endian order,
 * then initialize a bitarray from this buffer.
 *
 * If create_separate_bitarray=true, then a separate bitarray is created
 * according to the arguments specified -- instead of letting Bitr_frombuffer
 * allocate anything.
 */
static enum testStatus test_bitarray_frombuffer(
        bool create_separate_bitarray, size_t width, bool all_ones,
        uint64_t initializer, size_t num_bytes, const char *expected)
{
    uint8_t buff[8];
    pack_buff_le(initializer, buff);

    enum testStatus status = TEST_PASS;

    ba a = NULL;
    if (create_separate_bitarray){
        a = Bitr_new(width, all_ones);
        ba b = Bitr_frombuff(a, buff, num_bytes);

        if (!b) {
            Bitr_destroy(&a);
            cond_test_debug_print(TEST_FAIL, "Bitr_frombuff() returned NULL");
            status = TEST_FAIL;
        }

        a = b; /* b was simply the same as a if not NULL */
    } else {
        a = Bitr_frombuff(NULL, buff, num_bytes);
        if (!a){
            cond_test_debug_print(TEST_FAIL, "Bitr_frombuff() returned NULL");
            status = TEST_FAIL;
        }
    }

    if (status == TEST_PASS){
        char *s = Bitr_tostring(a, 0, NULL);
        if (!match(s, expected)){
            status = TEST_FAIL;
            cond_test_debug_print(status, "result='%s' expected='%s'", s, expected);
        }
        free(s);
    }

    Bitr_destroy(&a);
    return status;
}

/*
 * The others (u8, u16, u64) are identical and if this and the frombuffer methods
 * work, then initialization from the other integral types works as well */
static enum testStatus test_bitarray_fromu32(bool create_separate_bitarray,
        size_t width, bool all_ones, uint32_t initializer, const char *expected)
{
    enum testStatus status = TEST_PASS;

    ba a = NULL;
    if (create_separate_bitarray){
        a = Bitr_new(width, all_ones);
        ba b = Bitr_fromu32(a, initializer);

        if (!b) {
            Bitr_destroy(&a);
            cond_test_debug_print(TEST_FAIL, "Bitr_fromu32() returned NULL");
            status = TEST_FAIL;
        }

        a = b; /* b was simply the same as a if not NULL */
    } else {
        a = Bitr_fromu32(NULL, initializer);
        if (!a){
            cond_test_debug_print(TEST_FAIL, "Bitr_fromu32() returned NULL");
            status = TEST_FAIL;
        }
    }

    if (status == TEST_PASS){
        char *s = Bitr_tostring(a, 0, NULL);
        if (!match(s, expected)){
            status = TEST_FAIL;
            cond_test_debug_print(status, "result='%s' expected='%s'", s, expected);
        }
        free(s);
    }

    Bitr_destroy(&a);
    return status;
}

enum testStatus test_bitarray_slice(const char *initializer,
        size_t start, size_t end, const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    ba b = Bitr_slice(a, start, end);
    if (b){
        char *s = Bitr_tostring(b, 0, NULL);
        if (!match(s, expected)){
            cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
            status = TEST_FAIL;
        }
        free(s);
    }

    Bitr_destroy(&a);
    Bitr_destroy(&b);
    return status;
}

// shift left if left=1, else right
enum testStatus test_bitarray_shift(const char *initializer,
        bool left, bool rotate, size_t n, const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    if (left) Bitr_lshift(a, n, rotate);
    else Bitr_rshift(a, n, rotate);

    char *s = Bitr_tostring(a, 0, NULL);
    assert(s);
    if (!match(s, expected)){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
    }

    free(s);
    Bitr_destroy(&a);

    return status;
}

// push left if left=1, else right
// bit=1 if on_bit=true, else 0
enum testStatus test_bitarray_push(const char *initializer,
        bool left, unsigned int times, bool on_bit, const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    for (unsigned int i = 0; i < times; ++i){
        if (left) Bitr_lpush(a, on_bit);
        else Bitr_rpush(a, on_bit);
    }

    char *s = Bitr_tostring(a, 0, NULL);
    assert(s);
    if (!match(s, expected)){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
    }

    free(s);
    Bitr_destroy(&a);

    return status;
}

// if times>1, expected_bit is the last bit popped
enum testStatus test_bitarray_pop(const char *initializer,
        bool left, unsigned int times, const char *expected, int expected_bit)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    if (!a){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    int bit = -1;
    for (unsigned int i = 0; i < times; ++i){
        if (left) bit = Bitr_lpop(a);
        else bit = Bitr_rpop(a);
    }

    char *s = Bitr_tostring(a, 0, NULL);
    assert(s);
    if (!match(s, expected)){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
    }

    if (bit != expected_bit){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: %d | result: %d", expected_bit, bit);
    }

    free(s);
    Bitr_destroy(&a);

    return status;
}

/*
 * For bop: 1=binary or, 2=binary and, 3=binary xor */
enum testStatus test_bitarray_bops(const char *initializer_a, const char *initializer_b,
        unsigned int bop, const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer_a);
    ba b = Bitr_fromstring(NULL, initializer_b);

    if (!a || !b){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    switch(bop){
    case 1:
        Bitr_bor(a, b);
        break;
    case 2:
        Bitr_band(a, b);
        break;
    case 3:
        Bitr_bxor(a, b);
        break;
    default:
        assert(false);
    }

    char *s = Bitr_tostring(a, 0, NULL);
    assert(s);
    if (!match(s, expected)){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
    }

    free(s);
    Bitr_destroy(&a);
    Bitr_destroy(&b);

    return status;
}

/*
 * For op: 1=any(), 2=all() 3=none() */
enum testStatus test_bitarray_set_tester(const char *initializer,
        unsigned int op, bool expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    assert(a);

    bool res = false;

    switch(op){
    case 1:
        res = Bitr_any(a);
        break;
    case 2:
        res = Bitr_all(a);
        break;
    case 3:
        res = Bitr_none(a);
        break;
    default:
        assert(false);
    }

    if (res != expected){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", bool2str(expected), bool2str(res));
    }

    Bitr_destroy(&a);

    return status;
}

/*
 * For op: 1=any(), 2=all() 3=none() */
enum testStatus test_bitarray_bulk_methods_tester(const char *initializer,
        unsigned int op, size_t pos, size_t how_many, const char *expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer);
    assert(a);

    switch(op){
    case 1:
        Bitr_setn(a, pos, how_many);
        break;
    case 2:
        Bitr_clearn(a, pos, how_many);
        break;
    case 3:
        Bitr_togglen(a, pos, how_many);
        break;
    default:
        assert(false);
    }

    char *s = Bitr_tostring(a, 0, NULL);
    assert(a);
    if (!(match(s, expected))){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'", expected, s);
    }
    free(s);
    Bitr_destroy(&a);

    return status;
}

/*
 * For bop: 1=binary or, 2=binary and, 3=binary xor */
enum testStatus test_bitarray_equality(const char *initializer_a, const char *initializer_b,
        bool expected)
{
    enum testStatus status = TEST_PASS;
    ba a = Bitr_fromstring(NULL, initializer_a);
    ba b = Bitr_fromstring(NULL, initializer_b);

    if (!a || !b){
        status = TEST_FAIL;
        cond_test_debug_print(status, "fromstring() returned NULL");
        return status;
    }

    bool res = Bitr_equal(a, b);
    if (res != expected){
        status = TEST_FAIL;
        cond_test_debug_print(status, "expected: '%s' | result: '%s'",
                bool2str(expected), bool2str(res));
    }

    Bitr_destroy(&a);
    Bitr_destroy(&b);

    return status;
}

int main(int argc, char **argv){
    UNUSED(argc);
    UNUSED(argv);

    prepare_test_variables();

#if 0
    puts("");
    puts("#####################################################");
    puts("################ Running C tests #################### ");
    puts("#####################################################");
#endif

    printf("\n%s\n", "===== Validating standard bitarray initialization ========= ");
    run(test_standard_bitarray_creation, TEST_PASS, 1, true);
    run(test_standard_bitarray_creation, TEST_PASS, 8, true);
    run(test_standard_bitarray_creation, TEST_PASS, 9, true);
    run(test_standard_bitarray_creation, TEST_PASS, 15, true);
    run(test_standard_bitarray_creation, TEST_PASS, 16, false);
    run(test_standard_bitarray_creation, TEST_PASS, 17, true);
    run(test_standard_bitarray_creation, TEST_PASS, 500000, true);
    run(test_standard_bitarray_creation, TEST_PASS, 2341414, false);

    printf("\n%s\n", "===== Validating bitarray string dump ========= ");
    run(test_bitarray2string, TEST_PASS, 1, true, 0, NULL, "1");
    run(test_bitarray2string, TEST_PASS, 3, false, 0, NULL, "000");
    run(test_bitarray2string, TEST_PASS, 8, true, 8, NULL, "11111111");
    run(test_bitarray2string, TEST_PASS, 9, false, 8, ".", "0.00000000");
    run(test_bitarray2string, TEST_PASS, 9, true, 3, "-", "111-111-111");
    run(test_bitarray2string, TEST_PASS, 24, true, 8, ".", "11111111.11111111.11111111");
    run(test_bitarray2string, TEST_PASS, 16, false, 7, ";;", "00;;0000000;;0000000");
    run(test_bitarray2string, TEST_PASS, 34, true, 11, "/", "1/11111111111/11111111111/11111111111");

    printf("\n%s\n", "===== Validating bitarray initialization from string ========= ");
    run(test_bitarray_fromstring, TEST_PASS, false, 0, false,"111100001", "111100001");
    run(test_bitarray_fromstring, TEST_PASS, false, 0, false, "0", "0");
    run(test_bitarray_fromstring, TEST_PASS, false, 0, false, "01111111111111110", "01111111111111110");
    run(test_bitarray_fromstring, TEST_PASS, true, 4, false, "1111", "1111");
    run(test_bitarray_fromstring, TEST_PASS, true, 8, true, "0001", "11110001");
    run(test_bitarray_fromstring, TEST_FAIL, true, 4, true, "11111", NULL);
    run(test_bitarray_fromstring, TEST_PASS, true, 9, true, "00000001", "100000001");
    run(test_bitarray_fromstring, TEST_FAIL, true, 9, true, "00000001.", "");
    run(test_bitarray_fromstring, TEST_FAIL, true, 9, true, "00000001*", "");
    run(test_bitarray_fromstring, TEST_FAIL, true, 9, true, "00000001 ", "");
    run(test_bitarray_fromstring, TEST_FAIL, true, 9, true, " 00000001", "");

    printf("\n%s\n", "===== Validating bitarray initialization from buffer ========= ");
    run(test_bitarray_frombuffer, TEST_PASS, false, 0, false, 0xff, 1, "11111111");
    run(test_bitarray_frombuffer, TEST_PASS, true,  8, false, 0xff, 1, "11111111");
    run(test_bitarray_frombuffer, TEST_FAIL, true,  9, false, 0xff, 2, "011111111");
    run(test_bitarray_frombuffer, TEST_FAIL, true,  9, false, 0xff, 2, "011111111");
    run(test_bitarray_frombuffer, TEST_PASS, true,  16, false, 0xff, 2, "0000000011111111");
    run(test_bitarray_frombuffer, TEST_FAIL, true,  23, true, 0xff, 3, NULL);
    run(test_bitarray_frombuffer, TEST_PASS, true,  24, true, 0xff, 3, "000000000000000011111111");
    run(test_bitarray_frombuffer, TEST_PASS, true,  16, true, 0xff, 1, "1111111111111111");
    run(test_bitarray_frombuffer, TEST_PASS, true,  64, true, 0x11207db9a148c3c1, 8, "0001000100100000011111011011100110100001010010001100001111000001");
    run(test_bitarray_frombuffer, TEST_PASS, true,  32, true, 0x75b1fe7, 4, "00000111010110110001111111100111");
    run(test_bitarray_frombuffer, TEST_PASS, false,  0, false, 0x75b1fe7, 4, "00000111010110110001111111100111");

    printf("\n%s\n", "===== Validating bitarray initialization from u32 ========= ");
    run(test_bitarray_fromu32, TEST_FAIL, true, 31, false, 0, NULL);
    run(test_bitarray_fromu32, TEST_FAIL, true, 18, false, 0, NULL);
    run(test_bitarray_fromu32, TEST_PASS, false, 0, false, 999991131, "00111011100110101010011101011011");
    run(test_bitarray_fromu32, TEST_PASS, false, 0, false, 1, "00000000000000000000000000000001");

    printf("\n%s\n", "===== Validating bitarray cloning ========= ");
    run(test_bitarray_clone, TEST_PASS, "1");
    run(test_bitarray_clone, TEST_PASS, "0001110");
    run(test_bitarray_clone, TEST_PASS, "111111111111111111111111111111");
    run(test_bitarray_clone, TEST_PASS, "111111110");
    run(test_bitarray_clone, TEST_PASS, "1000000");

    printf("\n%s\n", "===== Validating bitarray join ========= ");
    run(test_bitarray_join, TEST_PASS, "1111", "0000", "11110000");
    run(test_bitarray_join, TEST_FAIL, "1111", "0000", "11111000");
    run(test_bitarray_join, TEST_PASS, "1111111", "0", "11111110");
    run(test_bitarray_join, TEST_PASS, "11111111", "0000000011111111", "111111110000000011111111");
    run(test_bitarray_join, TEST_PASS, "000001111", "0000001", "0000011110000001");

    printf("\n%s\n", "===== Validating bitarray repeat ========= ");
    run(test_bitarray_repeat, TEST_PASS, 8, "1", "11111111");
    run(test_bitarray_repeat, TEST_PASS, 2, "1010", "10101010");
    run(test_bitarray_repeat, TEST_PASS, 3, "101", "101101101");
    run(test_bitarray_repeat, TEST_PASS, 5, "01", "0101010101");
    run(test_bitarray_repeat, TEST_PASS, 3, "111111111", "111111111111111111111111111");
    run(test_bitarray_repeat, TEST_PASS, 4, "11111111", "11111111111111111111111111111111");

    printf("\n%s\n", "===== Validating bitarray slicing ========= ");
    run(test_bitarray_slice, TEST_PASS, "11110000", 1, 5, "0000");
    run(test_bitarray_slice, TEST_PASS, "11110000", 0, 0, "11110000");
    run(test_bitarray_slice, TEST_PASS, "11110000", 6, 0, "111");
    run(test_bitarray_slice, TEST_PASS, "11110000", 0, 7, "110000");
    run(test_bitarray_slice, TEST_PASS, "11110000", 0, 8, "1110000");
    run(test_bitarray_slice, TEST_PASS, "11110000", 0, 9, "11110000");
    run(test_bitarray_slice, TEST_PASS, "11110000", 7, 9, "11");
    run(test_bitarray_slice, TEST_PASS, "111111110", 9, 10, "1");
    run(test_bitarray_slice, TEST_PASS, "111111110", 1, 2, "0");

    printf("\n%s\n", "===== Validating bitarray shifting ========= ");
    run(test_bitarray_shift, TEST_PASS, "100000001", true, false, 3, "000001000")
    run(test_bitarray_shift, TEST_PASS, "1", true, false, 1, "0")
    run(test_bitarray_shift, TEST_PASS, "0000", true, false, 4, "0000")
    run(test_bitarray_shift, TEST_PASS, "111100001", true, true, 9, "111100001")
    run(test_bitarray_shift, TEST_PASS, "111100001", true, true, 8, "111110000")
    run(test_bitarray_shift, TEST_PASS, "11111111111", false, false, 10, "00000000001")
    run(test_bitarray_shift, TEST_PASS, "11111111000000001111111100000000", false, true, 24, "00000000111111110000000011111111");
    run(test_bitarray_shift, TEST_PASS, "11111111000000001111111100000000", false, false, 24,"00000000000000000000000011111111");
    run(test_bitarray_shift, TEST_PASS, "111111101", false, false, 8,"000000001");
    run(test_bitarray_shift, TEST_PASS, "111111101", true, false, 7,"010000000");

    printf("\n%s\n", "===== Validating bitarray push/pop ========= ");
    run(test_bitarray_push, TEST_PASS,"00000000000", false, 1, true, "00000000001");
    run(test_bitarray_push, TEST_PASS,"00000000000", true, 1, true, "10000000000");
    run(test_bitarray_pop, TEST_PASS, "00000000001", false, 1, "00000000000", 1);
    run(test_bitarray_pop, TEST_PASS, "10000000000", true, 1, "00000000000", 1);
    run(test_bitarray_pop, TEST_PASS, "1110000000", true, 2, "1000000000", 1);
    run(test_bitarray_push, TEST_PASS, "00000000", true, 100000, true, "11111111");

    printf("\n%s\n", "===== Validating bitarray bitwise methods ========= ");
    run(test_bitarray_bops, TEST_PASS, "000000001", "111111100", 1, "111111101");
    run(test_bitarray_bops, TEST_PASS, "000000001", "111111100", 2, "000000000");
    run(test_bitarray_bops, TEST_PASS, "000000001", "111111100", 3, "111111101");
    run(test_bitarray_bops, TEST_PASS, "11110000", "11111111", 2, "11110000");
    run(test_bitarray_bops, TEST_PASS, "11110000", "11111111", 1, "11111111");
    run(test_bitarray_bops, TEST_PASS, "11110000", "11111111", 3, "00001111");

    printf("\n%s\n", "===== Validating bitarray equality ========= ");
    run(test_bitarray_equality, TEST_PASS, "11111", "1111", false);
    run(test_bitarray_equality, TEST_PASS, "11111", "11110", false);
    run(test_bitarray_equality, TEST_PASS, "00000000", "000000001", false);
    run(test_bitarray_equality, TEST_PASS, "00000001", "000000001", false);
    run(test_bitarray_equality, TEST_PASS, "1", "1", true);
    run(test_bitarray_equality, TEST_PASS, "0", "1", false);
    run(test_bitarray_equality, TEST_PASS, "01", "1", false);

    printf("\n%s\n", "===== Validating bitarray reverse method ========= ");
    run(test_bitarray_reverse, TEST_PASS, 1, "11110000", "00001111");
    run(test_bitarray_reverse, TEST_PASS, 1, "00000111", "11100000");
    run(test_bitarray_reverse, TEST_PASS, 1, "100000000000", "000000000001");

    printf("\n%s\n", "===== Validating bitarray set tester ========= ");
    run(test_bitarray_set_tester, TEST_PASS, "11110000", 1, true);
    run(test_bitarray_set_tester, TEST_PASS, "11110000", 2, false);
    run(test_bitarray_set_tester, TEST_PASS, "11110000", 3, false);
    run(test_bitarray_set_tester, TEST_PASS, "00000000", 3, true);
    run(test_bitarray_set_tester, TEST_PASS, "0", 3, true);
    run(test_bitarray_set_tester, TEST_PASS, "1", 3, false);
    run(test_bitarray_set_tester, TEST_PASS, "100000000000000", 2, false);
    run(test_bitarray_set_tester, TEST_PASS, "100000000000000", 1, true);
    run(test_bitarray_set_tester, TEST_PASS, "1", 2, true);
    run(test_bitarray_set_tester, TEST_PASS, "1111", 2, true);

    printf("\n%s\n", "===== Validating bitarray bulk methods ========= ");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "11110000", 1, 2, 1, "11110010");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "11111111", 2, 8, 1, "01111111");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "11111111", 2, 8, 7, "00000001");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "11111111", 2, 8, 8, "00000000");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "00000000", 2, 8, 8, "00000000");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "00000000", 3, 8, 8, "11111111");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "10101010111", 3, 11, 8, "01010101111");
    run(test_bitarray_bulk_methods_tester, TEST_PASS, "11111111111111111", 2, 17, 16, "00000000000000001");

report_test_summary();
}
