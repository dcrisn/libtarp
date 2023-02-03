#ifndef TARP_TIME_UTILS
#define TARP_TIME_UTILS

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

double timespec_to_double(struct timespec *time);
void double_to_timespec(struct timespec *timespec, double secs);
uint64_t timespec_to_ms(struct timespec *time);
int timespec_cmp(struct timespec *a, struct timespec *b);
void timespec_add(struct timespec *a, struct timespec *b, struct timespec *c);
void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *c);
uint64_t msts(clockid_t clock);
void ms_to_timespec(uint64_t ms, struct timespec *timespec);
bool time_expired(struct timespec *a, clockid_t clock);
int mssleep(uint64_t ms, bool uninterruptible);
int timespec_tostring(const struct timespec *time, const char *specname, char buff[], size_t buffsz);

#endif
