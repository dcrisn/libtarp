#ifndef __MO_COMMON__
#define __MO_COMMON__

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

void _dbglog_(int line, const char *file, const char *func, char *fmt, ...);

// ======================
// Macros
// ======================


/*
 * Get length of array (note: NOT array decayed to pointer!) 
 */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define membersz(type, member) sizeof(((type *)0)->member)

/*
 * Stringify token x to allow concatenation with string literals */
#define tostring__(x) #x
#define tkn2str(x) tostring__(x)

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
#define UNUSED(x) do { (void)(x); } while (0)

/*
 * Safe allocation: exits the program if calloc/malloc/realloc return NULL;
 *
 * Calls calloc (size>0, ptr=NULL), realloc (size>0, ptr!=NUL), or free (size=0,
 * ptr != NULL).
 *  - Calloc is called instead of malloc to ensure the data is zeroed.
 *
 */
void *salloc(size_t size, void *ptr);

#define nl(n) do { for (int i = n; i > 0; --i) puts(""); } while(0);

#define match(a, b) (strcmp(a, b) == 0 )
#define bool2str(val) ((val) ? "True":"False")
#endif
