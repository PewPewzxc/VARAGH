#include "ClockActivity.h"

#include <Arduino.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {

constexpr uint32_t kCheckEveryMs = 2000;

}  // namespace

ClockActivity::ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Clock", renderer, mappedInput) {}

void ClockActivity::statusLine(char* out, const size_t size) {
  calmath::DateTime time;
  if (clockface::now(time)) {
    snprintf(out, size, "%s", clockface::weekdayName(calmath::weekday(time.date)));
  } else {
    out[0] = '\0';
  }
}

void ClockActivity::onEnter() {
  Activity::onEnter();
  clockface::loadOptions(options);
  monthOffset = 0;
  clockSet = clockface::now(current);
  lastCheckMs = millis();
  requestUpdate();
}

calmath::Date ClockActivity::shownMonth() const {
  calmath::Date first = current.date;
  first.day = 1;
  return calmath::addMonths(first, monthOffset);
}

void ClockActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  // The minute, the date or the clock itself may have changed.
  if (millis() - lastCheckMs >= kCheckEveryMs) {
    lastCheckMs = millis();
    calmath::DateTime time;
    const bool set = clockface::now(time);
    if (set != clockSet || (set && (time.minute != current.minute || time.hour != current.hour ||
                                    !(time.date == current.date)))) {
      clockSet = set;
      if (set) current = time;
      requestUpdate();
    }
  }
  if (!clockSet) return;

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right) {
    monthOffset += swipe == MappedInputManager::SwipeDir::Left ? 1 : -1;
    requestUpdate();
    return;
  }

  int tx = 0;
  int ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return;
  const clockface::Layout layout =
      clockface::appLayout(options, gameui::headerBottom(renderer, mappedInput), shownMonth());
  const int width = renderer.getScreenWidth();
  if (ty < layout.monthY - 6) {
    // The clock itself: the next face. The one with Persian days is skipped
    // while the Persian date is switched off.
    do {
      options.face = static_cast<uint8_t>((options.face + 1) % clockface::kFaceCount);
    } while (options.face == clockface::PersianDays && !options.persianDate);
    clockface::saveOptions(options);
    requestUpdate();
  } else if (ty < layout.weekdayY) {
    // The month's row: back, today, forward.
    if (tx < width / 4) {
      --monthOffset;
    } else if (tx >= width - width / 4) {
      ++monthOffset;
    } else {
      monthOffset = 0;
    }
    requestUpdate();
  }
}

void ClockActivity::render(RenderLock&&) {
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, "");
  const int width = renderer.getScreenWidth();
  if (!clockSet) {
    int y = renderer.getScreenHeight() / 3;
    gameui::centredText(renderer, UI_12_FONT_ID, {0, y, width, 34}, tr(STR_CLOCK_NOT_SET), true, EpdFontFamily::BOLD);
    y += 48;
    for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, tr(STR_CLOCK_NOT_SET_HINT), width - 60, 4)) {
      gameui::centredText(renderer, UI_10_FONT_ID, {0, y, width, 28}, line.c_str());
      y += 30;
    }
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  clockface::drawApp(renderer, options, gameui::headerBottom(renderer, mappedInput), current, shownMonth(),
                     monthOffset == 0);
  // Fast repaints leave a trace of the old digits; every ten minutes the
  // screen is repainted cleanly instead.
  const int minuteOfDay = current.hour * 60 + current.minute;
  if (current.minute % 10 == 0 && minuteOfDay != lastCleanMinute) {
    lastCleanMinute = minuteOfDay;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}
