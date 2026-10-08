#include "ClockFace.h"

#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "GameUi.h"
#include "fontIds.h"
#include "util/LocalClockOffset.h"

namespace clockface {

namespace {

constexpr char kSavePath[] = "/.crosspoint/games/clock.bin";
constexpr uint8_t kFileMagic[4] = {'V', 'C', 'L', '2'};
constexpr int kGridMargin = 26;
// A clock chip that was never set reports a date from long ago.
constexpr int kEarliestYear = 2024;

constexpr StrId kMonthNames[12] = {StrId::STR_MONTH_1, StrId::STR_MONTH_2,  StrId::STR_MONTH_3,  StrId::STR_MONTH_4,
                                   StrId::STR_MONTH_5, StrId::STR_MONTH_6,  StrId::STR_MONTH_7,  StrId::STR_MONTH_8,
                                   StrId::STR_MONTH_9, StrId::STR_MONTH_10, StrId::STR_MONTH_11, StrId::STR_MONTH_12};
constexpr StrId kWeekdayNames[7] = {StrId::STR_WEEKDAY_1, StrId::STR_WEEKDAY_2, StrId::STR_WEEKDAY_3,
                                    StrId::STR_WEEKDAY_4, StrId::STR_WEEKDAY_5, StrId::STR_WEEKDAY_6,
                                    StrId::STR_WEEKDAY_7};

const gameui::DigitSet kLargeDigits{appart::kClockLargeGlyphs, appart::kClockLargeBits, appart::kClockLargeRows};
const gameui::DigitSet kMediumDigits{appart::kClockMediumGlyphs, appart::kClockMediumBits, appart::kClockMediumRows};

// The first `count` characters of a UTF-8 string.
void leadingCharacters(const char* text, const int count, char* out, const size_t size) {
  size_t length = 0;
  int taken = 0;
  while (text[length] != '\0' && length + 1 < size) {
    const bool startsCharacter = (static_cast<uint8_t>(text[length]) & 0xC0) != 0x80;
    if (startsCharacter && taken++ == count) break;
    ++length;
  }
  memcpy(out, text, length);
  out[length] = '\0';
}

// Column of a weekday (0 = Monday) in a week that starts on the chosen day.
int columnOf(const int weekday, const uint8_t weekStart) {
  const int first = weekStart == Saturday ? 5 : (weekStart == Sunday ? 6 : 0);
  return (weekday - first + 7) % 7;
}

int rowsFor(const calmath::Date& shown, const uint8_t weekStart) {
  return (columnOf(calmath::weekday(shown), weekStart) + calmath::daysInMonth(shown.year, shown.month) + 6) / 7;
}

void drawChevron(const GfxRenderer& renderer, const int x, const int y, const bool pointsLeft) {
  const int tip = pointsLeft ? x - 5 : x + 5;
  const int tail = pointsLeft ? x + 5 : x - 5;
  renderer.drawLine(tail, y - 10, tip, y, 3, true);
  renderer.drawLine(tip, y, tail, y + 10, 3, true);
}

void drawTime(const GfxRenderer& renderer, const Face face, const int top, const calmath::DateTime& time) {
  char text[8];
  const bool twelveHour = SETTINGS.clockFormat == 1;
  if (twelveHour) {
    snprintf(text, sizeof(text), "%d:%02d", time.hour % 12 == 0 ? 12 : time.hour % 12, time.minute);
  } else {
    snprintf(text, sizeof(text), "%02d:%02d", time.hour, time.minute);
  }
  const int width = renderer.getScreenWidth();
  const bool large = face == Digital;
  const gameui::DigitSet& digits = large ? kLargeDigits : kMediumDigits;
  const int gap = large ? 8 : 6;
  const int y = large ? top + 8 : top - 6;
  gameui::digitText(renderer, digits, text, width / 2, y, gap);
  if (twelveHour) {
    const int right = width / 2 + gameui::digitTextWidth(digits, text, gap) / 2;
    renderer.drawText(UI_10_FONT_ID, std::min(right + 6, width - 44), y + digits.rows - 22,
                      time.hour < 12 ? "AM" : "PM", true, EpdFontFamily::BOLD);
  }
}

void drawAnalog(const GfxRenderer& renderer, const int top, const calmath::DateTime& time) {
  const int cx = renderer.getScreenWidth() / 2;
  const int cy = top + 118;
  constexpr int radius = 128;
  constexpr float kPi = 3.14159265f;
  gameui::ring(renderer, cx, cy, radius, 4, true);
  for (int mark = 0; mark < 12; ++mark) {
    const float angle = static_cast<float>(mark) * kPi / 6.0f;
    const bool quarter = mark % 3 == 0;
    const float inner = static_cast<float>(radius - (quarter ? 22 : 15));
    const float outer = static_cast<float>(radius - 8);
    renderer.drawLine(cx + static_cast<int>(lroundf(inner * sinf(angle))),
                      cy - static_cast<int>(lroundf(inner * cosf(angle))),
                      cx + static_cast<int>(lroundf(outer * sinf(angle))),
                      cy - static_cast<int>(lroundf(outer * cosf(angle))), quarter ? 5 : 3, true);
  }
  const float hourAngle = (static_cast<float>(time.hour % 12) + static_cast<float>(time.minute) / 60.0f) * kPi / 6.0f;
  const float minuteAngle = static_cast<float>(time.minute) * kPi / 30.0f;
  const struct {
    float angle;
    float length;
    int width;
  } hands[2] = {{hourAngle, 68.0f, 9}, {minuteAngle, 100.0f, 5}};
  for (const auto& hand : hands) {
    renderer.drawLine(cx - static_cast<int>(lroundf(14.0f * sinf(hand.angle))),
                      cy + static_cast<int>(lroundf(14.0f * cosf(hand.angle))),
                      cx + static_cast<int>(lroundf(hand.length * sinf(hand.angle))),
                      cy - static_cast<int>(lroundf(hand.length * cosf(hand.angle))), hand.width, true);
  }
  gameui::disc(renderer, cx, cy, 9, true);
}

void persianDateText(const calmath::Date& date, char* out, const size_t size) {
  const calmath::Date persian = calmath::toPersian(date);
  snprintf(out, size, "%d %s %d", static_cast<int>(persian.day), calmath::persianMonthName(persian.month),
           static_cast<int>(persian.year));
}

// The weekday names above the columns, then the days of the month `shown`.
void drawMonth(const GfxRenderer& renderer, const Options& options, const bool secondNumbers, const int weekdayY,
               const int gridY, const int rowHeight, const calmath::Date& shown, const calmath::Date& today) {
  const int width = renderer.getScreenWidth();
  const int cellWidth = (width - 2 * kGridMargin) / 7;
  const int left = (width - 7 * cellWidth) / 2;
  for (int weekday = 0; weekday < 7; ++weekday) {
    char shortName[12];
    leadingCharacters(weekdayName(weekday), 2, shortName, sizeof(shortName));
    gameui::centredText(renderer, SMALL_FONT_ID,
                        {left + columnOf(weekday, options.weekStart) * cellWidth, weekdayY, cellWidth, 24}, shortName,
                        true, EpdFontFamily::BOLD);
  }

  const calmath::Date first = calmath::addDays(shown, -columnOf(calmath::weekday(shown), options.weekStart));
  const int rows = rowsFor(shown, options.weekStart);
  for (int i = 0; i < rows * 7; ++i) {
    const calmath::Date day = calmath::addDays(first, i);
    const gameui::Box cell{left + (i % 7) * cellWidth, gridY + (i / 7) * rowHeight, cellWidth, rowHeight};
    const bool isToday = day == today;
    const bool inMonth = day.month == shown.month;
    char number[4];
    snprintf(number, sizeof(number), "%d", static_cast<int>(day.day));
    if (secondNumbers) {
      if (isToday) renderer.fillRoundedRect(cell.x + 3, cell.y + 3, cell.w - 6, cell.h - 6, 10, Color::Black);
      gameui::centredText(renderer, UI_12_FONT_ID, {cell.x, cell.y + 8, cell.w, 30}, number, !isToday,
                          inMonth ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      snprintf(number, sizeof(number), "%d", static_cast<int>(calmath::toPersian(day).day));
      gameui::centredText(renderer, SMALL_FONT_ID, {cell.x, cell.y + cell.h - 32, cell.w, 22}, number, !isToday);
    } else {
      if (isToday) {
        const int side = std::min(cell.w, cell.h) - 8;
        renderer.fillRoundedRect(cell.x + (cell.w - side) / 2, cell.y + (cell.h - side) / 2, side, side, 12,
                                 Color::Black);
      }
      gameui::centredText(renderer, UI_12_FONT_ID, cell, number, !isToday,
                          isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
    }
    if (!inMonth && !isToday) gameui::fade(renderer, cell);
  }
}

void drawWeekNumber(const GfxRenderer& renderer, const int y, const calmath::Date& today) {
  if (y + 24 > renderer.getScreenHeight()) return;
  char week[32];
  snprintf(week, sizeof(week), tr(STR_CLOCK_WEEK), calmath::isoWeek(today));
  gameui::centredText(renderer, SMALL_FONT_ID, {0, y, renderer.getScreenWidth(), 24}, week);
}

}  // namespace

void loadOptions(Options& options) {
  uint8_t bytes[8];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return;
  options.face = bytes[4] < kFaceCount ? bytes[4] : Digital;
  options.persianDate = bytes[5] != 0;
  options.weekStart = bytes[6] < kWeekStartCount ? bytes[6] : Monday;
  options.weekNumber = bytes[7] != 0;
}

void saveOptions(const Options& options) {
  const uint8_t bytes[8] = {kFileMagic[0],
                            kFileMagic[1],
                            kFileMagic[2],
                            kFileMagic[3],
                            options.face,
                            static_cast<uint8_t>(options.persianDate ? 1 : 0),
                            options.weekStart,
                            static_cast<uint8_t>(options.weekNumber ? 1 : 0)};
  gameui::writeSave(kSavePath, bytes, sizeof(bytes));
}

const char* monthName(const int month) { return month >= 1 && month <= 12 ? I18N.get(kMonthNames[month - 1]) : ""; }

const char* weekdayName(const int weekday) {
  return weekday >= 0 && weekday <= 6 ? I18N.get(kWeekdayNames[weekday]) : "";
}

bool now(calmath::DateTime& out) {
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!halClock.isAvailable() || !halClock.getDateTime(year, month, day, hour, minute) || year < kEarliestYear) {
    return false;
  }
  // The clock chip keeps UTC; the offset is stored in quarter hours from -12:00.
  const int offsetMinutes = (static_cast<int>(localClockOffsetQ()) - 48) * 15;
  out = calmath::localTime(calmath::Date{static_cast<int16_t>(year), month, day}, hour, minute, offsetMinutes);
  return true;
}

Layout appLayout(const Options& options, const int top, const calmath::Date& shown) {
  Layout layout;
  // A month that starts late in the week and is long enough needs a sixth row.
  layout.rows = rowsFor(shown, options.weekStart);
  // Without the Persian line the month moves up a little.
  const int lift = options.persianDate ? 0 : 18;
  switch (options.shownFace()) {
    case Analog:
      layout.dateY = top + 264;
      layout.monthY = top + 342 - lift;
      layout.weekdayY = top + 384 - lift;
      layout.gridY = top + 412 - lift;
      layout.rowHeight = layout.rows == 6 ? 46 : 50;
      break;
    case PersianDays:
      layout.dateY = top + 84;
      layout.monthY = top + 172;
      layout.weekdayY = top + 214;
      layout.gridY = top + 242;
      layout.rowHeight = layout.rows == 6 ? 70 : 78;
      break;
    default:
      layout.dateY = top + 144;
      layout.monthY = top + 240 - lift;
      layout.weekdayY = top + 288 - lift;
      layout.gridY = top + 318 - lift;
      layout.rowHeight = layout.rows == 6 ? 58 : 60;
      break;
  }
  return layout;
}

void drawApp(const GfxRenderer& renderer, const Options& options, const int top, const calmath::DateTime& time,
             const calmath::Date& shown, const bool showsCurrentMonth) {
  const int width = renderer.getScreenWidth();
  const Face face = options.shownFace();
  const Layout layout = appLayout(options, top, shown);
  if (face == Analog) {
    drawAnalog(renderer, top, time);
  } else {
    drawTime(renderer, face, top, time);
  }

  char line[96];
  snprintf(line, sizeof(line), tr(STR_CLOCK_DATE_FORMAT), weekdayName(calmath::weekday(time.date)),
           static_cast<int>(time.date.day), monthName(time.date.month), static_cast<int>(time.date.year));
  gameui::fittedText(renderer, {12, layout.dateY, width - 24, 34}, line);
  if (options.persianDate) {
    persianDateText(time.date, line, sizeof(line));
    gameui::centredText(renderer, UI_10_FONT_ID, {0, layout.dateY + 34, width, 28}, line);
  }
  if (face != Analog) renderer.drawLine(40, layout.monthY - 14, width - 40, layout.monthY - 14, true);

  // The month's name between the arrows that turn the months.
  char title[48];
  snprintf(title, sizeof(title), "%s %d", monthName(shown.month), static_cast<int>(shown.year));
  drawChevron(renderer, 24, layout.monthY + 17, true);
  drawChevron(renderer, width - 24, layout.monthY + 17, false);
  if (face != PersianDays) {
    gameui::centredText(renderer, UI_12_FONT_ID, {0, layout.monthY, width, 34}, title, true, EpdFontFamily::BOLD);
  } else {
    // The Persian months this month runs through, on the right.
    const gameui::Box row{48, layout.monthY, width - 96, 34};
    renderer.drawText(UI_12_FONT_ID, row.x, gameui::centredTextY(renderer, UI_12_FONT_ID, row), title, true,
                      EpdFontFamily::BOLD);
    calmath::Date last = shown;
    last.day = static_cast<uint8_t>(calmath::daysInMonth(shown.year, shown.month));
    const calmath::Date from = calmath::toPersian(shown);
    const calmath::Date to = calmath::toPersian(last);
    char persian[64];
    if (from.month == to.month) {
      snprintf(persian, sizeof(persian), "%s %d", calmath::persianMonthName(from.month), static_cast<int>(to.year));
    } else {
      snprintf(persian, sizeof(persian), "%s - %s %d", calmath::persianMonthName(from.month),
               calmath::persianMonthName(to.month), static_cast<int>(to.year));
    }
    const int fontId = renderer.getTextWidth(UI_10_FONT_ID, persian) <= row.w / 2 + 20 ? UI_10_FONT_ID : SMALL_FONT_ID;
    renderer.drawText(fontId, row.x + row.w - renderer.getTextWidth(fontId, persian),
                      gameui::centredTextY(renderer, fontId, row), persian);
  }

  drawMonth(renderer, options, face == PersianDays, layout.weekdayY, layout.gridY, layout.rowHeight, shown, time.date);
  if (options.weekNumber && face != Analog && showsCurrentMonth) {
    drawWeekNumber(renderer, layout.gridY + layout.rows * layout.rowHeight + 6, time.date);
  }
}

void drawSleepCalendar(const GfxRenderer& renderer, const Options& options, const calmath::DateTime& time) {
  const int width = renderer.getScreenWidth();
  gameui::centredText(renderer, UI_12_FONT_ID, {0, 44, width, 34}, weekdayName(calmath::weekday(time.date)), true,
                      EpdFontFamily::BOLD);
  char text[96];
  snprintf(text, sizeof(text), "%d", static_cast<int>(time.date.day));
  gameui::digitText(renderer, kLargeDigits, text, width / 2, 104, 8);
  snprintf(text, sizeof(text), "%s %d", monthName(time.date.month), static_cast<int>(time.date.year));
  gameui::centredText(renderer, UI_12_FONT_ID, {0, 246, width, 34}, text, true, EpdFontFamily::BOLD);
  int ruleY = 300;
  if (options.persianDate) {
    persianDateText(time.date, text, sizeof(text));
    gameui::centredText(renderer, UI_10_FONT_ID, {0, 282, width, 28}, text);
    ruleY = 326;
  }
  renderer.drawLine(40, ruleY, width - 40, ruleY, true);

  calmath::Date shown = time.date;
  shown.day = 1;
  constexpr int rowHeight = 62;
  drawMonth(renderer, options, false, ruleY + 20, ruleY + 52, rowHeight, shown, time.date);
  if (options.weekNumber) {
    drawWeekNumber(renderer, ruleY + 52 + rowsFor(shown, options.weekStart) * rowHeight + 10, time.date);
  }
}

}  // namespace clockface
