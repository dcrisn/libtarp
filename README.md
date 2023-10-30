# libtarp
Miscellaneous utility functions, macros, and data structures in C and C++

## Building

**make**:
```
cmake -B build -DCMAKE_INSTALL_PREFIX=<SOME LOCAL INSTALLATION DIRECTORY>
make -C build install
```

**ninja**:
```
cmake -B build -G Ninja -DCMAKE_INSTALL_PREFIX=<SOME LOCALL INSTALATION DIRECTORY>
ninja -C build install
```

## Tests

The tests do not get compiled by default. To enable them, pass the `RUN_TESTS`
flag when asking cmake to generate the build setup. For example:
```
cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local -DRUN_TESTS=1
```

Build and run all tests:
```
make -C build tests
```
or
```
ninja -C build tests
```
depending on which build system you use.

Individual tests can be run as well. To see the available test build targets
that can be invoked, type `make -C build test.` and use
`tab`-autocompletion to get a list of suggestions.

## Examples

Examples can be found in `examples/`. These, like the tests, are compilable
as well but must also similarly be enabled.

To enable the compilation of examples, specify the `BUILD_EXAMPLES` flag
to cmake when generating the build setup.

Examples should all be built automatically and can otherwise be built
individually. Each example source file in `examples/` has an associated target.
E.g. for `examples/myexample`, you can build it with `make -C build myexample`.

---------------------------------------------------------------

## General notes on the C Data Structures API

### containers and links

NOTE:
I'm using 'user struct{ure}' below and 'data structure' to distinguish between:
 - a user-defined struct that typically contains various data of interest to the
   user. The user struct contains one or more intrusive links needed for the
   library data structures. Because of this the user structure is also referred
   to as the 'container' below.
 - a data structure in this library that the user-defined struct can be linked
   into.

Most data structures in this library are 'intrusive'. Generally they involve:
 - a head structure (i.e. handle) that can either be declared and initialized
   on the stack, or be allocated and initialized dynamically on the heap (and
   in this latter case they must be `destroy`ed as well).
 - `node` structures that are meant to be embedded inside a user-defined
   structure. That is, the user struct must contain a member struct
   of the type of the node, under some arbitrary name. Therefore the user
   struct is a `container` that embeds a link that allows it to be linked
   into a corresponding data structure. NOTE a user structure can contain
   many such links and each one can be used to link it into different data
   structures.
 - various functions (e.g. node destructors discussed down below) will be
   expected to take as argument(s) a pointer to the member field inside the
   container that was used to link the user structure into the data structure.
   The functions can and must convert this pointer to a pointer to the container
   itself. This is done via the `container` macro. See `tarp/container.h` for
   notes on how to use it.

An example can be seen [here](examples/heap_dll_example.c).

### Macro vs Function API

The core of (almost) all the data structure implementations consist of
functions. However, the functions operate on (take, return) pointers to the
'link' struct type associated with the data structure in question. The
unfortunate consequence is that usability suffers a little (but this is C
after all). To make things simpler, macro wrappers are provided for each
data structure as a matter of course. They convert between a pointer to the
data structure link and a pointer to the container (which is what the user
cares about). Where possible, it's recommended to use the macros rather than
the functions.

### statically and dynamically initialized head structs

Many data structures allow for their head struct to be declared statically
and then be statically initialized or be declared and initialized dynamically
(see `STAQ_INITIALIZER` and `Staq_new` for example in `tarp/staq.h`).
The difference in terms of the API is that a statically initialized head
struct must only be `cleared` (e.g. via `Staq_clear`) but not destroyed.
OTOH dynamically initialized head structs *must* be `destroy`ed to avoid memory
leakage (even if `clear`ed beforehand).
 * `clear` = reset the internal state kept in the data structure head and
  remove all nodes (e.g. all `dlnode`s or `avlnode`s). The nodes are only
  removed but not deallocated if the standard `free_containers` argument
  is `false`. However, if `free_containers=true`, then the node destructor
  (which as discussed below must in this case have been specified at
  initialization time) is called on each node as it gets unliked from the
  data structure.
 * `destroy` = `clear` and then also deallocate the dynamically-allocated
    head struct (i.e. handle) itself.

