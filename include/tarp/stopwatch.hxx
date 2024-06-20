#pragma once

#include <chrono>

namespace tarp {

/*
 * Convenience class to get the elapsed duration between two points
 * in time to a specified resolution (milliseconds by default).
 * NOTE: the ceiling of the converted value is returned. For example,
 * if 10 ms has elapsed and the returned value is in seconds, this will
 * be 1s.
 */
template<typename resolution = std::chrono::milliseconds>
class StopWatch {
public:
    StopWatch() : m_start_time(), m_stop_time() {};

    void start(void) { m_start_time = clock_type::now(); }

    void stop(void) { m_stop_time = clock_type::now(); }

    resolution get_time(void) const {
        auto time = m_stop_time - m_start_time;
        return std::chrono::ceil<resolution>(time);
    }

private:
    using clock_type = std::chrono::high_resolution_clock;
    std::chrono::time_point<clock_type> m_start_time;
    std::chrono::time_point<clock_type> m_stop_time;
};

}  // namespace tarp
