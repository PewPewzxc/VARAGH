#pragma once

#include <cstdint>

// Calendar arithmetic for the Clock & Calendar app: day counts, weekdays, ISO
// week numbers, the local date behind a UTC clock reading, and the Persian
// (Solar Hijri) date of a Gregorian day. No device code, so it is host-tested.
namespace calmath {

struct Date {
  int16_t year = 1970;
  uint8_t month = 1;  // 1..12
  uint8_t day = 1;    // 1..31

  bool operator==(const Date& other) const { return year == other.year && month == other.month && day == other.day; }
};

struct DateTime {
  Date date;
  uint8_t hour = 0;
  uint8_t minute = 0;
};

constexpr bool isLeapYear(const int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

constexpr int daysInMonth(const int year, const int month) {
  return month == 2 ? (isLeapYear(year) ? 29 : 28) : ((month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31);
}

// Days since 1 January 1970 (Howard Hinnant's civil-calendar algorithms).
constexpr int daysFromDate(const Date& date) {
  const int y = date.year - (date.month <= 2 ? 1 : 0);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const int yearOfEra = y - era * 400;
  const int dayOfYear = (153 * (date.month + (date.month > 2 ? -3 : 9)) + 2) / 5 + date.day - 1;
  const int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + dayOfEra - 719468;
}

constexpr Date dateFromDays(const int days) {
  const int z = days + 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int dayOfEra = z - era * 146097;
  const int yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const int dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const int mp = (5 * dayOfYear + 2) / 153;
  const int day = dayOfYear - (153 * mp + 2) / 5 + 1;
  const int month = mp < 10 ? mp + 3 : mp - 9;
  const int year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
  return Date{static_cast<int16_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day)};
}

// 0 = Monday ... 6 = Sunday.
constexpr int weekday(const Date& date) {
  const int days = daysFromDate(date);  // 1 January 1970 was a Thursday
  return ((days % 7) + 7 + 3) % 7;
}

constexpr Date addDays(const Date& date, const int days) { return dateFromDays(daysFromDate(date) + days); }

constexpr Date addMonths(const Date& date, const int months) {
  const int index = date.year * 12 + (date.month - 1) + months;
  const int year = index / 12;
  const int month = index % 12 + 1;
  const int last = daysInMonth(year, month);
  return Date{static_cast<int16_t>(year), static_cast<uint8_t>(month),
              static_cast<uint8_t>(date.day > last ? last : date.day)};
}

// ISO 8601 week number: weeks start on Monday and week 1 holds the year's first Thursday.
constexpr int isoWeek(const Date& date) {
  const int thursday = daysFromDate(date) - weekday(date) + 3;
  const Date inThursdayYear = dateFromDays(thursday);
  const int firstDay = daysFromDate(Date{inThursdayYear.year, 1, 1});
  return (thursday - firstDay) / 7 + 1;
}

// The local wall-clock time for a UTC reading and an offset in minutes.
constexpr DateTime localTime(const Date& utcDate, const int utcHour, const int utcMinute, const int offsetMinutes) {
  int minutes = utcHour * 60 + utcMinute + offsetMinutes;
  int dayShift = 0;
  while (minutes < 0) {
    minutes += 1440;
    --dayShift;
  }
  while (minutes >= 1440) {
    minutes -= 1440;
    ++dayShift;
  }
  return DateTime{addDays(utcDate, dayShift), static_cast<uint8_t>(minutes / 60), static_cast<uint8_t>(minutes % 60)};
}

// The Persian (Solar Hijri) date of a Gregorian day, by the 33-year arithmetic
// rule. It agrees with the official calendar for the years 1799 to 2256.
constexpr Date toPersian(const Date& date) {
  constexpr int kDaysBeforeMonth[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  const int gy = date.year;
  const int gy2 = date.month > 2 ? gy + 1 : gy;
  int days = 355666 + 365 * gy + (gy2 + 3) / 4 - (gy2 + 99) / 100 + (gy2 + 399) / 400 + date.day +
             kDaysBeforeMonth[date.month - 1];
  int year = -1595 + 33 * (days / 12053);
  days %= 12053;
  year += 4 * (days / 1461);
  days %= 1461;
  if (days > 365) {
    year += (days - 1) / 365;
    days = (days - 1) % 365;
  }
  const int month = days < 186 ? 1 + days / 31 : 7 + (days - 186) / 30;
  const int day = days < 186 ? 1 + days % 31 : 1 + (days - 186) % 30;
  return Date{static_cast<int16_t>(year), static_cast<uint8_t>(month), static_cast<uint8_t>(day)};
}

// Persian month names in Latin letters, 1..12.
inline const char* persianMonthName(const int month) {
  static const char* const kNames[12] = {"Farvardin", "Ordibehesht", "Khordad", "Tir",    "Mordad", "Shahrivar",
                                         "Mehr",      "Aban",        "Azar",    "Dey",    "Bahman", "Esfand"};
  return month >= 1 && month <= 12 ? kNames[month - 1] : "";
}

}  // namespace calmath
