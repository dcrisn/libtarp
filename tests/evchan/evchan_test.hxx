#pragma once

#include <cstdint>
#include <chrono>

bool test_chan_monitor(uint32_t num_producers, uint32_t num_consumers,
        uint32_t num_msgs,
        unsigned producer_buffsz,
        unsigned consumer_buffsz, 
        std::chrono::seconds max_duration);

bool test_chan_interfaces(unsigned num_msgs, unsigned chan_capacity, std::chrono::seconds duration);
 
