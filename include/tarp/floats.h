#ifndef TARP_FLOATS_H
#define TARP_FLOATS_H

#include <math.h>

/*
 * Some helpers for working with floating point numbers.
 * Do be very aware of the following issues:
 */

/*
 * Perform a conversion from double to long - if safe.
 *
 * --> d
 * The double to convert. Must not be NaN or infinity.
 *
 * --> fallback
 * Value to return if the conversion is unsafe.
 *
 * --> truncate
 * If true, d is truncated (rounded *down*) to the nearest integer.
 * Otherwise if false, d is rounded to the nearest integer
 * (see man 3 round).
 *
 * --> safe
 * if not NULL, boolean true will be stored here if the conversion
 * was safe and successful, otherwise false if unsafe and unsuccessful.
 *
 * <-- return
 * The conversion result as a long if safe; otherwise fallback.
 */
long db2long(double d, bool truncate, long fallback, bool *safe);

/*
 * Return the integral/decimal part of d */
double get_integral_part(double d);
double get_decimal_part(double d);

/*
 * Keep only n decimal places. Truncate the rest.
 * Note the last digit before cutoff point is not rounded.
 * E.g. in: d=8.2318, out: truncate(d, 3)=8.231
 *
 * If d has fewer decimal places than n, then nothing is done.
 */
double truncateto(double d, int numdp);

/*
 * Round n to PRECISION decimal places and return the result */
double roundto(double d, int precision);

/*
 * Compare two floating point number; the result returned
 * is semantically equivalent to strcmp() for the two operands.
 * If epsilon < 0 (e.g. -1), FLT_EPSILON is defaulted to.
 */
int dbcmp(double a, double b, double epsilon);


#endif
