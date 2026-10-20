#include "FrontlightNight.h"

#include <Arduino.h>
#include <CrossInkHalFrontlight.h>
#include <HalClock.h>

#include <utility>

#include "CrossPointSettings.h"
#include "util/LocalClockOffset.h"

namespace {

// -1 = not read yet. Remembered so tick() acts only when the schedule's
// answer changes, never against a switch made by hand in between.
int8_t lastWanted = -1;

bool scheduleWants(bool& night) {
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.getTime(hour, minute)) return false;
  const uint16_t now = FrontlightSchedule::localTimeOfDay(hour, minute, localClockOffsetQ());
  night = FrontlightNight::scheduleWantsNight(SETTINGS.frontlightNightEnabled != 0, SETTINGS.frontlightNightStart,
                                              SETTINGS.frontlightNightEnd, now);
  return true;
}

void swapSets(const bool night) {
  std::swap(SETTINGS.frontlightBrightness, SETTINGS.frontlightOtherBrightness);
  std::swap(SETTINGS.frontlightWarmth, SETTINGS.frontlightOtherWarmth);
  SETTINGS.frontlightLiveIsNight = night ? 1 : 0;
}

}  // namespace

namespace FrontlightNight {

bool isNight() { return SETTINGS.frontlightLiveIsNight != 0; }

bool setNight(const bool night) {
  if (isNight() == night) return false;
  // While the light runs, the driver holds the newest values of the live set.
  if (Frontlight.present()) {
    SETTINGS.frontlightBrightness = Frontlight.brightness();
    SETTINGS.frontlightWarmth = Frontlight.warmth();
  }
  swapSets(night);
  Frontlight.setBrightness(SETTINGS.frontlightBrightness);
  Frontlight.setWarmth(SETTINGS.frontlightWarmth);
  return true;
}

void applyAtBoot() {
  bool night = false;
  if (!scheduleWants(night)) return;
  lastWanted = night ? 1 : 0;
  // Night light switched off in Settings: back to the day set for good.
  if (SETTINGS.frontlightNightEnabled == 0) night = false;
  if (isNight() != night) swapSets(night);
}

bool tick() {
  static uint32_t lastCheckMs = 0;
  // The clock chip is read once a minute at most.
  if (lastCheckMs != 0 && millis() - lastCheckMs < 60000) return false;
  lastCheckMs = millis() | 1;
  bool night = false;
  if (!scheduleWants(night)) return false;
  const int8_t wanted = night ? 1 : 0;
  if (wanted == lastWanted) return false;
  lastWanted = wanted;
  return setNight(night);
}

}  // namespace FrontlightNight
