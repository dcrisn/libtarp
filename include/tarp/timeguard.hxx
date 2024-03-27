#pragma once

#include <chrono>
#include <iostream>

#include <tarp/log.h>
#include <tarp/math.h>

namespace tarp {

/*
 * This class is an abstraction for working with time intervals delays.
 * It can be advanced by whole time interval durations and can
 * be used to check whether the duration last set has elapsed.
 *
 * Expected usage is:
 * - create guard; NOTE: if created before the guard actually starts
 *   being used (and primarily, if the interval is short), then the
 *   guard must be reset() to recalibrate it with the current time.
 * - while the guard is up, do not do whatever action should not be
 *   done for the duration specified for the guard.
 * - when the guard is down, do whatever action is supposed to be
 *   performed. Then shift() the guard so that it is raised up
 *   again for another duration.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Therefore a guard is used to only perform a certain action at
 * given intervals and seeks to reduce boilerplate checks
 * and error proneness when dealing with this sort of scenario.
 * Its outputs may naturallly be fed directly or indirectly to
 * a timer or be used to put a thread to sleep.
 * NOTE: the basic case is that of a fixed time interval.
 * However, the guard may be extended through derived classes
 * to implement other more complex behaviors, such as exponentially
 * increasing intervals to simplify implementation of exponential
 * backoff algorithms.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * When the guard is:
 *  + up  => the latest end timepoint is still in the future and hence
 * a one-interval duration has not elapsed. Whatever action it is that
 * is being 'guarded' (i.e. not supposed to be done until the specified
 * time interval has elapsed) is considered to still be protected.
 *
 * + down => A whole interval duration has elapsed or the latest
 * end timepoint is now in the past. Also see notes below.
 *
 * + enabled => indicates whether a guard is actually being used or not.
 * The user may choose to disable a guard after a certain period for
 * whatever reason. NOTE: a user can only disable a guard, but not enable
 * it. A disabled guard must be reset() to re-enable it.
 *
 * + disabled => guard is not being used. A disabled guard is always 'down'
 * and unresponsive to shift()s -- its end timepoint stays whatever it
 * was at the time the guard was disabled.
 *
 * + shift()-ed => the end timepoint of the guard is advanced by one whole
 * interval. If the guard is up to date with the current time, this means
 * the guard is up until some point in the future. If it has fallen behind,
 * on the other hand, it may mean the guard comes down again instantly.
 * See point below.
 *
 * NOTE: On lagging time guards
 * ----------------------------
 * A guard is meant to be advanced in time interval steps -- although
 * it can be advanced by a number of intervals, this is only meant
 * for exceptional cases. Expected usage is for a guard to be
 * advanced by a whole time interval as soon as it has dropped 'down'.
 * If this is not done, then the guard falls behind and if it is
 * far enough behind (e.g. many intervals) then it will be 'down'
 * every time it is advanced, until it has caught up with the current
 * time on the std::steady_clock. If this happens:
 * + it normally means the wrong interval is being used with the guard.
 * Clearly, the user cannot keep up with intervals that short so a loger
 * one should would have been appropriate.
 * + the guard can be reset() which recalibrates it so its starting time
 * point is NOW. But if the previous point is true, it will fall behind
 * again.
 * + the guard can be advanced a number of intervals at a time to make
 * it catch up with or keep abreast of the current time.
 *
 * NOTE: Exponential time guards
 * ------------------------------
 * The TimeGuard class can be subclassed and extended by derived classes
 * to change the way the *next* interval duration is derived.
 * Currently:
 *
 * 1. TimeGuard
 * Virtual Base class (but *not* abstract). This always advances its
 * end timepoint linearly, i.e. by fixed-increment intervals.
 * IOW the interval is fixed hence the guard is 'periodic':
 *  => end timepoint = start timepoint + n*interval, where n=1,2,3, ...
 *
 * 2. ExpTimeGuar
 * This advances its end timepoint by exponentially-increasing intervals.
 *  => end timepoint = start timepoint + (base^n * interval), where
 * n=0,1,2,3,...
 *
 * NOTE: For the fundamental example of base=2 (binary exponential) the
 * interval is therefore always doubled. The exponential time_interval is
 * meant to facilitate implementations of exponential-backoff timers.
 *
 * NOTE: 'n' is incremented on each call to 'shift'. Normally this is to be
 * called by the user on immediate expiry of the previous end timepoint.
 */
template<typename T>
class TimeGuard {
public:
    /*
     * Create a timeguard with the specified characteristics.
     *
     * @param interval
     * The time duration that the guard stays up for once shift()ed
     * (or specifically, the guard stays up until its updated end
     * timepoint is in the past).
     *
     * @param start_low
     * If true, up() will return false when called for the first time
     * on the newly-created and not yet shift()-ed guard. Otherwise
     * up() returns true for the first guarded duration.
     *
     * @param max_intervals
     * + If < 0, then there is no upper limit to the number of intervals.
     * + if 0, it means the guard is disabled. See details above on what
     * this means.
     * + of > 0, then the guard only stays enabled for max_intervals and
     * then becomes automatically disabled.
     */
    TimeGuard(const T &interval = {},
              bool start_low = true,
              int max_intervals = -1);

