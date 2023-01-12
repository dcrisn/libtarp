#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <mocom.h>
#include <tarp/stack.h>


/* ********************************************************** */
/*                  Implementation Notes

    Stacks and queues are essentially the inverse of one another 
    in that the former always removes the oldest item  added, 
    while the latter removes the latest item. Both offer linear
    (sequential) access to the data, just like a linked list.
   
    It's worth noting that stacks and queues are not 
    data structures, but abstract data types. Conceptually, 
    stacks and queues are to be thought of as an interface rather 
    than in implementation terms. That is to say, both of them can 
    easily be implemented using either arrays or linked lists 
    at the implementation level. 
    Arrays arguably lend themselves better to stacks and queues that 
    hold a fixed number of items (since arrays are fixed in size, 
    unless they're dynamic arrays) whereas linked lists are 
    best used when the number of items is indefinite. 

    Whatever the underlying data structure, stacks and queues are 
    defined by the interface they provide, which is in terms of 
    the data flow: first-in-first-first-out for queues and 
    last-in-first-out for stacks.

    This specific implementation implements a stack ADT via a 
    singly-linked list.


    --------------Overflow and underflow--------------------

    This implementation of a stack doesn't have a fixed size, i.e. 
    there's no stack overflow scenario that's accounted for.
    This could easily be incorporated, however. For example, 
    by having the Stack_push() function check the count member 
    and stop pushing new items when its value has reached a defined limit.

    Stack 'underflow', is, on the other hand, accounted for, as Stack_pop() 
    will return a NULL pointer when called on an empty stack (or when an item
    that's out of bounds is specified to be popped).

*/
  
///|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||

/* 
   Initialize a Stack object. 

       Takes a Stack reference pointer and calls malloc to allocate
       heap space for a Stack struct. If successful, 
       the inner members are initialized to zero
       or NULL, as appropriate.

    The user must call Stack_dynamic_destroy() on this stack when no
    longer needed.
*/
void Stack_dynamic_init(struct stack **stack_ref){
    struct stack *temp = falloc(NULL, sizeof(struct stack));

    if (!temp){
        *stack_ref = NULL;
        return;
    };

    temp->count = 0;
    temp->top = NULL;
    temp->bottom = NULL;
    *stack_ref = temp;  
}

/* 
   Initialize a Stack object STATICALLY.
   It does NOT allocate space for the stack as it's 
   assumed it has automatic storage. Instead, ir merely
   initializes its members to default values.

   The user must call Stack_static_destroy() on this stack
   when no longer needed.
*/
void Stack_static_init(struct stack *stack_ptr){
    assert(stack_ptr);

    stack_ptr->count = 0;
    stack_ptr->top = NULL;
    stack_ptr->bottom = NULL;
}

/*
   Return the number of items currently stacked.
*/
ssize_t Stack_count(const struct stack *stack){
    return stack->count;
}

/* 
   Call free on *stack_ref, and set it to NULL. 
   Free() is called on each item in the stack, but NOT each item's 
   .data member UNLESS free_data is true.
   The stack items' .data member must have been dynamically
   allocated when free_data is true (else calling free() on it would 
   be illegal!)

   To be called on Stacks initialized with Stack_dynamic_init()
   - but NOT ones initialized with Stack_static_init().
*/
void Stack_dynamic_destroy(struct stack **stack_ref, bool free_data){
    assert(stack_ref != NULL);
   // nothing to free, there's no stack (stack object is NULL)
    if (!(*stack_ref)){
        return;
    }
    // there are still items on the stack -- free all of them
    if ((*stack_ref)->top){
        struct stackitem *temp = (*stack_ref)->top;
        struct stackitem *current;
        while (temp){
            current = temp;
            temp = temp->prev;
            if (free_data){
                falloc(current->data, 0);
            }
            falloc(current, 0);
        };
    }
    // else there's a stack, but it's empty (Stack object but no struct stackitem *'s). 
    // Deallocate that
    falloc(*stack_ref, 0);
    *stack_ref = NULL;
}

