#include "cohort.h"

#include <stdio.h>
#include <stdbool.h>

#include <tarp/stackk.h>

STACK_HEAD(mystack, lease);

struct lease {
    int id;
    STACK_ENTRY(lease) stack;
};



bool stack_dump(struct mystack *stack){
    struct lease *node = NULL;
    STACK_FOREACH(stack, node, stack){
        printf(" => node with id %i\n", node->id);
    }
}

/*
   Push and pop to to the stack and check that the counter is correctly
   updated.
*/
enum status can_push(void){
    struct mystack mystack = STACK_HEAD_STATIC_INITIALIZER;

    struct lease lease1 = {.id = 1};
    struct lease lease2 = {.id = 2};
    struct lease lease3 = {.id = 3};
    struct lease lease4 = {.id = 4};
    struct lease lease5 = {.id = 5};
    
    printf("stack empty? %i, count=%i\n", STACK_EMPTY(&mystack), STACK_COUNT(&mystack));;
    STACK_PUSH(&mystack, &lease5, stack);
    STACK_PUSH(&mystack, &lease4, stack);
    STACK_PUSH(&mystack, &lease3, stack);
    STACK_PUSH(&mystack, &lease2, stack);
    STACK_PUSH(&mystack, &lease1, stack);
    
    printf("stack empty? %i, count=%i\n", STACK_EMPTY(&mystack), STACK_COUNT(&mystack));;

    struct lease *curr = NULL;
    stack_dump(&mystack); 

    //STACK_UPEND(&mystack);
    struct lease *temp, *dummy = NULL;
    puts("=====");
    //STACK_UPEND(&mystack, stack, curr, temp, dummy);
    STACK_UPEND(&mystack, lease, stack);

    stack_dump(&mystack); 

    printf("stack empty? %i, count=%i\n", STACK_EMPTY(&mystack), STACK_COUNT(&mystack));;

    return SUCCESS;
}

