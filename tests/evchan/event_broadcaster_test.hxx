#pragma once

#include <cstdint>
#include <chrono>

bool test_event_broadcaster(
        uint32_t num_msgs,
        std::chrono::microseconds broadcast_period,
        uint32_t num_consumers,
        unsigned consumer_buffsz);
