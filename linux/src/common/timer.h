#pragma once

#include <cstdint>

namespace Timer {
    // Get current time in milliseconds (monotonic clock)
    uint32_t getTickCount();

    // Sleep for specified milliseconds
    void sleep(uint32_t ms);

    // High-resolution timer for performance measurement
    uint64_t getTickCountMicros();
}
