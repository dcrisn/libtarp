#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <sllist.h>
#include "cohort.h"

struct testnode{
    char *name;
    struct slnode node;
    size_t age;
};

/*
 * Test if the number of items is correctly maintained and 
 * reported through deletions and insertions. 
 */
enum status keeps_count(void){
    struct sllist mylist = SLL_STATIC_INITIALIZER;
    if (SLL_count(&mylist)) return FAILURE;

    struct testnode one;
    struct testnode two;
    struct testnode three;
    
    SLL_append(&mylist, &(one.node));
    SLL_append(&mylist, &(two.node));
    SLL_append(&mylist, &(three.node));
	
    if (SLL_count(&mylist) != 3) return FAILURE;

    SLL_poph(&mylist);
    SLL_popt(&mylist);

    if (SLL_count(&mylist) != 1) return FAILURE;

    for (unsigned long i = 0; i < 540000; ++i){
        SLL_prepend(&mylist, &(three.node));
    }
    for (unsigned long i = 0; i < 730000; ++i){
        SLL_append(&mylist, &(two.node));
    }

    if (SLL_count(&mylist) != 1270001) return FAILURE;


    return SUCCESS;
}

/*
 * Test if items can be inserted and removed 
 * from both the head and tail end correctly.
 */
enum status can_insert_and_delete_items(void){
    struct sllist mylist = SLL_STATIC_INITIALIZER;
    
    struct testnode *node1 = calloc(1, sizeof(struct testnode));
    struct testnode *node2 = calloc(1, sizeof(struct testnode));
    struct testnode *node3 = calloc(1, sizeof(struct testnode));
    struct testnode *node4 = calloc(1, sizeof(struct testnode));
    struct testnode *node5 = calloc(1, sizeof(struct testnode));
    struct testnode *node6 = calloc(1, sizeof(struct testnode));
    struct testnode *node7 = calloc(1, sizeof(struct testnode));
    struct testnode *node8 = calloc(1, sizeof(struct testnode));
    struct testnode *node9 = calloc(1, sizeof(struct testnode));
    struct testnode *node10 = calloc(1, sizeof(struct testnode));
    struct testnode *node11 = calloc(1, sizeof(struct testnode));
    struct testnode *node12 = calloc(1, sizeof(struct testnode));
    struct testnode *node13 = calloc(1, sizeof(struct testnode));
    
    node1->name = "one";
    node2->name = "two";
    node3->name = "three";
    node4->name = "four";
    node5->name = "five";
    node6->name = "six";
    node7->name = "seven";
    node8->name = "eight";
    node9->name = "nine";
    node10->name = "ten";
    node11->name = "eleven";
    node12->name = "twelve";
    node13->name = "thirteen";

    SLL_prepend(&mylist, &node1->node);
    SLL_prepend(&mylist, &node2->node);
    SLL_append(&mylist, &node3->node);
    SLL_prepend(&mylist, &node4->node);
    SLL_append(&mylist, &node5->node);
    SLL_append(&mylist, &node6->node);
    SLL_append(&mylist, &node7->node);
    SLL_append(&mylist, &node8->node);
    SLL_append(&mylist, &node9->node);
    SLL_append(&mylist, &node10->node);
    SLL_prepend(&mylist, &node11->node);
    SLL_prepend(&mylist, &node12->node);
    SLL_append(&mylist, &node13->node);
    
    struct testnode *i = NULL;
    struct slnode *node = NULL;

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "twelve") != 0) return FAILURE;
    free(i);

    node = SLL_popt(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "thirteen") != 0) return FAILURE;
    free(i);    

    node = SLL_popt(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "ten") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "eleven") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "four") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "two") != 0) return FAILURE;
    free(i);

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "one") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "three") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "five") != 0) return FAILURE;
    free(i);    

    node = SLL_popt(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "nine") != 0) return FAILURE;
    free(i);    

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "six") != 0) return FAILURE;
    free(i);

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "seven") != 0) return FAILURE;
    free(i);

    node = SLL_poph(&mylist);
    i = list_entry(node, struct testnode, node);
    if (strcmp(i->name, "eight") != 0) return FAILURE;
    free(i);
    
    node = SLL_poph(&mylist);
    if (node) return FAILURE;

    return SUCCESS;
}

/* 
 * Test whether the list can be iterated over and each item 
 * destroyed in a relatively easy manner.
 * To destroy an item means to free() the container (assuming
 * it was dynamically allocated, else this function should _not_ 
 * be called) that has a struct slnode embedded within it.
 */
enum status can_destroy_list(void){
    struct sllist mylist = SLL_STATIC_INITIALIZER;
    
    struct testnode *i = NULL;

    int32_t count = 7300500;
    for (int32_t n = 0; n < count; ++n){
        i = calloc(1, sizeof(struct testnode));
        SLL_prepend(&mylist, &i->node);
    }

    if (SLL_count(&mylist) != count) return FAILURE;

    struct testnode *node_ptr = NULL;
    struct slnode *slnode_ptr = NULL;
    SLL_destroy(node_ptr, slnode_ptr, &mylist, struct testnode, node);
    
    return SUCCESS;
}
