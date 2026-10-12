#include "BookRatingActivity.h"

#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "fontIds.h"
#include "util/BookRatingText.h"
#include "util/BookRatings.h"

namespace {

constexpr int kClearWidth = 220;
constexpr int kClearHeight = 56;

}  // namespace

BookRatingActivity::BookRatingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath,
                                       std::string title, std::string author)
    : Activity("BookRating", renderer, mappedInput),
      bookPath(std::move(bookPath)),
      title(std::move(title)),
      author(std::move(author)) {}

void BookRatingActivity::onEnter() {
  Activity::onEnter();
  halfStars = BookRatings::get(bookPath);
  requestUpdate();
}

void BookRatingActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int top = gameui::contentTop(renderer, mappedInput);
  titleY = top + 56;
  labelY = titleY + renderer.getLineHeight(UI_12_FONT_ID) * 2 + renderer.getLineHeight(UI_10_FONT_ID) + 48;
  stars.radius = 30;
  stars.pitch = 80;
  stars.centreX = width / 2;
  stars.centreY = labelY + renderer.getLineHeight(UI_10_FONT_ID) + 20 + stars.radius;
  valueY = stars.centreY + stars.radius + 22;
  hintY = valueY + renderer.getLineHeight(UI_10_FONT_ID) + 28;
  clearBox = {(width - kClearWidth) / 2, gameui::contentBottom(renderer) - kClearHeight, kClearWidth, kClearHeight};
}

void BookRatingActivity::rate(const uint8_t value) {
  if (value == halfStars) return;
  halfStars = value;
  BookRatings::set(bookPath, halfStars, title, author);
  requestUpdate();
}

void BookRatingActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    setResult(ActivityResult{});
    finish();
    return;
  }
  // Readers with buttons: half a star down or up.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && halfStars > 0) {
    rate(static_cast<uint8_t>(halfStars - 1));
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && halfStars < bookrating::kMaxHalfStars) {
    rate(static_cast<uint8_t>(halfStars + 1));
    return;
  }

  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  int star = 0;
  bool rightHalf = false;
  if (starrating::hit(stars, x, y, star, rightHalf)) {
    rate(bookrating::afterTap(halfStars, star, rightHalf));
  } else if (halfStars != 0 && clearBox.grown(6).contains(x, y)) {
    rate(0);
  }
}

void BookRatingActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_RATING));
  const int width = renderer.getScreenWidth();

  const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, title.c_str(), width - 48, 2, EpdFontFamily::BOLD);
  int y = titleY;
  for (const auto& line : titleLines) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(UI_12_FONT_ID);
  }
  if (!author.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, y + 4,
                              renderer.truncatedText(UI_10_FONT_ID, author.c_str(), width - 48).c_str());
  }

  renderer.drawCenteredText(UI_10_FONT_ID, labelY, tr(STR_YOUR_RATING));
  starrating::draw(renderer, stars, halfStars);
  if (halfStars == 0) {
    renderer.drawCenteredText(UI_10_FONT_ID, valueY, tr(STR_NOT_RATED), true, EpdFontFamily::BOLD);
  } else {
    char value[32];
    snprintf(value, sizeof(value), tr(STR_RATING_OF_FIVE), bookrating::number(halfStars).c_str());
    renderer.drawCenteredText(UI_10_FONT_ID, valueY, value, true, EpdFontFamily::BOLD);
  }
  if (mappedInput.hasTouchHardware()) {
    for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, tr(STR_RATING_HINT), width - 64, 2)) {
      renderer.drawCenteredText(SMALL_FONT_ID, hintY, line.c_str());
      hintY += renderer.getLineHeight(SMALL_FONT_ID);
    }
    if (halfStars != 0) {
      renderer.drawRoundedRect(clearBox.x, clearBox.y, clearBox.w, clearBox.h, 2, 10, true);
      gameui::centredText(renderer, UI_10_FONT_ID, clearBox, tr(STR_RATING_CLEAR), true, EpdFontFamily::BOLD);
    }
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
