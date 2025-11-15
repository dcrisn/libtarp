#pragma once

#include <chrono>

#define MSECS_PER_SEC  1000U        /* 1e3 */
#define USECS_PER_SEC  1000000U     /* 1e6 */
#define NSECS_PER_SEC  1000000000LU /* 1e9 */
#define NSECS_PER_MSEC 1000000U     /* 1e6 */
#define NSECS_PER_USEC 1000U        /* 1e3 */

namespace tarp {
namespace time_utils {
using fractional_hours = std::chrono::duration<double, std::ratio<3600, 1>>;
using fractional_secs = std::chrono::duration<double, std::ratio<1, 1>>;
using fractional_ms = std::chrono::duration<double, std::milli>;
using fractional_us = std::chrono::duration<double, std::micro>;

//

/*
 * Convert a chrono duration to a timespec and throw an exception
 * if the tv_sec member of the timespec overflows */
template<typename chrono_duration>
void chrono2timespec(chrono_duration duration,
                     struct timespec *tspec,
                     bool throw_on_overflow = false);

template<>
void chrono2timespec<std::chrono::seconds>(std::chrono::seconds secs,
                                           struct timespec *tspec,
                                           bool throw_on_overflow);

template<>
void chrono2timespec<std::chrono::milliseconds>(std::chrono::milliseconds ms,
                                                struct timespec *tspec,
                                                bool throw_on_overflow);

template<>
void chrono2timespec<std::chrono::microseconds>(std::chrono::microseconds us,
                                                struct timespec *tspec,
                                                bool throw_on_overflow);
}  // namespace time_utils
}  // namespace tarp
