#include <math.h>          /* pow */
#include <errno.h>
#include <stdio.h>         /* perror */

#include <tarp/floats.h>
#include <tarp/timeutils.h>

/*
 * convert a timespec to a double, in seconds */
double timespec_to_double(struct timespec *time){
    return time->tv_sec + ( time->tv_nsec * pow(10, -9) );
}

/*
 * Break down a double in seconds into a timespec */
void double_to_timespec(struct timespec *timespec, double secs){
    time_t seconds = (time_t)secs;
    double nsecs   = round(get_decimal_part(seconds) * 1e9);
    timespec->tv_sec  = seconds;
    timespec->tv_nsec = (long)nsecs;
}

/*
 * Convert a timespec to a millisecond integral, rounded up to the
 * nearest integer -- see man page for `round()`
 */
uint64_t timespec_to_ms(struct timespec *time){
    return (uint64_t)( round(timespec_to_double(time) * 1e3) );
}

/*
 * a and b are compared first by their seconds; if equal the nanoseconds field
 * is used as the discriminant.
 * a == b IFF a(seconds, nanoseconds) == b(seconds, nanoseconds)
 */
int timespec_cmp(struct timespec *a, struct timespec *b){
    if (a->tv_sec > b->tv_sec){
        return 1;
    } else if (a->tv_sec < b->tv_sec){
        return 1;
    }
    /* else, use nanosecond discriminant */
    if (a->tv_nsec > b->tv_nsec){
        return 1;
    } else if (a->tv_nsec < b->tv_nsec){
        return -1;
    }
    /* complete equality */
    return 0;
}

/*
 * Add the values in timespec a and b and write them to c.
 * Note that c could be one of a or b, repeated.
 */
void timespec_add(struct timespec *a, struct timespec *b, struct timespec *c){
    time_t secs = a->tv_sec + b->tv_sec;
    uint64_t nsecs =  a->tv_nsec + b->tv_nsec;

    secs += nsecs / 1000000000; /* / 10^9 */
    nsecs = nsecs % 1000000000;
    c->tv_sec = secs;
    c->tv_nsec = nsecs;
}

/*
 * Subtract b from a and write the result to c.
 * Note that c could be one of a or b, repeated.
 */
void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *c){
    time_t secs = a->tv_sec - b->tv_sec;
    long nsecs = a->tv_nsec - b->tv_nsec;

    /* a > b, decrement secods and make ns positive if negative */
    if (secs > 0 && nsecs < 0){
        --secs;
        nsecs = 1000000000 /* 10^9 ns = 1s*/ - nsecs;
    } 
    /* a < b, increment secods and make ns negative if positive */
    else if (secs < 0 && nsecs > 0){
        ++secs;
        nsecs = nsecs - 1000000000 /* 10^9 ns = 1s*/;
    }

    c->tv_sec = secs;
    c->tv_nsec = nsecs;
}

/* 
 * Get a millisecond timestamp.
 *
 * CLOCK_REALTIME is used by default but it can be overriden 
 * via the clock parameter e.g. CLOCK_MONOTONIC etc.
 * To use the default, -1 should be specified as the argument.
 */
uint64_t msts(clockid_t clock){
    struct timespec now = {0};
    clock = (clock == -1) ? CLOCK_REALTIME : clock;
    clock_gettime(clock, &now);
    return timespec_to_ms(&now);
}

/*
 * Break a milliesecond timestamp e.g. as obtained with `timespec_to_ms()`
 * down to timespec values.
 */
void ms_to_timespec(uint64_t ms, struct timespec *timespec){
    timespec->tv_sec =  ms / 1000;
    timespec->tv_nsec = (ms % 1000) * 1000000; /* *10^6 */
}

/*
 * Determine whether the time indicated in the timespec a has
 * passed the current time as reported by CLOCK.
 *
 * If -1 is specified for the clock argument, CLOCK_REALTIME
 * is used by default */
bool time_expired(struct timespec *a, clockid_t clock){
    struct timespec now;
    clock = (clock == -1) ? CLOCK_REALTIME : clock;
    if (clock_gettime(clock, &now) == -1){
        perror("Failed to get time");
    }
    return (timespec_cmp(a, &now) >= 0);
}

