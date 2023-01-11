#include <math.h>  /* pow(), round() */
#include <float.h> /* FLT_EPSILON) */

#include "floats.h"

/*
 * Truncate off the decimal part -- equivalent to trunc() in math.h .*/
long get_integral_part(double n){
    return (long) n;
}

double get_decimal_part(double n){
    return n - get_integral_part(n);
}

double truncateto(double n, int dp){
    long scaling_factor = pow(10, dp);
    long truncated = (long) (n * scaling_factor);
    return 
        ((double) truncated) / scaling_factor;
}

double roundto(double n, int precision){
    long scaling_factor = pow(10, precision);
    return round(n * scaling_factor) / scaling_factor;
}

int dbcmp(double a, double b, double epsilon){
       if(fabs(a - b) < (epsilon > 0 ? epsilon : FLT_EPSILON) ){
          return 0;
    }
    return a > b ? 1 : -1;
}

