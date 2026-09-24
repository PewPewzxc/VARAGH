#include <gtest/gtest.h>

#include "DaylightSaving.h"

namespace {
constexpr uint8_t BERLIN = 48 + 4;     // UTC+1
constexpr uint8_t NEW_YORK = 48 - 20;  // UTC-5
}  // namespace

TEST(DaylightSaving, WeekdaysAndSundays) {
  EXPECT_EQ(dst::dayOfWeek(2026, 9, 23), 3);  // Wednesday
  EXPECT_EQ(dst::sundayOfMonth(2026, 3, 0), 29);
  EXPECT_EQ(dst::sundayOfMonth(2026, 10, 0), 25);
  EXPECT_EQ(dst::sundayOfMonth(2025, 3, 0), 30);
  EXPECT_EQ(dst::sundayOfMonth(2026, 3, 2), 8);
  EXPECT_EQ(dst::sundayOfMonth(2026, 11, 1), 1);
}

TEST(DaylightSaving, EuSwitchesAtOneUtc) {
  EXPECT_FALSE(dst::isActive(dst::EU, 2026, 3, 29, 0, 59, BERLIN));
  EXPECT_TRUE(dst::isActive(dst::EU, 2026, 3, 29, 1, 0, BERLIN));
  EXPECT_TRUE(dst::isActive(dst::EU, 2026, 7, 1, 12, 0, BERLIN));
  EXPECT_TRUE(dst::isActive(dst::EU, 2026, 10, 25, 0, 59, BERLIN));
  EXPECT_FALSE(dst::isActive(dst::EU, 2026, 10, 25, 1, 0, BERLIN));
  EXPECT_FALSE(dst::isActive(dst::EU, 2026, 1, 15, 12, 0, BERLIN));
  EXPECT_TRUE(dst::isActive(dst::EU, 2025, 3, 30, 1, 0, BERLIN));
}

TEST(DaylightSaving, UsSwitchesAtTwoLocal) {
  EXPECT_FALSE(dst::isActive(dst::US, 2026, 3, 8, 6, 59, NEW_YORK));
  EXPECT_TRUE(dst::isActive(dst::US, 2026, 3, 8, 7, 0, NEW_YORK));
  EXPECT_TRUE(dst::isActive(dst::US, 2026, 11, 1, 5, 59, NEW_YORK));
  EXPECT_FALSE(dst::isActive(dst::US, 2026, 11, 1, 6, 0, NEW_YORK));
}

TEST(DaylightSaving, EffectiveOffset) {
  EXPECT_EQ(dst::effectiveOffsetQ(dst::OFF, 2026, 7, 1, 12, 0, BERLIN), BERLIN);
  EXPECT_EQ(dst::effectiveOffsetQ(dst::EU, 2026, 7, 1, 12, 0, BERLIN), BERLIN + 4);
  EXPECT_EQ(dst::effectiveOffsetQ(dst::EU, 2026, 12, 1, 12, 0, BERLIN), BERLIN);
  EXPECT_EQ(dst::effectiveOffsetQ(dst::EU, 2026, 7, 1, 12, 0, 104), 104);  // clamped at UTC+14
  EXPECT_EQ(dst::effectiveOffsetQ(dst::EU, 2026, 0, 1, 12, 0, BERLIN), BERLIN);  // invalid RTC date
}
