Doubly Linked List ADT implementation
=======================================

Intrusive singly linked list: this is useful for when you just want to
iterate over a list of items linearly without needing to go backward.
It being an intrusive linked list it performs well due to cache
locality and it also lends itself well to polymophism so any data
structure can be put in a list as long as it embeds a `struct sllist`
in itself. This may save significant development time as one does not
need to spend time reinventing the wheel all the time. With that said,
the implementation is minimal and only includes a few generic
functions. For anything specific/tailored to the implementation, the
user will have to build on top of what's provided.


ToDos
--------------------
 - develop the foreach iterator macros and make them usable


NOTES
-------
 - dummy node would slightly simplify insertion and deletion as you don't have to check if
   a node is already inserted; however rotation is more cumbersome
 - the point of having a truly CIRCULAR list (tail point to head) instead of
   circularlity only at the abstract klevel (a next operation gives you the head
   instead of NULL when you reach the end) is that you can easily ROTATE the
   list, moving the head and tail and head arbitrarily. You can do it with the
   abstract approapch as well but then when you move the tail and head you have
   to 1) make hte former head point to the former tail and vice versa and 2)
   remove the current head from pointing to the current tail and vice versa.
   I.e. you would have to fight circularity  -- why fight it?
