#include <gtest/gtest.h>

#include "src/util/CalendarMath.h"

using calmath::Date;

TEST(CalendarMath, DayCountsRoundTrip) {
  EXPECT_EQ(calmath::daysFromDate(Date{1970, 1, 1}), 0);
  EXPECT_EQ(calmath::daysFromDate(Date{2000, 3, 1}), 11017);
  for (int days = 10000; days < 40000; days += 37) {
    EXPECT_EQ(calmath::daysFromDate(calmath::dateFromDays(days)), days);
  }
  EXPECT_EQ(calmath::dateFromDays(calmath::daysFromDate(Date{2024, 2, 29})), (Date{2024, 2, 29}));
}

TEST(CalendarMath, Weekdays) {
  EXPECT_EQ(calmath::weekday(Date{1970, 1, 1}), 3);   // Thursday
  EXPECT_EQ(calmath::weekday(Date{2026, 10, 6}), 1);  // Tuesday
  EXPECT_EQ(calmath::weekday(Date{2026, 10, 1}), 3);  // Thursday
  EXPECT_EQ(calmath::weekday(Date{2024, 2, 29}), 3);  // Thursday
  EXPECT_EQ(calmath::weekday(Date{2000, 1, 1}), 5);   // Saturday
}

TEST(CalendarMath, MonthLengthsAndStepping) {
  EXPECT_EQ(calmath::daysInMonth(2024, 2), 29);
  EXPECT_EQ(calmath::daysInMonth(2026, 2), 28);
  EXPECT_EQ(calmath::daysInMonth(2100, 2), 28);
  EXPECT_EQ(calmath::daysInMonth(2026, 10), 31);
  EXPECT_EQ(calmath::addMonths(Date{2026, 10, 6}, 3), (Date{2027, 1, 6}));
  EXPECT_EQ(calmath::addMonths(Date{2026, 1, 31}, 1), (Date{2026, 2, 28}));
  EXPECT_EQ(calmath::addMonths(Date{2026, 1, 15}, -1), (Date{2025, 12, 15}));
  EXPECT_EQ(calmath::addDays(Date{2026, 12, 31}, 1), (Date{2027, 1, 1}));
}

TEST(CalendarMath, IsoWeekNumbers) {
  EXPECT_EQ(calmath::isoWeek(Date{2026, 10, 6}), 41);
  EXPECT_EQ(calmath::isoWeek(Date{2026, 1, 1}), 1);
  EXPECT_EQ(calmath::isoWeek(Date{2027, 1, 1}), 53);   // a Friday, still week 53 of 2026
  EXPECT_EQ(calmath::isoWeek(Date{2024, 12, 30}), 1);  // a Monday, already week 1 of 2025
  EXPECT_EQ(calmath::isoWeek(Date{2021, 1, 3}), 53);
}

TEST(CalendarMath, LocalTimeCrossesMidnight) {
  // Berlin summer time, UTC+2.
  auto local = calmath::localTime(Date{2026, 10, 6}, 23, 15, 120);
  EXPECT_EQ(local.date, (Date{2026, 10, 7}));
  EXPECT_EQ(local.hour, 1);
  EXPECT_EQ(local.minute, 15);
  // Tehran, UTC+3:30.
  local = calmath::localTime(Date{2026, 12, 31}, 21, 0, 210);
  EXPECT_EQ(local.date, (Date{2027, 1, 1}));
  EXPECT_EQ(local.hour, 0);
  EXPECT_EQ(local.minute, 30);
  // West of Greenwich.
  local = calmath::localTime(Date{2026, 3, 1}, 2, 0, -300);
  EXPECT_EQ(local.date, (Date{2026, 2, 28}));
  EXPECT_EQ(local.hour, 21);
}

// Nowruz (1 Farvardin) dates as published for these years.
TEST(CalendarMath, PersianNewYears) {
  EXPECT_EQ(calmath::toPersian(Date{2017, 3, 21}), (Date{1396, 1, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2020, 3, 20}), (Date{1399, 1, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2021, 3, 21}), (Date{1400, 1, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2024, 3, 20}), (Date{1403, 1, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2025, 3, 21}), (Date{1404, 1, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2026, 3, 21}), (Date{1405, 1, 1}));
}

TEST(CalendarMath, PersianDatesInsideTheYear) {
  EXPECT_EQ(calmath::toPersian(Date{2026, 10, 6}), (Date{1405, 7, 14}));
  EXPECT_EQ(calmath::toPersian(Date{2026, 3, 20}), (Date{1404, 12, 29}));
  EXPECT_EQ(calmath::toPersian(Date{2025, 3, 20}), (Date{1403, 12, 30}));  // 1403 is a leap year
  EXPECT_EQ(calmath::toPersian(Date{2026, 9, 22}), (Date{1405, 6, 31}));
  EXPECT_EQ(calmath::toPersian(Date{2026, 9, 23}), (Date{1405, 7, 1}));
  EXPECT_EQ(calmath::toPersian(Date{2026, 12, 21}), (Date{1405, 9, 30}));
  EXPECT_STREQ(calmath::persianMonthName(7), "Mehr");
}