/* 
   Call free() on each item on the stack. Each item's .data
   member is NOT free()d UNLESS free_data is true.

   To be called on Stacks initialized with Stack_static_init() - 
   but NOT ones initialized with Stack_dynamic_init().
*/
void Stack_static_destroy(struct stack *stack_ptr, bool free_data){
    assert(stack_ptr != NULL);

    // there are still items on the stack -- free all of them
    if (stack_ptr->top){
        struct stackitem *temp = stack_ptr->top;
        struct stackitem *current;
        while (temp){
            current = temp;
            temp = temp->prev;
            if (free_data){
                falloc(current->data, 0);
            }
            falloc(current, 0);
        };
    }
}

/*
   Create a struct stackitem * that can be passed to Stack_push() and
   take care of initializing it to the correct values.
   the_value is the actual value that gets wrapped in a struct stackitem *.
   This can be anything: a char array, a struct pointer etc.
   A struct stackitem * is simply a struct that has a void .data ptr
   member that can be used to point to anything.

   The user MUST know what the type of the object that .data points 
   to is, so that they can CAST IT BACK when popping it off the stack.
*/
static struct stackitem *Stack_make_item__(void *the_value){
    struct stackitem *newitem = falloc(NULL, sizeof(struct stackitem));
    if (newitem){
        newitem->data = the_value;
        newitem->prev = NULL;
    }
    return newitem;
}

/*
   This pushed onto the stack a struct stackitem * by making it the top
   the stack. A struct stackitem * type is created by calling 
   Stack_make_item().
*/
void Stack_push(struct stack *stack, void *item){
    assert(stack != NULL && item != NULL);
    struct stackitem *node = Stack_make_item__(item);

    node->prev = stack->top;
    stack->top = node;
    if(!stack->count){ stack->bottom = node; }
    stack->count++;
}

/*
 * Pop the top of the stack (the nth item from the bottom in an n-item stack)
 * when n is 1, or the nth-item from the top if n is > 1.
 *
 * --> stack
 *     Stack object, dynamically or statically allocated.
 * 
 * --> n
 *     - If n=1, return the top of the stack. NULL is returned if the stack
 *       is empty.
 *
 *     - If n>1, return the nth item from the top of the stack. Specifically,
 *       if n=3, top->prev->prev will be returned. 
 *       If n>Stack_count(), then NULL is returned as that is out of bounds.
 *       If n<=Stack_count(), then the nth item from the top is returned AND
 *       the items higher up on the stack get popped off as well (but are not
 *       returned).
 *
 * --> free_data
 *     Since if n>1 all items higher up on the stack get popped off as well but do
 *     not get returned, the internal structures (stackitem) get free()d.
 *     free_data specifies whether the actual data held in these structures (i.e.
 *     .data) must be called free() on as well. If free_data is false, free will NOT
 *     get called. This is highly relevant since if you specify free_data as false
 *     on data that should be freed, you will get memory leaks. Converesely, if you 
 *     try to free data that can't be free()d (i.e. string literals), the program
 *     will crash as that's an illegal action.
 *     The user must be cognizant of what data (static or dynamic) they push onto
 *     the stack and specify free_data in line with that.
 *
 * <-- res
 *     the .data field of the stackitem specified, or NULL if no such item exists.
 *     The user MUST know how to cast an interpret the result, as it's a void pointer!
 */
void *Stack_pop(struct stack *stack, uint32_t n, bool free_data){
    void *res = NULL;

    assert(stack != NULL && n > 0);
    if (!stack->top){
        return NULL;
    }

    struct stackitem *node = stack->top;
    struct stackitem *temp;
    while (--n && node){ // we need to do n-1 iterations
        temp = node; 
        node = node->prev;
        if (free_data){
            falloc(temp->data, 0);
        }
        falloc(temp, 0);
        stack->count--;
    }

    if (node){
        res = node->data;
        stack->top = node->prev;
        falloc(node, 0);
        stack->count--;
    }
    else{
        stack->top = NULL;
        stack->bottom = NULL;
    }

    return res;
}

/* Return the top of the stack or the nth item from the top of the stack, 
 * -- or NULL if no such item exists -- but without removing any item(s)
 * from the stack.
 */
