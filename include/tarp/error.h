#ifndef TARP_ERROR_H
#define TARP_ERROR_H

#include <tarp/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * These are errors that are "thrown" and are uncatchable.
 * Of course, C does not have exceptions so these simply represent conditions
 * that cause the program to exit. Essentially, they are assertions that don't
 * disappear in non-debug code.
 *
 * EXAMPLE:
 * If memory allocation fails, the program should just exit.
 * By decorating the calls to the memory allocator we can ensure termination on
 * allocation failure and not have to check for 'NULL' all over the place.
 */
enum Exception{
    EXCEPTION_BAD_ALLOC=0,
    EXCEPTION_RUNTIME_ERROR, /* a fatal runtime error, that is */
    EXCEPTION_LAST__
};

/*
 * These are errors that are simply signalled by return of an error code
 * constant. Almost all errors are to be indicated this way in C. The only
 * exception are errors that are expected to terminate the program should they
 * occur -- and these should be enumerated in the Exception enum. */
enum ErrorCode{
    ERROR_CODE_FIRST__ = 0, /* success */
    ERROR_NOSPACE = 1,
    ERROR_OUTOFBOUNDS,
    ERROR_INVALIDVALUE, /* generic, encompasses ERROR_OUTOFBOUNDS */
    ERROR_CODE_LAST__
};

/*
 * return a statically allocated string describing the exception or error,
 * respectively, associated with CODE. */
const char *tarp_strexcept(unsigned int code);
const char *tarp_strerror(unsigned int code);


void throw__(
        enum Exception code,
        const char *file,
        const char *func,
        int line);


#define THROW(exception, cond) \
    do { if (cond) throw__(exception, __FILE__, __func__, __LINE__); }while(0)

#define ASSUME(cond, ...) do{ if (!cond) warn(__VA_ARGS__); } while(0)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
