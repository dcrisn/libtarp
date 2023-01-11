#ifndef INTRUSIVE_PARADIGM__
#define INTRUSIVE_PARADIGM__
/*
 * ISO-compliant implementation of container_of.
 * Given a pointer to a singly linked list node object, 
 * this macro will return the memory address of the enclosing 
 * structure.
 *
 * Things to note are that:
 *  [ ] ptr is cast to a char * for the purposes of pointer arithmetic 
 *      (otherwise the offset would not be calculated accurately)
 *
 *  [ ] the resulting pointer (resulting from the offset calculation)
 *      is cast to a void * in order to silence the compiler warning
 *      about the alignment requirements increasing due to the cast.
 *      The alignment warning can be disregarded here: there would only 
 *      ever be a problem if we cast to a different type and NOT the 
 *      actual type that member is embedded in!
 *
 *  [ ] the void pointer is then cast back to a type *. If the enclosing
 *      object is not a type * in actuality, this would introduce a strict
 *      aliasing violation!
 *
 *  From the last two points it follows that it's fundamental that care 
 *  be taken that the TYPE argument to the macro indeed represents a 
 *  structure that has a `struct sllist` embedded within it by the name of 
 *  member.
 *
 * --> ptr
 *    A pointer to a singly linked list node object that's embedded within 
 *    some other user-defined structure.
 *
 * --> type
 *     The struct type of the structure that has a singly linked list node object
 *     (struct slnode) embedded within. For example, `struct mystruct`.
 *
 * --> member
 *     The name of the variable that identifies the singly linked list node object
 *     within the enclosing structure.
 *
 * <-- return
 *     The memory address of the enclosing structure that embeds the singly linked list
 *     node object.
 */
#define container_of(ptr, type, member) (type *)(void *) ( (char *)ptr - offsetof(type, member))

/*
 * Mnemonic for container_of() used when iterating over the linked list
 * to obtain for each node (or for a given node) its enclosing structure.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

#endif
