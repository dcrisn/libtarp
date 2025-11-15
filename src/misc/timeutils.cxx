#include <tarp/timeutils.hxx>

#include <cassert>
#include <stdexcept>

namespace tarp::time_utils {
template<>
void chrono2timespec<std::chrono::seconds>(std::chrono::seconds secs,
                                           struct timespec *tspec,
                                           bool throw_on_overflow) {
    assert(tspec);
    tspec->tv_nsec = 0;
    tspec->tv_sec = secs.count();

    // tv_sec is bigger than chrono's seconds duration so this will never
    // overflow but is kept for consistency. Also, if the c++ standard
    // changes the number of bits, it is a good sanity check.
    if (throw_on_overflow) {
        if (secs.count() != tspec->tv_sec)
            throw std::overflow_error("timespec overflow");
    }
}

template<>
void chrono2timespec<std::chrono::milliseconds>(std::chrono::milliseconds ms,
                                                struct timespec *tspec,
                                                bool throw_on_overflow) {
    assert(tspec);
    tspec->tv_nsec = (ms.count() % MSECS_PER_SEC) * NSECS_PER_MSEC;
    tspec->tv_sec = ms.count() / MSECS_PER_SEC;

    if (throw_on_overflow) {
        if (ms.count() / MSECS_PER_SEC != tspec->tv_sec)
            throw std::overflow_error("timespec overflow");
    }
}

template<>
void chrono2timespec<std::chrono::microseconds>(std::chrono::microseconds us,
                                                struct timespec *tspec,
                                                bool throw_on_overflow) {
    assert(tspec);
    tspec->tv_nsec = (us.count() % USECS_PER_SEC) * NSECS_PER_USEC;
    tspec->tv_sec = us.count() / USECS_PER_SEC;

    if (throw_on_overflow) {
        if (us.count() / USECS_PER_SEC != tspec->tv_sec)
            throw std::overflow_error("timespec overflow");
    }
}

}  // namespace tarp::time_utils
