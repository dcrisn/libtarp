#ifndef TARP_CONTAINER_H
#define TARP_CONTAINER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// To intrude or not to intrude?
// -----------------------------
//
// Most data structures in this library are 'intrusive'. That is, instead
// of the data structure containing a pointer (typically void pointer) to
// the user data (*payload*), the user data structure embeds the respective
// data structure as a struct member field. The pros and cons of this approach
// are discussed to death but here's a simplified summary.
// PROS:
// - the user manages the allocation of the containing structure. The data
//   structure api does not normally have to manage any memory since no
//   allocation is done by the API. The userdata is allocated by the user and
//   the data structure is embedded within it inside the same memory block.
// - cache locality. When the data structure *points* to the userdata, this
//   can be in a totally different place in RAM. Possibly/likely a cache miss,
//   possibly/likely slower. But if the data structure is embedded, as discussed,
//   it is in the same block of memory and there is no search to be done -- it
//   comes down to just calculating an offset.
// - the data structure may not necessarily need to point to the userdata (more
//   on this later). If it does not, it's one less pointer per node. The
//   advantage of this is self-evident particularly if the linked data structure
//   is meant to have many nodes.
// - a user structure can be linked into n different data structures at the same
//   time. Since this one structure contains the handles for all the data
//   structures it has been linked into, removing it or otherwise operating on
//   it may be done more quickly than it would otherwise be possible or even
//   be done in constant time (depending on the data structure).
// CONS:
// - arguably the API is less intuitive.
// - encapsulation is more or less out the window. Higher reliance on macros.
// - a void pointer based api would just return the data structure payload as
//   a void pointer. What does the intrusive data structure api return? How
//   is the user data obtained back from a data structure node? The user has
//   to do more and as already mentioned encapsulation is somewhat out of the
//   question.
// - instead of passing the user data to the api functions, a pointer to the
//   embedded structure needs to be passed. Looks weird.
//
//
// container_of magic
// ---------------------
//
// NOTE:
// container/containing structure/enclosing structure = a user-defined struct
// type that embeds an intrusive data structure 'node' as a member field.
// e.g.
// struct mystruct{             /* <--- container */
//    ........                  /* arbitrary user data */
//    struct xheapnode xhn;     /* <--- embedded data structure node */
//    .........                 /* arbitrary user data */
// };
//
// Assuming we've opted for the intrusive approach, how does one get back the
// user data i.e. the payload from a data structure 'node' e.g.
// a linked list node?
//
// There are 2 possibilities:
// 1. the data structure node keeps a pointer to its container (i.e. to its
//    containing user-defined struct). This would seem not much different from
//    the usual aforementioned void pointer approach (that we opted against) so
//    what gives? Well, the data structure is still intrusive and so the cache
//    locality benefit should still be there. The pointer does not point to
//    some random location in memory but just a few bytes from where the data
//    structure node is.
//    Also, instead of taking and returning void pointers all over the place,
//    macros can be written to simplify the API significantly. For example,
//    they can take as argument the container type and do the casting implicitly
//    when called.
//
// PROS:
// - no need for questionble-looking macro artifice. No 'magic'.
// - works just fine with C++ through judicious casting of the '.container'
//   pointer stored inside the embedded struct to the type of the containing
//   structure
// - more portable: not C-specific in that the same could be done in other,
//   possibly higher-level, languages.
// - Since the api has access to the void pointer to the containing
//   structure, a simpler macro-based api can be provided such that the
//   user does not need to know or care about the '.container' pointer.
//   The macros will take user-data arguments and 'return' user data results
//   from the actual functions by translating between user data pointers
//   and embedded api struct pointers.
// - since the api has access to the void pointer to the containing structure,
//   comparator functions (e.g. bst_comparator), or destructor callbacks
//   can take a void pointer to the enclosing structure (to the userdata,
//   that is), instead of a pointer to the the intrusive structure (which they
//   would have to call container_of on).
// - most of the macros need one less argument: the field name (see below) does
//   not need to be passed to get back a pointer to the enclosing structure
//   from a pointer to an enclosed structure. Only the embedded node pointer
//   and the container type is necessary.
// - destructors are not even needed for basic things. Since the api has a
//   pointer to the containing structure, free() can be called automatically
//   if requested (e.g. if a free_container argument is passed as True).
//
// CONS:
// - 8 bytes of overhead (assuming the size of a pointer is 8 bytes). You want
//   your data to be linked into 4 different structures? That's 8*4=32 bytes
//   of overhead per user data element.
// - this relies on the enclosed structure having a ".container" void pointer
//   field that is set to point back to the enclosing structure. Care must be
//   taken to correctly set this pointer when linking an item into the
//   respective intrusive data structure. There's always a risk of incorrectly
//   setting it or simply forgetting to set it and causing a NULL or
//   otherwise garbage pointer dereference.
//
// 2. Strictly speaking the pointer to the enclosing structure is unnecessary
//    overhead. Given the name of the containing structure and the name of
//    the member field inside it that identifies the embedded structure, the
//    offsetof() macro combined with judicious casting can be used to get back
//    a pointer to the containing structure. This ends up looking somewhat
//    quetionable and undefined behavior -inducing. However, *in C* this is
//    completely safe as long as the pointer fed to the macro is not garbage
//    and is actually correct. In fact, this approach is extremely common
//    in kernel code -- Linux, FreeBSD (from which the macros below are
//    ultimately taken) etc rely heavily on it for their data structures.
//
//    See for example:
//    - Linux:
//    https://github.com/torvalds/linux/blob/master/include/linux/list.h#L600
//    - FreeBSD:
//    https://github.com/freebsd/freebsd-src/blob/62c332ce9c9cf015eabb0a4aa0c83d4e96652820/sys/sys/queue.h#L316
//
//    There is some debate as whether this sort of macro is pedantically
//    standard C. However, it's unlikely to be problematic if used with care,
//    given its widespread use as mentioned above.
//
// PROS:
// - No extra pointer needed. This is huge. Given a user structure that
//   is linked into n different pointer-based structures this removes
//   8*n (assuming 8-byte pointers) bytes of overhead - per linked user structure.
//   This is ultimately the reason this library chose this approach instead of
//   (1).
//
// CONS:
// - very C specific in its design. You can't really do this sort of thing
//   in other languages since they don't have offsetof etc. The code therefore
//   becomes a little harder to translate.
// - will *not* work with non-POD c++ objects as it is UB to use offsetof()
//   on them. This is not as bad as it sounds -- a simple POD wrapper around
//   a non-POD object could be used.
// - user is responsible for their own structures in terms of freeing them etc.
// - encapsulation is reduced. Api functions are compelled to return pointers
//   to the embedded structure and the user has to call container() on them
//   to get a pointer to the containing structure.
// - api becomes slightly more cumbersome to use. Macros need an extra argument
//   (the name of the embedded struct member field inside the container)
//   almost everywhere. Internal functions cannot ever get a pointer to the
//   userdata e.g. to free it etc without delegating to the user (e.g. calling
//   a destructor callback that uses container_of() and then calls free()).
//

