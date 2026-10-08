#pragma once

#include "ClockFace.h"
#include "GameUi.h"
#include "activities/Activity.h"

// Clock & Calendar: the time above a month, repainted once a minute while the
// screen is open. Three faces - a large digital clock, an analog clock, and a
// calendar that also carries the Persian day under every date - that a tap on
// the clock steps through. The arrows (or a swipe) turn the months; a tap on
// the month's name returns to today. Its settings live in Apps > Manage.
//
// The time comes from the reader's own clock with the time zone and summer
// time chosen in Settings; nothing here keeps the device awake.
class ClockActivity final : public Activity {
 public:
  ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  // A short line for the Apps menu (the weekday); empty when the clock is not set.
  static void statusLine(char* out, size_t size);

 private:
  calmath::Date shownMonth() const;

  clockface::Options options;
  int monthOffset = 0;  // months away from the current one
  bool clockSet = false;
  calmath::DateTime current;
  uint32_t lastCheckMs = 0;
  int lastCleanMinute = -1;  // minute of the day of the last clean repaint
};
