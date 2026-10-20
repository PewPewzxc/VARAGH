#include <gtest/gtest.h>

#include "FrontlightSchedule.h"

namespace {
constexpr uint16_t timeOfDay(const uint8_t hour, const uint8_t minute = 0) {
  return static_cast<uint16_t>(hour * 60 + minute);
}
}  // namespace

TEST(FrontlightSchedule, DisabledOrIncompleteWindowIsInactive) {
  EXPECT_FALSE(FrontlightSchedule::hasCompleteWindow(false, timeOfDay(18), timeOfDay(7)));
  EXPECT_FALSE(FrontlightSchedule::hasCompleteWindow(true, FrontlightSchedule::kUnsetTimeOfDay, timeOfDay(7)));
  EXPECT_FALSE(FrontlightSchedule::hasCompleteWindow(true, timeOfDay(18), FrontlightSchedule::kUnsetTimeOfDay));
}

TEST(FrontlightSchedule, RestoreOnWakeFallsThroughToScheduleOnlyWhenPreviouslyOff) {
  EXPECT_FALSE(FrontlightSchedule::shouldApplyOnWakeSchedule(false, true, true));
  EXPECT_TRUE(FrontlightSchedule::shouldApplyOnWakeSchedule(false, true, false));
  EXPECT_TRUE(FrontlightSchedule::shouldApplyOnWakeSchedule(false, false, true));
  EXPECT_FALSE(FrontlightSchedule::shouldApplyOnWakeSchedule(true, false, false));
}

TEST(FrontlightSchedule, SilentRestartPreservesLightStateWithoutWakePolicy) {
  EXPECT_TRUE(FrontlightSchedule::shouldPreserveLightAcrossRestart(/*isSilentReboot=*/true));
  EXPECT_FALSE(FrontlightSchedule::shouldPreserveLightAcrossRestart(/*isSilentReboot=*/false));
  EXPECT_TRUE(FrontlightSchedule::shouldRestoreLightOnStart(
      /*preserveLightAcrossRestart=*/true, /*restoreOnWake=*/false, /*wasLightOnBeforeSleep=*/true));
  EXPECT_FALSE(FrontlightSchedule::shouldRestoreLightOnStart(
      /*preserveLightAcrossRestart=*/true, /*restoreOnWake=*/true, /*wasLightOnBeforeSleep=*/false));
  EXPECT_FALSE(FrontlightSchedule::shouldApplyOnWakeSchedule(
      /*preserveLightAcrossRestart=*/true, /*restoreOnWake=*/false, /*wasLightOnBeforeSleep=*/true));
  EXPECT_FALSE(FrontlightSchedule::shouldApplyOnWakeSchedule(
      /*preserveLightAcrossRestart=*/true, /*restoreOnWake=*/true, /*wasLightOnBeforeSleep=*/false));
}

TEST(FrontlightSchedule, SameEndpointIsAnEmptyWindow) {
  EXPECT_FALSE(FrontlightSchedule::hasCompleteWindow(true, timeOfDay(18), timeOfDay(18)));
  EXPECT_FALSE(FrontlightSchedule::containsTimeOfDay(timeOfDay(18), timeOfDay(18), timeOfDay(18)));
}

TEST(FrontlightSchedule, NormalWindowIncludesStartAndExcludesEnd) {
  EXPECT_TRUE(FrontlightSchedule::containsTimeOfDay(timeOfDay(7), timeOfDay(22), timeOfDay(7)));
  EXPECT_TRUE(FrontlightSchedule::containsTimeOfDay(timeOfDay(7), timeOfDay(22), timeOfDay(21, 59)));
  EXPECT_FALSE(FrontlightSchedule::containsTimeOfDay(timeOfDay(7), timeOfDay(22), timeOfDay(22)));
}

TEST(FrontlightSchedule, OvernightWindowWrapsMidnight) {
  EXPECT_TRUE(FrontlightSchedule::containsTimeOfDay(timeOfDay(21), timeOfDay(7), timeOfDay(23)));
  EXPECT_TRUE(FrontlightSchedule::containsTimeOfDay(timeOfDay(21), timeOfDay(7), timeOfDay(6, 59)));
  EXPECT_FALSE(FrontlightSchedule::containsTimeOfDay(timeOfDay(21), timeOfDay(7), timeOfDay(12)));
}

