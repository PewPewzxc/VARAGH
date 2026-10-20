#pragma once

#include <cstdint>
#include <string>

namespace HalSystem {
struct StackFrame {
  uint32_t sp;
  uint32_t spp[8];
};

void begin();

// Dump panic info to SD card if necessary
// Writes /crash_report.txt after a panic reboot. `when` is the local date and
// time to put in it ("2026-10-19 21:05"), or empty when the clock is not set.
void checkPanic(const char* when = "");
void clearPanic();

std::string getPanicInfo(bool full = false);
bool isRebootFromPanic();
}  // namespace HalSystem
