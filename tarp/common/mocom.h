#ifndef __MO_COMMON__
#define __MO_COMMON__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

// ======================
// Functions
// ======================
void *falloc(void *ptr, size_t size);
void _dbglog_(int line, const char *file, const char *func, char *fmt, ...);

/*
 * Return the minimum of a and b.
 * if a and b are equal, b is returned.
 */
static inline uint64_t min(uint64_t a, uint64_t b){
    return (a < b) ? a : b;
}

/*
 * Return the maximum of a and b.
 * if a and b are equal, b is returned.
 */
static inline uint64_t max(uint64_t a, uint64_t b){
    return (a < b) ? b : a;
}

// ======================
// Macros
// ======================
#define dump_msg__(stream, msg, ...) fprintf(stream, " ~~[%s(), %s:%d] <<" msg ">>\n", __func__, __FILE__, __LINE__,  ##__VA_ARGS__ )
#define SAY(fmt, ...)      dump_msg__(stdout, fmt, ##__VA_ARGS__)
#define COMPLAIN(fmt, ...) dump_msg__(stderr, fmt, ##__VA_ARGS__)

/*
 * Get length of array (note: NOT array decayed to pointer!)
 */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

/*
 * Stringify token x to allow concatenation with string literals */
#define tostring__(x) #x
#define TOSTRING(x) tostring__(x)

/*
 * Suppress warning about variable being declared but unused.
 *
 * --> x
 *     Variable to suppress the warning for; can be preceded by any
 *     number of qualifiers e.g. UNUSED(static const int myvar) = 13;
 *
 *  Casting to void will always work; using the unused attribute works
 *  with gcc and clang.
 */
#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#   define UNUSED(x) x __attribute__((unused))
#else
#   define UNUSED(x) do { (void)(x); } while (0)
#endif

/*
 * Print message to stderr and if FATAL_ASSUMPTIONS is defined,
 * exit with error.
 */
#define CRASH(cond, fmt, ...) do{ if (!(cond)) { COMPLAIN(fmt, ##__VA_ARGS__); exit(EXIT_FAILURE);} }while(0)
#ifdef FATAL_ASSUMPTIONS
#  define ASSUME(cond, fmt, ...) CRASH(cond, fmt, __VA_ARGS__)
#else
#  define ASSUME(cond, fmt, ...) do{ if (!(cond)) { COMPLAIN(fmt, ##__VA_ARGS__);} }while(0)
#endif


#endif