    bool up() const;
    bool down() const;
    bool enabled() const;
    bool disabled() const;
    void disable();

    /*
     * Reset the guard to its initial state as configured.
     * Specifically, the guard is re-enabled and recalibrated
     * to the current time. And for exponential guards the
     * exponent is set back to 0.
     */
    void reset();

    /*
     * Shift the guard end time point by another duration
     * and bring the guard up until that time point is in
     * the past.
     *
     * If num_intervals > 1, then the next num_intervals are
     * joined together into one single interval in terms of
     * how much to shift the end time point of the guard.
     *
     * NOTE: if max_intervals > 0 and num_intervals is greater than
     * the number of intervals left available, then all the remaining
     * intervals are joined, and the guard is disabled after it next
     * comes down.
     *
     * NOTE: calling shift() on a disabled guard does nothing.
     */
    void shift(unsigned int num_intervals = 1);

    void operator<<(int num_intervals) { shift(num_intervals); }

    /*
     * Get the next guard duration that will be when shift() is
     * next called. This may be used to e.g. put a thread to
     * sleep or as a timeout value etc. */
    std::chrono::steady_clock::duration get_next_interval() const;

    /*
     * Get the end timepoint of the guard. */
    std::chrono::steady_clock::time_point get_next_timepoint() const;

protected:
    virtual void initialize(void);
    virtual void advance(void);
    std::chrono::steady_clock::time_point now() const;
    void set_end_timepoint(std::chrono::steady_clock::time_point tp);
    std::chrono::steady_clock::duration get_base_interval() const;

private:
    bool maxed_out(void) const;

    const bool m_start_low;
    const T m_base_interval;
    const int m_max_intervals;

