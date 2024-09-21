#pragma once

#include <cstdint>
#include <chrono>

bool test_channel_closing();
bool test_no_buffering();
bool test_unbuffered_mcsp();
bool test_unbuffered_mpsc();
bool test_try_for_timings();
bool test_benchmark(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs, std::chrono::seconds max_duration);

bool test_trunk_monitor(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs, std::chrono::seconds max_duration);

bool test_trunk_interfaces(unsigned num_msgs, std::chrono::seconds duration);

