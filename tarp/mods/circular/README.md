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
