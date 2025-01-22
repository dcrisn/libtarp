#include <assert.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include <tarp/floats.h>
#include <tarp/log.h>
#include <tarp/math.h>
#include <tarp/timeutils.h>

// see man (2) clock_gettime
#define TV_NSEC_MAX 999999999LU

/*
 * Convert ts to a double in the following format:
 * <seconds * secs_multiplier>.<nanoseconds / nsecs_divisor>
 */
static double timespec2db(const struct timespec *ts,
                          unsigned secs_multiplier,
                          unsigned nsecs_divisor) {
    assert(ts);
    return (double)ts->tv_sec * secs_multiplier +
           (double)ts->tv_nsec / nsecs_divisor;
}

double timespec2dbs(const struct timespec *time) {
    assert(time);
    return timespec2db(time, 1, NSECS_PER_SEC);
}

double timespec2dbms(const struct timespec *time) {
    assert(time);
    return timespec2db(time, MSECS_PER_SEC, NSECS_PER_MSEC);
}

double timespec2dbus(const struct timespec *time) {
    assert(time);
    return timespec2db(time, USECS_PER_SEC, NSECS_PER_USEC);
}

/*
 * These functions should not fail given the way they are called;
 * there's no room for the arguments to be invalid etc. */
struct timespec time_now_monotonic(void) {
    struct timespec ts;
    memset(&ts, 0, sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

struct timespec time_now_epoch(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts;
}

struct timespec get_unix_timepoint(unsigned ms) {
    struct timespec now = time_now_epoch();

    struct timespec delay;
    delay.tv_sec = ms / MSECS_PER_SEC;

    // leftover milliseconds (< 1s)
    ms %= MSECS_PER_SEC;
    delay.tv_nsec = ms * NSECS_PER_MSEC;

    struct timespec ts;
    timespec_add(&now, &delay, &ts);
    return ts;
}

double time_now_monotonic_dbs(void) {
    struct timespec ts = time_now_monotonic();
    return timespec2dbs(&ts);
}

double time_now_monotonic_dbms(void) {
    struct timespec ts = time_now_monotonic();
    return timespec2dbms(&ts);
}

int db2timespec(double secs, struct timespec *ts) {
    assert(secs >= 0);
    assert(ts);

    bool converted = false;
    double i = get_integral_part(secs);
    double d = get_decimal_part(secs);

    ts->tv_sec = db2long(i, true, 0, &converted);
    if (!converted) return -1;

    ts->tv_sec = db2long(d, true, 0, &converted);
    if (!converted) return -1;

    return 0;
}

enum comparatorResult timespec_cmp(const struct timespec *a,
                                   const struct timespec *b) {
    assert(a);
    assert(b);

    if (a->tv_sec > b->tv_sec) {
        return GT;
    } else if (a->tv_sec < b->tv_sec) {
        return LT;
    }

    /* else, use nanosecond discriminant */
    if (a->tv_nsec > b->tv_nsec) {
        return GT;
    } else if (a->tv_nsec < b->tv_nsec) {
        return LT;
    }

    return EQ;
}

/*
 * Add the values in timespec a and b and write them to c.
 * Note that c could be one of a or b, repeated.
 * If the values are too big, the tv_sec member may overflow so the
 * user should be careful of the inputs.
 *
 * NOTE: time_t is normally signed; this library is compiled with -frapv
 * to ensure well-defined wraparound for signed integers.
 */
void timespec_add(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *c) {
    assert(a);
    assert(b);
    assert(c);

    time_t secs = a->tv_sec + b->tv_sec;  // may wrap around
    uint64_t nsecs = a->tv_nsec + b->tv_nsec;

    secs += nsecs / NSECS_PER_SEC; /* / 10^9 */
    nsecs = nsecs % NSECS_PER_SEC;
    c->tv_sec = secs;
    c->tv_nsec = nsecs;
}

struct timespec ms2timespec(uint32_t ms) {
    struct timespec ts = {0};
    ts.tv_sec = ms / MSECS_PER_SEC;
    ts.tv_nsec = (ms % MSECS_PER_SEC) * NSECS_PER_MSEC;
    return ts;
}

int mssleep(uint32_t ms, bool uninterruptible) {
    struct timespec ts = time_now_monotonic();
    struct timespec millis = ms2timespec(ms);
    timespec_add(&ts, &millis, &ts);

    int rc = 0;

    // continue if interrupted
    while ((rc = (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) ==
                  -1)) &&
           uninterruptible && errno == EINTR) {
        errno = 0;
    }

    if (rc) { /* an error other than EINTR */
        error(
          "clock_nanosleep() error in %s() : '%s'", __func__, strerror(errno));
        return -1;
    }

    return 0;
}
