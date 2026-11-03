#include <gtest/gtest.h>

#include "src/util/ClockNetSync.h"

TEST(ClockNetSync, ReadsBothServiceFormats) {
  int m = 0;
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("+0330", m));
  EXPECT_EQ(m, 210);
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("-0500\n", m));
  EXPECT_EQ(m, -300);
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("+0545", m));
  EXPECT_EQ(m, 345);
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("12600\n", m));
  EXPECT_EQ(m, 210);
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("-34200", m));
  EXPECT_EQ(m, -570);
  EXPECT_TRUE(clocksync::parseUtcOffsetMinutes("0", m));
  EXPECT_EQ(m, 0);
}

TEST(ClockNetSync, RejectsJunk) {
  int m = 7;
  EXPECT_FALSE(clocksync::parseUtcOffsetMinutes("", m));
  EXPECT_FALSE(clocksync::parseUtcOffsetMinutes("<html>", m));
  EXPECT_FALSE(clocksync::parseUtcOffsetMinutes("+0330 rate limited", m));
  EXPECT_FALSE(clocksync::parseUtcOffsetMinutes("99999999", m));
  EXPECT_FALSE(clocksync::parseUtcOffsetMinutes(nullptr, m));
  EXPECT_EQ(m, 7);
}
