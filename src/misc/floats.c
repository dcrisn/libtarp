#include <limits.h>
#include <math.h>  /* pow(), round() */
#include <float.h> /* FLT_EPSILON) */

#include <tarp/common.h>
#include <tarp/bits.h>
#include <tarp/floats.h>

/*
 * Ensure casting from a long to a double is always safe (even though
 * precision may be lost) */
static_assert(sizeof(long) <= sizeof(double), "sizeof(long)>sizeof(double)");

long db2long(double d, bool truncate, long fallback, bool *safe){
    assert(!isnan(d));
    assert(!isinf(d));

    d = truncate ? trunc(d) : round(d);

    /*
     * maximum value of long as a double */
    double limit = ldexp(1.0, bytes2bits(sizeof(long))-1);

    if (d >= -(limit) && d < limit){
        *safe = true;
        /* safe because the value being cast is within the range of
         * a signed long [-2^63, +2^63) */
        return (long)d;
    }

    *safe = false;
    return fallback;
}


/*
 * Truncate off the decimal part -- equivalent to trunc() in math.h.
 *
 * NOTE mofd may too slow if performance is critical, since it deals
 * with various cases. If you are sure the value is within bounds and
 * does not need to be range-checked, then just cast to an appropriate
 * integral type directly.
 * */
double get_integral_part(double d){
    double integral = 0.0;
    modf(d, &integral);
    return integral;
}

double get_decimal_part(double d){
    double integral;
    return modf(d, &integral);
}

double truncateto(double d, int numdp){
    double scaling_factor = pow(10, numdp);
    double integral, decimal;
    decimal = modf(d, &integral);

    return integral + (trunc(decimal * scaling_factor) / scaling_factor);
}

double roundto(double d, int numdp){
    double scaling_factor = pow(10, numdp);
    double integral, decimal;
    decimal = modf(d, &integral);

    return integral + (round(decimal * scaling_factor) / scaling_factor);
}

int dbcmp(double a, double b, double epsilon){
       if(fabs(a - b) < ((epsilon > 0) ? epsilon : FLT_EPSILON) ){
          return EQ;
    }
    return a > b ? GT : LT;
}

