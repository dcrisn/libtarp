#ifndef TARP_CONTAINER_H
#define TARP_CONTAINER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "container_impl.h"

/*
 * Given a non-NULL pointer to an embedded structure, get a pointer to
 * its enclosing container.
 *
 * The container must be of the specified type and contain an
 * embedded structure as a member field with the specified name.
 *
 * A correctly-cast pointer to the container is returned.
 *
 *
 * EXAMPLE
 *
 * struct mystruct{
 *    struct staqnode link;
 *    int myint;
 * };
 *
 * void testf(struct staqnode *node){
 *    assert(node);
 *    struct mystruct *ms = container(node, struct mystruct, link);
 *    printf("myint is %d\n", ms->myint);
 * }
 *
 * struct mystruct *ms = calloc(1, sizeof(struct mystruct));
 * ms->myint = 1;
 * testf(&ms->link); // 1
 */
#define container(embedded_structure_ptr, container_type, field) \
    get_container(embedded_structure_ptr, container_type, field)


#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif


