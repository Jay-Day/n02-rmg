#include "timer.h"
#include <time.h>
#include <thread>
#include <chrono>

namespace Timer {

uint32_t getTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void sleep(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

uint64_t getTickCountMicros() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000);
}

} // namespace Timer