TEST(FrontlightSchedule, LocalTimeOfDayAppliesQuarterHourOffset) {
  EXPECT_EQ(FrontlightSchedule::localTimeOfDay(23, 45, 52), timeOfDay(0, 45));  // UTC+1
  EXPECT_EQ(FrontlightSchedule::localTimeOfDay(0, 15, 44), timeOfDay(23, 15));  // UTC-1
}

TEST(FrontlightSchedule, UnsetTimeStartsAtNoonAndRoundTripsTwelveHourValues) {
  const auto unset = FrontlightSchedule::timeOfDayFromMinutes(FrontlightSchedule::kUnsetTimeOfDay);
  EXPECT_EQ(unset.hour12, 12);
  EXPECT_EQ(unset.minute, 0);
  EXPECT_TRUE(unset.isPm);

  const auto midnight = FrontlightSchedule::timeOfDayFromMinutes(0);
  EXPECT_EQ(midnight.hour12, 12);
  EXPECT_FALSE(midnight.isPm);
  EXPECT_EQ(FrontlightSchedule::minutesFromTimeOfDay(midnight.hour12, midnight.minute, midnight.isPm), 0);

  const auto afternoon = FrontlightSchedule::timeOfDayFromMinutes(timeOfDay(16, 43));
  EXPECT_EQ(afternoon.hour12, 4);
  EXPECT_EQ(afternoon.minute, 43);
  EXPECT_TRUE(afternoon.isPm);
  EXPECT_EQ(FrontlightSchedule::minutesFromTimeOfDay(afternoon.hour12, afternoon.minute, afternoon.isPm),
            timeOfDay(16, 43));
}

// VARAGH 1.3.0: night light and the Control Center light slider.
#include "FrontlightNight.h"
#include "FrontlightPanelModel.h"

TEST(FrontlightNight, NightAcrossMidnight) {
  EXPECT_TRUE(FrontlightNight::scheduleWantsNight(true, 22 * 60, 7 * 60, 23 * 60));
  EXPECT_TRUE(FrontlightNight::scheduleWantsNight(true, 22 * 60, 7 * 60, 3 * 60));
  EXPECT_FALSE(FrontlightNight::scheduleWantsNight(true, 22 * 60, 7 * 60, 7 * 60));
  EXPECT_FALSE(FrontlightNight::scheduleWantsNight(true, 22 * 60, 7 * 60, 12 * 60));
  EXPECT_FALSE(FrontlightNight::scheduleWantsNight(false, 22 * 60, 7 * 60, 23 * 60));
}

TEST(LightSlider, LeftHalfIsZeroToTwentyPercent) {
  EXPECT_EQ(lightPercentFromPermille(0), 0);
  EXPECT_EQ(lightPercentFromPermille(500), 20);
  EXPECT_EQ(lightPercentFromPermille(1000), 100);
  EXPECT_EQ(lightPercentFromPermille(100), 4);
  EXPECT_EQ(lightPercentFromPermille(200), 8);
  EXPECT_EQ(lightPercentFromPermille(-5), 0);
  EXPECT_EQ(lightPercentFromPermille(1200), 100);
}

TEST(LightSlider, EveryPercentRoundTrips) {
  for (int p = 0; p <= 100; ++p) {
    EXPECT_EQ(lightPercentFromPermille(lightPermilleFromPercent(static_cast<uint8_t>(p))), p) << p;
  }
}

TEST(LightSlider, KnobOverhangReachesTheEnds) {
  // 300 px slider, 64 px knob: the centre travels from pixel 32 to pixel 268.
  EXPECT_EQ(travelPermille(0, 300, 64), 0);       // on the left overhang
  EXPECT_EQ(travelPermille(107, 300, 64), 0);     // pixel 32: knob at the start
  EXPECT_EQ(travelPermille(1000, 300, 64), 1000); // on the right overhang
  EXPECT_EQ(travelPermille(897, 300, 64), 1000);  // pixel 268: knob at the end
  EXPECT_EQ(travelPermille(500, 300, 64), 495);   // pixel 149: 117 of 236
  EXPECT_EQ(travelPermille(500, 40, 64), 0);      // no travel: no division by zero
}
