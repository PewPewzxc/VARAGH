#pragma once

#include <HalClock.h>

#include <cstdint>

#include "CrossPointSettings.h"

// The user's UTC offset for converting RTC (UTC) time to wall-clock time,
// including summer time when a daylight-saving rule is selected. The native
// simulator's clock has no daylight-saving support, so it keeps standard time.
inline uint8_t localClockOffsetQ() {
#ifdef SIMULATOR
  return SETTINGS.clockUtcOffsetQ;
#else
  return halClock.effectiveOffsetQ(SETTINGS.clockUtcOffsetQ);
#endif
}
