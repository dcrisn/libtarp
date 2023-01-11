#include "cohort.h"
#include "stack.h"

/*
   Push and pop to to the stack and check that the counter is correctly
   updated.
*/
enum status keeps_correct_count(void){
    Stack s = NULL;
    Stack_dynamic_init(&s);

    enum status res = SUCCESS;

    // empty stack
    if(Stack_count(s)){
        res = FAILURE;
    }

    // push 7
    int i = 1;
    Stack_push(s, &i);
    Stack_push(s, &i);
    Stack_push(s, &i);
    Stack_push(s, &i);
    Stack_push(s, &i);
    Stack_push(s, &i);
    Stack_push(s, &i);
    if (Stack_count(s) != 7){
        res = FAILURE;
    }

    // pop 2
    Stack_pop(s, 1, false);
    Stack_pop(s, 1, false);

    if (Stack_count(s) != 5){
        res = FAILURE;
    }

    Stack_dynamic_destroy(&s, false);
    return res;
}

/*
   Push and pop to to the stack and check that the value popped matches
   what you'd expect.
*/
enum status can_pop_top(void){
    struct stack s;
    Stack_static_init(&s);

    enum status res = SUCCESS;

    // push 1
    int one = 1;
    Stack_push(&s, &one);
    int r = *(int *)Stack_pop(&s, 1, false);
    if (r != one){
        res = FAILURE;
    }
   
    // push a couple more and then push '2617'
    Stack_push(&s, &one);
    Stack_push(&s, &one);
    Stack_push(&s, &one);
    Stack_push(&s, &one);
    Stack_push(&s, &one);

    int two = 23714;
    Stack_push(&s, &two);

    r = *(int *)Stack_pop(&s, 1, false);
    if (r != two){
        res = FAILURE;
    }
    
    // popping again should given us 1, not 23714
    r = *(int *)Stack_pop(&s, 1, false);
    if (r != one){
        res = FAILURE;
    }

    Stack_static_destroy(&s, false);
    return res;
}

/*
   Check that popping from an empty stack returns NULL, as you'd expect, and that
   the counter stays correct.
*/
enum status pop_empty_stack(void){
    Stack s = NULL;
    Stack_dynamic_init(&s);
    
    int test = 5;

    enum status res = SUCCESS;
    
    // empty stack
    if(Stack_count(s)){
        res = FAILURE;
    }

    if ((Stack_pop(s, 1, false))) res = FAILURE;
    if (Stack_count(s)){
        res = FAILURE;
    }

    // push a few, pop them off, and check the stack knows it's empty
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_pop(s, 1, false);
    Stack_pop(s, 1, false);
    Stack_pop(s, 1, false);
    
    if ((Stack_pop(s, 1, false))) res = FAILURE;
    if (Stack_count(s)){
        res = FAILURE;
    }

    Stack_dynamic_destroy(&s, false);
    return res;
}

/*
   Check that we can pop the nth item from the top, and that trying to pop an item
   out of bounds returns NULL.
*/
enum status can_pop_nth(void){
    Stack s = NULL;
    Stack_dynamic_init(&s);
    
    // some random items to populate the stack
    int test = 5;
    int test2 = 116;
    int r;

    enum status res = SUCCESS;
    
    // empty stack
    if(Stack_count(s)){
        res = FAILURE;
    }

    if ((Stack_pop(s, 2, false))) res = FAILURE;
    if (Stack_count(s)){
        res = FAILURE;
    }

    // push 7 few, pop them off, and check the stack knows it's empty
    Stack_push(s, &test);
    Stack_push(s, &test2);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);

    // this leaves 5 on the stack
    r = *(int *)Stack_pop(s, 2, false);
    if (r != test || Stack_count(s) != 5){
        res = FAILURE;
    }

    // 6 on the stack now
    Stack_push(s, &test);
    // get the 5th item: i.e. leave only one item
    r = *(int *)Stack_pop(s, 5, false);
    if (r != test2 || Stack_count(s) != 1){
        res = FAILURE;
    }

    Stack_dynamic_destroy(&s, false);
    return res;
}

/*
   Check that we can peek at the top of the stack or the nth item from the
   top of the stack and the stack is left intact + we get the correct
   value.
*/
enum status can_peek(void){
    Stack s = NULL;
    Stack_dynamic_init(&s);
    
    // some random items to populate the stack
    int test = 5;
    int test2 = 116;
    int r;

    enum status res = SUCCESS;
    
    // empty stack
    if(Stack_count(s)){
        res = FAILURE;
    }

    // nothing to peek at
    if ((Stack_peek(s, 1))) res = FAILURE;
    if ((Stack_peek(s, 31))) res = FAILURE;

    // push 8 in a specific order and check the value returned by peek
    // and that the counter stays correct
    Stack_push(s, &test);
    Stack_push(s, &test2);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test);
    Stack_push(s, &test2);

    r = *(int *)Stack_peek(s, 1);
    if (r != test2 || Stack_count(s) != 8){
        res = FAILURE;
    }

    r = *(int *)Stack_peek(s, 2);
    if (r != test || Stack_count(s) != 8){
        res = FAILURE;
    }

    r = *(int *)Stack_peek(s, 8);
    if (r != test || Stack_count(s) != 8){
        res = FAILURE;
    }

    r = *(int *)Stack_peek(s, 7);
    if (r != test2 || Stack_count(s) != 8){
        res = FAILURE;
    }

    r = *(int *)Stack_pop(s, 2, false);
    if (r != test || Stack_count(s) != 6){
        res = FAILURE;
    }

    Stack_dynamic_destroy(&s, false);
    return res;
}

