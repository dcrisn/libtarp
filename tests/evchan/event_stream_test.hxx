#pragma once

#include <cstdint>
#include <chrono>

bool test_event_wstream(
        uint32_t num_msgs_per_producer,
        uint32_t num_producers,
        unsigned buffsz,
        std::chrono::seconds max_wait,
        std::chrono::microseconds producer_period
        );

bool test_event_rstream(
        uint32_t num_msgs,
        std::chrono::microseconds stream_period,
        uint32_t num_consumers,
        unsigned buffsz);
