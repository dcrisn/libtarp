#ifndef MO_FLOAT_H
#define MO_FLOAT_H

/*
 * Includes:
 *  - math.h
 *
 *
 *  Must link with:
 *  - lm
 */

/* 
 * Return the integral part of n and discard the decimal part */
long get_integral_part(double n);

/*
 * Return the decimal part of n and discard the integral part */
double get_decimal_part(double n);

/*
 * Discard all digits > DP decimal places without any rounding.
 * */
double truncateto(double n, int dp);

/*
 * Round n to PRECISION decimal places and return the result */
double roundto(double n, int precision);

/*
 * Compare two floating point number; the result returned
 * is semantically equivalent to strcmp() for the two operands.
 * If epsilon < 0 (e.g. -1), FLT_EPSILON is defaulted to.
 */
int dbcmp(double a, double b, double epsilon);


#endif