void *Stack_peek(struct stack *stack, uint32_t n){
    void *res = NULL;

    assert(stack != NULL && n > 0);
    if (!stack->top){
        return NULL;
    }

    struct stackitem *node = stack->top;
    while (--n && node){ // we need to do n-1 iterations
        node = node->prev;
    }

    if (node){
        res = node->data;
    }

    return res;

}

/*
 * Given a stack of items n,n+1,n+2, ... n+x, (bottom to top)
 * reverse the items such that the items, bottom to top, will
 * be n+x .. n+2, n+1, n.
 */
void Stack_upend(struct stack *stack){
    assert(stack);
    
    stack->bottom = stack->top;
    struct stackitem *dummy = NULL;
    struct stackitem *item = stack->top;
    struct stackitem *next;

    while (item){
        next = item->prev;
        item->prev = dummy;
        dummy = item;
        item = next;
    }
    stack->top = dummy;
}

/*
 * Return a dynamically-allocated copy of stack.
 * The user must then call Stack_dynamic_destroy() on this when
 * no longer needed.
 * NOTE that what gets copied is the internal structures of STACK,
 * but NOT their .data field. That is to say this allows you to have
 * two independent stacks be able to pop ITEM off stackA and still have
 * it on stackB. However, if you FREE the item (free_data=true) when 
 * popping it off stackA, then the data is freed in StackB as well, with
 * the result that StackB now has a dangling pointer and accessing this
 * memory would be ILLEGAL. Special care must be taken when manipulating
 * two or more copies of the same stack that the data is only free()d ONCE
 * and never accesses or freed from any other stacks after that!
 *
 * <-- res
 *     A stack object that's a copy of STACK and holds the same data as it.
 */
struct stack *Stack_copy(struct stack *stack){
    assert(stack);
    struct stack *copy = falloc(NULL, sizeof(struct stack));
    assert(copy);
    
    struct stackitem *current = stack->top;
    while (current){
        Stack_push(copy, current->data);
        current = current->prev;
    }
    
    Stack_upend(copy);
    return copy;
}

/* 
 * Append the contents of stack B to stack A. 
 * Stack B is NOT destroyed after the append, but it's empties of its
 * items.
 * Appending an empty stack to another (empty or non-empty)
 * stack has no effect.
 *
 * Maitaining both a .bottom and .top pointers allow
 * for O(1) whole-stack appends, rather than O(n).
 * It's a matter of manipulating 2 or 3 pointers.
 */
void Stack_append(struct stack *A, struct stack *B){
    assert(A && B);
    if (!B->count){ return; }
    
    struct stackitem *bottom = B->bottom;
    bottom->prev = A->top;
    A->top = B->top;
    A->count+=B->count;

    // reset B 
    B->count = 0;
    B->top= NULL; 
    B->bottom = NULL;
}

/*
 * Pretty-print the contents of STACK.
 * This is meant as a debugging aid.
 * Since each stack items is a wrapper around a void pointer, 
 * the user must supply a function that knows how to cast and
 * interpret this pointer, and print it out. This is trickier 
 * than it might seem as if the stack contains data of different
 * types (char, int, char *), the user-supplied print function
 * will need to know how to figure this one out. With this in mind,
 * by far the easiest scenario is when the stack contains items all
 * of the same type e.g. strings and so the print function will 
 * have a much easier job, knowing what to expect.
 *
 * Some default functions are provided by default: 
 * - strprint : casts ITEM to string.
 * - intprint: casts ITEM to int.
 */
void Stack_print(struct stack *stack, char *stackname, void (*printer)(void *item, uint32_t level)){
    assert(stack && printer);
    struct stackitem *current = stack->top;
    uint32_t level = Stack_count(stack);
    
    printf("* * * Contents of stack \'%s\' [%li] * * *\n", stackname, Stack_count(stack));
    while(current){
        printer(current->data, level);
        level--;
        current = current->prev;
    }
}

// ----------- Printer Functions -------------
void Stack_strprint(void *item, uint32_t level){
    printf("--> [%u] %s\n", level, (char *)item);
}

void Stack_intprint(void *item, uint32_t level){
    printf("--> [%u] %i\n", level, *(int *)item);
}

void Stack_ptrprint(void *item, uint32_t level){
    printf("--> [%u] %p\n", level, item);
}