/*
   Check that upending works: an upended empty stack is still empty and a stack
   with items has its items reversed.
*/
enum status can_upend(void){
    struct stack s;
    Stack_static_init(&s);
    
    // items for populating the stack
    int r;
    int one = 1;
    int two = 2;
    int three = 3;
    int four = 4;
    int five = 5;
    int six = 6;
    int seven = 7;

    enum status res = SUCCESS;
    
    // upend empty stack
    Stack_upend(&s);
    
    // should still be empty
    if(Stack_count(&s)){
        res = FAILURE;
    }
    
    // stack with one item: check item remains intact
    Stack_push(&s, &one);
    Stack_upend(&s);
    r = *(int *)Stack_peek(&s, 1);
    if(Stack_count(&s) != 1 || r != 1){
        res = FAILURE;
    }
    
    // check items are reversed in multi-item stack
    Stack_push(&s, &two);
    Stack_push(&s, &three);
    Stack_push(&s, &four);
    Stack_push(&s, &five);
    Stack_push(&s, &six);
    Stack_push(&s, &seven);

    if (*(int *)Stack_peek(&s, 1) != seven) res = FAILURE;
    if (*(int *)Stack_peek(&s, 2) != six) res = FAILURE;
    if (*(int *)Stack_peek(&s, 3) != five) res = FAILURE;
    if (*(int *)Stack_peek(&s, 4) != four) res = FAILURE;
    if (*(int *)Stack_peek(&s, 5) != three) res = FAILURE;
    if (*(int *)Stack_peek(&s, 6) != two) res = FAILURE;
    if (*(int *)Stack_peek(&s, 7) != one) res = FAILURE;
    
    // still full
    if (Stack_count(&s) != 7) res = FAILURE;

    Stack_static_destroy(&s, false);
    return res;
}

/*
   Check that a stack can be copied.
*/
enum status can_copy(void){
    struct stack stackA;
    struct stack *stackB;
    Stack_static_init(&stackA);
    
    // items for populating the stack
    int one = 1;
    int two = 2;
    int three = 3;
    int four = 4;
    int five = 5;
    int six = 6;
    int seven = 7;

    enum status res = SUCCESS;
    stackB = Stack_copy(&stackA);
     
    // both should be emty
    if(Stack_count(&stackA) || Stack_count(stackB)){
        res = FAILURE;
    }
    
    Stack_dynamic_destroy(&stackB, true);

    // populate stack and copy it then
    Stack_push(&stackA, &one);
    Stack_push(&stackA, &two);
    Stack_push(&stackA, &three);
    Stack_push(&stackA, &four);
    Stack_push(&stackA, &five);
    Stack_push(&stackA, &six);
    Stack_push(&stackA, &seven);
    
    stackB = Stack_copy(&stackA);
    // check that the items are identical
    if (Stack_count(&stackA) != Stack_count(stackB)) res = FAILURE;
    if (Stack_peek(&stackA, 1) != Stack_peek(stackB, 1)) res = FAILURE;
    if (Stack_peek(&stackA, 2) != Stack_peek(stackB, 2)) res = FAILURE;
    if (Stack_peek(&stackA, 3) != Stack_peek(stackB, 3)) res = FAILURE;
    if (Stack_peek(&stackA, 4) != Stack_peek(stackB, 4)) res = FAILURE;
    if (Stack_peek(&stackA, 5) != Stack_peek(stackB, 5)) res = FAILURE;
    if (Stack_peek(&stackA, 6) != Stack_peek(stackB, 6)) res = FAILURE;
    if (Stack_peek(&stackA, 7) != Stack_peek(stackB, 7)) res = FAILURE;
    
    // check that removing items from one stack doesn't affect the other
    Stack_pop(&stackA, 7, false); // emptied out the stack
    Stack_static_destroy(&stackA, false);
    
    if (*(int *)Stack_pop(stackB, 1, false) != seven) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != six) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != five) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != four) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != three) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != two) res = FAILURE;
    if (*(int *)Stack_pop(stackB, 1, false) != one) res = FAILURE;
    if (Stack_pop(stackB, 1, false) || Stack_count(stackB)) res = FAILURE;

    Stack_dynamic_destroy(&stackB, false);

    return res;
}

/*
   Check that a stack can be appended to another stack.
*/
enum status can_append_whole_stack(void){
    struct stack stackA;
    struct stack stackB;
    Stack_static_init(&stackA);
    Stack_static_init(&stackB);
    
    int one = 1;
    int two = 2;
    int three = 3;
    int four = 4;
    int five = 5;
    int six = 6;
    int seven = 7;
    
    int a = 33;
    int b = 34;
    int c = 35;
    int d = 777;

    enum status res = SUCCESS;
    
    // populate stack A
    Stack_push(&stackA, &one);
    Stack_push(&stackA, &two);
    Stack_push(&stackA, &three);
    Stack_push(&stackA, &four);
    Stack_push(&stackA, &five);
    Stack_push(&stackA, &six);
    Stack_push(&stackA, &seven);
    
    Stack_push(&stackB, &a);
    Stack_push(&stackB, &b);
    Stack_push(&stackB, &c);
    Stack_push(&stackB, &d);

    // append B to A
    Stack_append(&stackA, &stackB);

    // check that A's contents are A + B
    if (*(int *)Stack_pop(&stackA, 1, false) != d) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != c) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != b) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != a) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != seven) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != six) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != five) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != four) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != three) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != two) res = FAILURE;
    if (*(int *)Stack_pop(&stackA, 1, false) != one) res = FAILURE;
    
    Stack_static_destroy(&stackB, false);
    Stack_static_destroy(&stackA, false);

    return res;
}
