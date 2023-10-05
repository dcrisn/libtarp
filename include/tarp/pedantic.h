#ifndef TARP_PEDANTIC_H
#define TARP_PEDANTIC_H

/*
 * This header makes available certain definitions that would not be available
 * when compiled pedantically for a certain C standard with no compiler-specific
 * extensions (e.g. the GNU dialect).
 */

#ifdef __cplusplus
extern "C" {
#endif

#define VERSION_CONSTANT_C23 202311L
#define VERSION_CONSTANT_C11 201112L

/*
 * typeof() is a GNU keyword extension and only made part of the C standard with
 * C23. When compiled pedantically, with warnings as errors, for a particular
 * C standard < 23, typeof is therefore unavailable. However, __typeof__ is
 * defined even then by both gcc and clang.
 * See `man gcc`, and https://clang.llvm.org/docs/UsersManual.html FMI.
 */
#if __STDC_VERSION__ < VERSION_CONSTANT_C23
#   define typeof(x) __typeof__(x)
#endif


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
