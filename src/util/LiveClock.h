#pragma once

#include <Arduino.h>
#include <HalClock.h>

#include <cstdint>

#include "CrossPointSettings.h"

// Live Clock: due() turns true once each time the minute changes, while the
// option is on, so a screen can redraw the clock it shows.
class LiveClockTicker {
  uint8_t shownMinute = 0xFF;
  unsigned long checkedMs = 0;

 public:
  bool due() {
    if (!SETTINGS.liveClock || !halClock.isAvailable()) return false;
    const unsigned long now = millis();
    if (now - checkedMs < 1000UL) return false;
    checkedMs = now;
    uint8_t hour = 0, minute = 0;
    if (!halClock.getTime(hour, minute)) return false;
    if (minute == shownMinute) return false;
    const bool first = shownMinute == 0xFF;
    shownMinute = minute;
    return !first;
  }
};