    bool m_enabled;
    mutable bool m_up;
    unsigned int m_intervals_left;
    std::chrono::steady_clock::duration m_next_interval;
    std::chrono::steady_clock::time_point m_next_timepoint;
};

template<typename T>
TimeGuard<T>::TimeGuard(const T &interval, bool start_low, int max_intervals)
    : m_start_low(start_low)
    , m_base_interval(interval)
    , m_max_intervals(max_intervals) {
    initialize();
}

template<typename T>
bool TimeGuard<T>::maxed_out() const {
    // no intervals left
    return (m_max_intervals > 0 && m_intervals_left == 0);
}

template<typename T>
void TimeGuard<T>::initialize() {
    m_up = not m_start_low;
    m_intervals_left = m_max_intervals > 0 ? m_max_intervals : 0;
    m_enabled = (m_max_intervals != 0);

    /* If m_start_low=true, set the next timepoint
     * to the current time so that up() will evaluate
     * to false and therefore the guard will start low.
     * Else if !m_start_low, guard the first interval as
     * normal. */
    m_next_timepoint = now();
    if (!m_start_low) {
        shift(1);
    }
}

// Assumes reasonable use such that overflow/wraparound
// is not a concern.
template<typename T>
void TimeGuard<T>::shift(unsigned int num_intervals) {
    if (maxed_out() or disabled()) return;

    if (m_intervals_left > 0) {
        if (m_intervals_left < num_intervals) {
            num_intervals = m_intervals_left;
        }
        m_intervals_left -= num_intervals;
        if (m_intervals_left == 0) {
            m_enabled = false;
        }
    }

    auto last_timepoint = m_next_timepoint;
    for (unsigned int i = 0; i < num_intervals; ++i) {
        crit("advancing");
        advance();
    }
    m_next_interval = m_next_timepoint - last_timepoint;
    m_up = true;
}

template<typename T>
void TimeGuard<T>::reset() {
    initialize();
}

template<typename T>
bool TimeGuard<T>::up() const {
    /* a disabled guard is always down */
    if (disabled()) return false;

    /* else if up, lower it if time is actually in the past */
    if (now() >= m_next_timepoint) {
        m_up = false;
    }

    return m_up;
}

template<typename T>
bool TimeGuard<T>::down() const {
    return not up();
}

template<typename T>
bool TimeGuard<T>::enabled() const {
    return m_enabled;
}

template<typename T>
bool TimeGuard<T>::disabled() const {
    return not m_enabled;
}

template<typename T>
void TimeGuard<T>::disable() {
    m_enabled = false;
}

template<typename T>
std::chrono::steady_clock::time_point TimeGuard<T>::get_next_timepoint() const {
    return m_next_timepoint;
}

template<typename T>
std::chrono::steady_clock::duration TimeGuard<T>::get_next_interval() const {
    return m_next_interval;
}

template<typename T>
std::chrono::steady_clock::time_point TimeGuard<T>::now() const {
    return std::chrono::steady_clock::now();
}

template<typename T>
void TimeGuard<T>::set_end_timepoint(std::chrono::steady_clock::time_point tp) {
    m_next_timepoint = tp;
}

template<typename T>
std::chrono::steady_clock::duration
TimeGuard<T>::get_base_interval(void) const {
    return m_base_interval;
}

/* Increment m_next_timepoint by one interval.
 * Note how the interval is calculated is provided by derived
 * classes and depends on the type of guard: periodic, expoential
 * etc. */
template<typename T>
void TimeGuard<T>::advance() {
    m_next_timepoint += m_base_interval;
}

template<typename T>
class ExpTimeGuard : public TimeGuard<T> {
public:
    ExpTimeGuard(const T &interval = {},
                 unsigned int base = 2,
                 bool start_low = true,
                 int max_intervals = -1);

private:
    virtual void initialize(void) override;
    virtual void advance(void) override;

    unsigned int m_base;
    uint64_t m_last_index;
};

template<typename T>
ExpTimeGuard<T>::ExpTimeGuard(const T &interval,
                              unsigned int base,
                              bool start_low,
                              int max_intervals)
    : TimeGuard<T>(interval, start_low, max_intervals)
    , m_base(base)
    , m_last_index(0) {
}

template<typename T>
void ExpTimeGuard<T>::initialize() {
    std::cerr << "Called exptimeguard initializer\n";
    TimeGuard<T>::initialize();
    m_last_index = 0;
}

template<typename T>
void ExpTimeGuard<T>::advance() {
    crit("called right advance, base is %u", m_base);
    auto tp = this->get_next_timepoint();

    if (m_base == 2) {
        tp += this->get_base_interval() * (1 << m_last_index++);
    } else {
        tp += (this->get_base_interval() *
               tarp::math::intpow<std::uint64_t>(m_base, m_last_index++));
    }

    this->set_end_timepoint(tp);
}


}  // namespace tarp