NOTE
Given a head struct handle, each dynamic initialization of it must be followed
by a corresponding `destroy` call, otherwise memory will be leaked.

NOTE
Given a head struct handle, you can initialize it once, `clear` it as many
times as needed, and destroy it once at the end.

The following should explain the expected dynamic initialization and
destruction call sequence.
```c
// correct
handle = dynamic_init();
....
destroy(handle);
...
handle = dynamic_init();
...
destroy(handle);


// correct
handle = dynamic_init();
....   // add new nodes etc
clear(handle);
....  // add new nodes etc
clear(handle);
...
clear(handle);
...
destroy(handle);


// incorrect
handle = dynamic_init();
.... // add new nodes etc
clear(handle);
handle = dynamic_init();     /* memory leak; handle not destroyed */
destroy(handle);
```

As mentioned, `destroy` must never be called on statically-initialized handles.
They can only (and **must**) be `clear`ed:
```c
handle;   // handle declared on the stack (i.e. not allocated on the heap)

// incorrect
... // add nodes etc
destroy(handle)

// correct
.... // add nodes etc
clear(handle)
.... // add nodes etc
clear(handle)
```

### node destructors

Each intrusive data structure e.g. `staq`, `dllist`, `avltree`, `hasht` takes
a destructor (prototype declared in the respective header file) at
initialization time. This may only be NULL if no function is ever called that
would need to destruct (e.g. call `free()`) a node.

In particular, many `clear` or `destroy` functions (e.g. `Staq_clear`,
`Dll_destroy`, `Avl_clear`, `Hasht_destroy` etc) take a `free_containers`
boolean argument. If `free_containers=true`, the destructor **must** have been
specified at initialization time. If it's NULL the program will exit with
error.

Other functions take a `free_container` argument as well e.g. `Staq_remove`.
The same logic applies there.

Where other functions need the destructor to have been set, this is explicitly
indicated in a comment above the respective function in the appropriate header
file.


#### example node destructor

As explained in the `containers and links` section, given a pointer to a
data structure node embedded inside a container, a pointer to the container can
be obtained by using the `container` macro. This is the case with node
destructors: a pointer to the container must be obtained so that it can be
deallocated.
Consider the following example of a user data structure and its associated
destructor.
```c
struct mystruct {
    const char *mydata;
    struct dlnode link;
};

void destructor(struct dlnode *node){
    assert(node);
    struct mystruct *s = container(node, struct mystruct, link);
    free(s);   /* of course, s must have been dynamically allocated here */
}
```
If the destructor fails to correctly deallocate the resources associated
with a node's container, then this will result in memory leakage.

### comparators

Certain data structures e.g. `avltree` rely on the comparison of keys for
various operations e.g. looking up a node in a binary search tree. For these,
a `comparator` callback **must** be specified at initialization time.
The comparator prototype is declared in the respective header file of the
data structure in question.
As is the case with node destructors, a comparator must use the `container`
macro to get pointers to the containers of the two nodes specified as arguments.

More information on the general idea of a `comparator` can be found in
`tarp/common.h` (serach for `comparator`). An example of a comparator for an
avl tree is shown below.
```c
struct testnode {
    size_t num;   /* key (and data)*/
    struct avlnode link;
};

enum comparatorResult compf(const struct avlnode *a_, const struct avlnode *b_)
{
    const struct testnode *a = container(a_, struct testnode, link);
    const struct testnode *b = container(b_, struct testnode, link);
    assert(a);
    assert(b);

    //debug("comparing a=%zu b=%zu", a->num, b->num);
    if (a->num < b->num) return LT;         // -1
    if (a->num > b->num) return GT;         // 1
    return EQ;                              // 0
}
```