/* to support the DEQUALIFY freebsd macro as is */
#ifndef __uintptr_t
#define __uintptr_t uintptr_t
#endif

/* This, as well as the container_of definitions are taken (very slightly
 * adapted) from FreeBsd. See sys/sys/cdefs FMI. */
#define	__DEQUALIFY(type, var)	((type)(__uintptr_t)(const volatile void *)(var))

/*
 * Given the pointer x to the member m of the struct s, return
 * a pointer to the containing structure. When using GCC[1], we first
 * assign pointer x to a local variable, to check that its type is
 * compatible with member m.
 *
 * [1] Currently compiled out; statement expressions are a GNU extension
 * and in the current compilation mode non-standard compiler-vendor
 * extensions are turned off.
 */
//#ifdef __GNUC__
#if 0
#define	container_of(x, s, m) ({					\
	const volatile typeof(((s *)0)->m) *__x = (x);		\
	__DEQUALIFY(s *, ((const volatile char *)__x) - offsetof(s, m));\
})
#else  /* ! __GNUC__ */
#define	container_of(x, s, m)						\
	__DEQUALIFY(s *, (const volatile char *)(x) - offsetof(s, m))
#endif /* ifdef __GNU_C */

#define container(node, container_type, field) \
    container_of(node, container_type, field)



#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif /* TARP_CONTAINER_H */

