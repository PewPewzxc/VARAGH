#pragma once

#include <cstdint>

#include "FrontlightSchedule.h"

// VARAGH night light: a second brightness/warmth set for the evening.
//
// SETTINGS.frontlightBrightness/Warmth always hold the set in use (so every
// existing light control keeps working unchanged); the other set waits in
// frontlightOtherBrightness/Warmth, and frontlightLiveIsNight says which one is
// live. Switching swaps the two. The night set is learned: whatever the reader
// sets while the night set is live is kept for next night.
namespace FrontlightNight {

// Pure: should the night set be live at this local time?
constexpr bool scheduleWantsNight(const bool enabled, const uint16_t start, const uint16_t end,
                                  const uint16_t localMinutes) {
  return enabled && FrontlightSchedule::containsTimeOfDay(start, end, localMinutes);
}

bool isNight();
// Make the night (or day) set live and drive the light with it. Returns true
// when it switched. The caller persists the settings.
bool setNight(bool night);
// Boot, before Frontlight.begin(): pick the set the clock asks for.
void applyAtBoot();
// Main loop: follows the schedule each time it crosses Night From / Night
// Until. A switch made with the Control Center button holds until then.
// Returns true when it switched (the caller persists).
bool tick();

}  // namespace FrontlightNight
