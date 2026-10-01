#pragma once

#include <atomic>
#include <cstdint>

// Test clock: tests advance it explicitly.
namespace fakeclock {
inline std::atomic<uint32_t> nowMs{1000};
}
inline uint32_t millis() { return fakeclock::nowMs.load(); }
inline uint32_t micros() { return millis() * 1000; }
inline void delay(uint32_t ms) { fakeclock::nowMs += ms; }
