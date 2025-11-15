#ifndef TARP_TIMEUTILS_H
#define TARP_TIMEUTILS_H


#include <tarp/common.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

#include <tarp/common.h>


#define MSECS_PER_SEC  1000U        /* 1e3 */
#define USECS_PER_SEC  1000000U     /* 1e6 */
#define NSECS_PER_SEC  1000000000LU /* 1e9 */
#define NSECS_PER_MSEC 1000000U     /* 1e6 */
#define NSECS_PER_USEC 1000U        /* 1e3 */

/*
 * timespec to double as the number of seconds or milliseconds */
double timespec2dbs(const struct timespec *tspec);
double timespec2dbms(const struct timespec *tspec);

/* convert a double number representing seconds to a timespec */
int db2timespec(double secs, struct timespec *tspec);

/* get the NOW timepoint on the monotonic system clock */
struct timespec time_now_monotonic(void);

/* get the NOW timepoint on the CLOCK_REALTIME system clock */
struct timespec time_now_epoch(void);

/* Get the time since the unix epoch + ms. This can be used for
 * computing an absolute time in the future as a unix timestamp. */
struct timespec get_unix_timepoint(unsigned ms);

/* like time_now_monotonic, but convert to double;
 * see timespec2db{s,ms} fmi. */
double time_now_monotonic_dbms(void);
double time_now_monotonic_dbs(void);

/* compare 2 timespecs; look for 'comparator' in common.h fmi */
enum comparatorResult timespec_cmp(const struct timespec *a,
                                   const struct timespec *b);

/* add 2 timespecs; note if unreasonable values are fed in as input, the
 * timespecs (the seconds member) may possibly overflow. */
void timespec_add(const struct timespec *a,
                  const struct timespec *b,
                  struct timespec *c);

/*
 * Convert ms to a timespec and return it. */
struct timespec ms2timespec(uint32_t ms);

/*
 * Sleep for ms milliseconds.
 *
 * If uninterruptible=True, then if the sleep is interrupted
 * by EINTR, the sleep is resumed until complete.
 *
 * <-- return
 * 0 on sucess (sleep completed); -1 on error (sleep not completed;
 * errno is set by clock_nanosleep).
 */
int mssleep(uint32_t ms, bool uninterruptible);


#ifdef __cplusplus
} /* extern "C" */
#endif


#endif /* TARP_TIMEUTILS_H  */
