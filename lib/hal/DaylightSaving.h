#pragma once

#include <cstdint>

// Daylight-saving rules applied on top of the user's standard UTC offset.
// Pure date arithmetic on the UTC time the RTC keeps, so it is host-testable
// and needs no timezone database. Rule values are persisted in settings: append
// only.
namespace dst {

enum Rule : uint8_t {
  OFF = 0,
  EU = 1,  // Last Sunday of March 01:00 UTC to last Sunday of October 01:00 UTC
  US = 2,  // Second Sunday of March 02:00 to first Sunday of November 02:00 local
  RULE_COUNT
};

// Day of week for a Gregorian date: 0 = Sunday ... 6 = Saturday.
constexpr int dayOfWeek(int year, const int month, const int day) {
  constexpr int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

constexpr bool isLeapYear(const int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

constexpr int daysInMonth(const int year, const int month) {
  constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

// Day of month of the nth Sunday (n >= 1), or of the last Sunday when n == 0.
constexpr int sundayOfMonth(const int year, const int month, const int n) {
  if (n == 0) {
    const int last = daysInMonth(year, month);
    return last - dayOfWeek(year, month, last);
  }
  const int firstSunday = 1 + (7 - dayOfWeek(year, month, 1)) % 7;
  return firstSunday + (n - 1) * 7;
}

// Minutes from the start of the year, for ordering instants within one year.
constexpr int minuteOfYear(const int year, const int month, const int day, const int hour, const int minute) {
  int days = day - 1;
  for (int m = 1; m < month; ++m) days += daysInMonth(year, m);
  return (days * 24 + hour) * 60 + minute;
}

// True when `rule` puts clocks forward at this UTC instant.
// standardOffsetQ is the user's standard offset in quarter hours, biased by 48
// (48 = UTC), as stored in CrossPointSettings::clockUtcOffsetQ.
constexpr bool isActive(const uint8_t rule, const int year, const int month, const int day, const int hour,
                        const int minute, const uint8_t standardOffsetQ) {
  if (month < 1 || month > 12 || day < 1 || day > 31) return false;
  const int now = minuteOfYear(year, month, day, hour, minute);
  switch (rule) {
    case EU: {
      // The EU switches every zone at the same UTC instant.
      const int start = minuteOfYear(year, 3, sundayOfMonth(year, 3, 0), 1, 0);
      const int end = minuteOfYear(year, 10, sundayOfMonth(year, 10, 0), 1, 0);
      return now >= start && now < end;
    }
    case US: {
      // 02:00 local standard time on both edges (the November edge is 02:00
      // daylight time, i.e. 01:00 standard). Shift the edges into UTC.
      const int offsetMinutes = (static_cast<int>(standardOffsetQ) - 48) * 15;
      const int start = minuteOfYear(year, 3, sundayOfMonth(year, 3, 2), 2, 0) - offsetMinutes;
      const int end = minuteOfYear(year, 11, sundayOfMonth(year, 11, 1), 1, 0) - offsetMinutes;
      return now >= start && now < end;
    }
    default:
      return false;
  }
}

// The offset to display: standard offset plus one hour while the rule is active.
constexpr uint8_t effectiveOffsetQ(const uint8_t rule, const int year, const int month, const int day, const int hour,
                                   const int minute, const uint8_t standardOffsetQ) {
  const uint8_t base = standardOffsetQ > 104 ? 104 : standardOffsetQ;
  if (!isActive(rule, year, month, day, hour, minute, base)) return base;
  return base + 4 > 104 ? 104 : static_cast<uint8_t>(base + 4);
}

}  // namespace dst
