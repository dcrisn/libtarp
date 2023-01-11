#ifndef MODS_STACK_H
#define MODS_STACK_H

#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* Implementation of a stack ADT through a singly-linked list.
 * A stack organizes its data in a LIFO -- Last-in-first-out - manner, 
 * always removing ('popping', in stack terminology) 
 * the newest item (the item that was 'pushed' last onto the stack). 
*/

struct stackitem{
    void *data;   // a void ptr is used so that any type can be pointed to and thus pushed onto the stack;
    struct stackitem *prev;
};

struct stack{
    ssize_t count;     // number of items in the queue
    struct stackitem *top;
    struct stackitem *bottom;
};
 
typedef struct stack *Stack; 

/*
 * Initialize a stack object dynamically (allocate heap memory).
 */
void Stack_dynamic_init(struct stack **stack_ref);  

/*
 * Initialize a stack object statically - Stack has automatic storage
 * or is statically allocated.
 */
void Stack_static_init(struct stack *stack_ptr);  

/*
 * Tear down the stack object by freeing all the malloc-ated memory.
 *
 * If free_data is true, then this function calls free() on the void pointers
 * that had been passed to Stack_push(), else it calls free on just the internal
 * structures used to hold those pointers (struct stackitem) but not these structures' 
 * .data fields. This must be the case for example when the void pointers point
 * to memory that is illegal to call free() on (e.g. string literals in RO memory)
 * or to dynamically allocated types still being used by another application!
 * 
 * data_free must be false if the nodes' .data field is NOT to be free()d.
 */
void Stack_dynamic_destroy(struct stack **stack_ref, bool free_data);  

/*
 * Tear down the stack object by freeing the memory associated with its 
 * stack items, but not the stack itself as it's assumed to have been
 * statically allocated or have automatic storage duration.
 *
 * if free_data is true, then this function calls free() on the void pointers
 * that had been passed to Stack_push(), else it calls free on just the internal
 * structures used to hold those pointers (struct stackitem) but not these structures'
 * .data fields. This must be the case for example when the void pointers point
 * to memory that is illegal to call free() on (e.g. string literals in RO memory)
 * or to dynamically allocated types still being used by another application!
 * 
 * data_free must be false if the nodes' .data field is NOT to be free()d.
 */
void Stack_static_destroy(struct stack *stack_ptr, bool free_data);  

/*
 * Push item onto the stack.
 */
void Stack_push(struct stack *stack, void *item);

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
void *Stack_pop(struct stack *stack, uint32_t n, bool free_data);  

/* Return the top of the stack or the nth item from the top of the stack, 
 * -- or NULL if no such item exists -- but without removing any item(s)
 * from the stack.
 */
void *Stack_peek(struct stack *stack, uint32_t n);  

/*
 * Return the number of items currently on the stack.
 */
ssize_t Stack_count(const struct stack *stack); 

/*
 * Given a stack of items n,n+1,n+2, ... n+x, (bottom to top)
 * reverse the items such that the items, bottom to top, will
 * be n+x .. n+2, n+1, n.
 */
void Stack_upend(struct stack *stack);

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
struct stack *Stack_copy(struct stack *stack);

/* 
 * Append the contents of stack B to stack A. 
 * Stack B is NOT destroyed after the append.
 */
void Stack_append(struct stack *A, struct stack *B);

/*
 * Pretty-print of stack meant as a debugging aid. A callback is expected
 * that knows how to cast each item on the stack. Some default functions are
 * provided below, but they expect the stack to be homogenous in terms of
 * the type of items it stores. If the stack stores items of different types
 * or items of a type other than the one supported by the functions provided
 * below, the user must implement their own.
 */
void Stack_print(struct stack *stack, char *stackname, void (*printer)(void *item, uint32_t level));

// default stack print functions
void Stack_strprint(void *item, uint32_t level);
void Stack_intprint(void *item, uint32_t level);
void Stack_ptrprint(void *item, uint32_t level);

#endif
