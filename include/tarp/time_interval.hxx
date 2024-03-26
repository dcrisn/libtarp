#pragma once

#include <chrono>

#include <tarp/math.h>

namespace tarp {

template<typename T>
class TimeInterval {
public:
    /* Fixed period */
    TimeInterval(const T &interval, bool skip_first = true);

    /* Exponential period */
    TimeInterval(const T &interval, unsigned base, bool skip_first = true);

    bool elapsed() const;
    void reset();
    std::chrono::steady_clock::time_point get_next_timepoint() const;
    std::chrono::steady_clock::duration get_next_interval() const;

    void roll(void);

protected:
    virtual void roll(unsigned int num_intervals);
    virtual void rollexp();
    virtual void initialize();

private:
    std::chrono::steady_clock::time_point now() const;

    T m_base_interval;
    std::chrono::steady_clock::duration m_next_interval;
    std::chrono::steady_clock::time_point m_next_timepoint;
    bool m_skip_first;
    bool m_exponential;
    unsigned int m_exp_base;
    unsigned int m_last_exp;
};

template<typename T>
std::chrono::steady_clock::time_point TimeInterval<T>::now() const {
    return std::chrono::steady_clock::now();
}

template<typename T>
std::chrono::steady_clock::time_point
TimeInterval<T>::get_next_timepoint() const {
    return m_next_timepoint;
}

template<typename T>
std::chrono::steady_clock::duration TimeInterval<T>::get_next_interval() const {
    return m_next_interval;
}

template<typename T>
TimeInterval<T>::TimeInterval(const T &interval, bool skip_first)
    : m_base_interval(interval)
    , m_next_interval(interval)
    , m_skip_first(skip_first)
    , m_exponential(false)
    , m_exp_base(0)
    , m_last_exp(0) {
    initialize();
}

template<typename T>
TimeInterval<T>::TimeInterval(const T &interval,
                              unsigned int base,
                              bool skip_first)
    : m_base_interval(interval)
    , m_next_interval(interval)
    , m_skip_first(skip_first)
    , m_exponential(true)
    , m_exp_base(base)
    , m_last_exp(0) {
    initialize();
}

template<typename T>
void TimeInterval<T>::reset() {
    initialize();
}

template<typename T>
void TimeInterval<T>::initialize() {
    m_next_interval = m_base_interval;
    m_last_exp = 0;

    m_next_timepoint = now();
    if (!m_skip_first) {
        m_next_timepoint += m_base_interval;
    }
}

template<typename T>
bool TimeInterval<T>::elapsed() const {
    return now() >= m_next_timepoint;
}

// Assumes reasonable use such that overflow/wraparound
// is not a concern.
template<typename T>
void TimeInterval<T>::roll(unsigned int num_intervals) {
    auto last_timepoint = m_next_timepoint;
    m_next_timepoint += (m_base_interval * num_intervals);
    m_next_interval = m_next_timepoint - last_timepoint;
}

template<typename T>
void TimeInterval<T>::roll() {
    if (m_exponential) {
        rollexp();
        return;
    }

    roll(1);
}

template<typename T>
void TimeInterval<T>::rollexp() {
    auto last_timepoint = m_next_timepoint;

    if (m_exp_base == 2) {
        m_next_timepoint += (m_base_interval * (1 << m_last_exp++));
    } else {
        m_next_timepoint +=
          (m_base_interval *
           tarp::math::intpow<std::uint64_t>(m_exp_base, m_last_exp++));
    }

    m_next_interval = m_next_timepoint - last_timepoint;
}


}  // namespace tarp
