#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <tarp/dllist.h>
#include "cohort.h"

struct testnode{
    char *name;
    struct dlnode link;
    uint64_t id;
};

/*
 * Test if the number of items is correctly maintained and 
 * reported through deletions and insertions. 
 */
enum status keeps_count(void){
    struct dllist mylist = DLL_STATIC_INITIALIZER;
    if (DLL_count(&mylist)) return FAILURE;

    struct testnode one = {.name = "one", .id = 111};
    struct testnode two = {.name = "two", .id = 222};
    struct testnode three = {.name = "three", .id = 333};
    struct testnode four = {.name = "four", .id = 444};
    struct testnode five = {.name = "five", .id = 555};
    struct testnode six = {.name = "six", .id = 666};
    struct testnode seven = {.name = "seven", .id = 777};
    
    DLL_append(&mylist, &one.link);
    DLL_append(&mylist, &two.link);
    DLL_prepend(&mylist, &three.link);
    DLL_prepend(&mylist, &four.link);
    if (DLL_empty(&mylist) || DLL_count(&mylist) != 4) return FAILURE;

    DLL_append(&mylist, &five.link);
    DLL_append(&mylist, &six.link);
    DLL_prepend(&mylist, &seven.link);
    if (DLL_empty(&mylist) || DLL_count(&mylist) != 7) return FAILURE;

    DLL_popt(&mylist);
    DLL_poph(&mylist);
    if (DLL_empty(&mylist) || DLL_count(&mylist) != 5) return FAILURE;

    DLL_poph(&mylist);
    DLL_poph(&mylist);
    DLL_poph(&mylist);
    DLL_poph(&mylist);
    DLL_popt(&mylist);
    if (!DLL_empty(&mylist) || DLL_count(&mylist) != 0) return FAILURE;

    return SUCCESS;
}
