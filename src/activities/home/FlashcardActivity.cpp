#include "FlashcardActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "components/SdFontPrewarm.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr int kCardMargin = 16;
constexpr int kCardRadius = 12;
constexpr int kButtonHeight = 56;
constexpr int kButtonGap = 12;

// Draw text wrapped to `width`, honouring line breaks; stops at maxY.
int drawParagraphs(const GfxRenderer& renderer, const int fontId, const std::string& text, const int x, int y,
                   const int width, const int maxY, const EpdFontFamily::Style style, const bool centered) {
  const int lineHeight = renderer.getLineHeight(fontId);
  size_t start = 0;
  while (start <= text.size() && y + lineHeight <= maxY) {
    const size_t nl = text.find('\n', start);
    const std::string paragraph = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    const int maxLines = std::max(1, (maxY - y) / lineHeight);
    for (const auto& line : renderer.wrappedText(fontId, paragraph.c_str(), width, maxLines, style)) {
      const int lineX = centered ? x + (width - renderer.getTextWidth(fontId, line.c_str(), style)) / 2 : x;
      renderer.drawText(fontId, lineX, y, line.c_str(), true, style);
      y += lineHeight;
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return y;
}

void drawButton(const GfxRenderer& renderer, const int x, const int y, const int w, const int h, const char* label,
                const bool filled) {
  if (filled) {
    renderer.fillRoundedRect(x, y, w, h, 8, Color::Black);
  } else {
    renderer.drawRoundedRect(x, y, w, h, 2, 8, true);
  }
  const int tw = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  const int ty = y + (h - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
  renderer.drawText(UI_12_FONT_ID, x + (w - tw) / 2, ty, label, !filled, EpdFontFamily::BOLD);
}

}  // namespace

FlashcardActivity::FlashcardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     const uint16_t categoryId)
    : Activity("Flashcards", renderer, mappedInput), categoryId(categoryId) {}

int FlashcardActivity::textFont() const {
  const int reader = SETTINGS.getReaderFontId();
  return renderer.getFontMap().count(reader) ? reader : UI_12_FONT_ID;
}

size_t FlashcardActivity::knownCount() const {
  return static_cast<size_t>(
      std::count_if(levels.begin(), levels.end(), [](const uint8_t l) { return l >= HighlightFormat::KNOWN_LEVEL; }));
}

void FlashcardActivity::onEnter() {
  Activity::onEnter();
  // Card text uses the reader font, which covers the scripts being learned.
  sdFontSystem.ensureLoaded(renderer);
  HighlightStore::CategoryInfo info;
  categoryName = HighlightStore::findCategory(categoryId, info) ? info.name : tr(STR_FLASHCARDS);
  HighlightStore::loadEntryOffsets(categoryId, offsets);
  startSession();
}

void FlashcardActivity::startSession() {
  HighlightStore::loadLevels(categoryId, offsets.size(), levels);
  order = HighlightFormat::reviewOrder(levels);
  finished = order.empty();
  showCard(0);
}

void FlashcardActivity::showCard(const size_t newPosition) {
  if (order.empty()) return;
  position = std::min(newPosition, order.size() - 1);
  flipped = false;
  card = HighlightFormat::Entry{};
  HighlightStore::readEntry(categoryId, offsets[order[position]], card);
  requestUpdate();
}

void FlashcardActivity::answer(const bool knewIt) {
  const uint16_t index = order[position];
  const uint8_t next = HighlightFormat::levelAfterAnswer(levels[index], knewIt);
  if (!HighlightStore::setLevel(categoryId, index, next)) LOG_ERR("FLASH", "Could not save card level");
  levels[index] = next;
  if (position + 1 >= order.size()) {
    finished = true;
    requestUpdate();
    return;
  }
  showCard(position + 1);
}

void FlashcardActivity::layout() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int top = metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) + metrics.verticalSpacing;
  const int bottom = height - metrics.buttonHintsHeight - metrics.verticalSpacing;
  const int buttonsY = bottom - kButtonHeight;
  cardRect = {kCardMargin, top, width - 2 * kCardMargin, buttonsY - kButtonGap - top};
  const int half = (width - 2 * kCardMargin - kButtonGap) / 2;
  againRect = {kCardMargin, buttonsY, half, kButtonHeight};
  knowRect = {kCardMargin + half + kButtonGap, buttonsY, half, kButtonHeight};
  restartRect = {width / 4, buttonsY, width / 2, kButtonHeight};
}

void FlashcardActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  int tx = 0;
  int ty = 0;
  const bool tapped = mappedInput.wasScreenTapped(tx, ty);

  if (finished) {
    if ((tapped && restartRect.contains(tx, ty)) || mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (!order.empty()) startSession();
    }
    return;
  }

  if (tapped) {
    if (flipped && againRect.contains(tx, ty)) {
      answer(false);
    } else if (flipped && knowRect.contains(tx, ty)) {
      answer(true);
    } else if (cardRect.contains(tx, ty)) {
      flipped = !flipped;
      requestUpdate();
    }
    return;
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left && position + 1 < order.size()) {
    showCard(position + 1);
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right && position > 0) {
    showCard(position - 1);
    return;
  }

  buttonNavigator.onNextRelease([this] {
    if (position + 1 < order.size()) showCard(position + 1);
  });
  buttonNavigator.onPreviousRelease([this] {
    if (position > 0) showCard(position - 1);
  });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    flipped = !flipped;
    requestUpdate();
  }
}

void FlashcardActivity::render(RenderLock&&) {
  layout();
  int font = textFont();
  if (!finished && renderer.isSdCardFont(font)) {
    // Widths for wrapping come from the font's advance table.
    renderer.ensureSdCardFontReady(font, card.text.c_str(), 0x03);  // regular + bold
    renderer.ensureSdCardFontReady(font, card.meaning.c_str(), 0x01);
    if (!card.context.empty()) renderer.ensureSdCardFontReady(font, card.context.c_str(), 0x04);  // italic
  }
  if (!drawWithSdFontPrewarm(renderer, font, [&] { drawScreen(font); })) {
    LOG_ERR("FLASH", "SD-font prewarm failed; drawing the card with the UI font");
    font = UI_12_FONT_ID;
    renderer.clearScreen();
    drawScreen(font);
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void FlashcardActivity::drawScreen(const int font) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  char title[96];
  if (finished || order.empty()) {
    snprintf(title, sizeof(title), "%s", categoryName.c_str());
  } else {
    snprintf(title, sizeof(title), "%s  %u/%u", categoryName.c_str(), static_cast<unsigned>(position + 1),
             static_cast<unsigned>(order.size()));
  }
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(),
                    TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, title, true);
  } else {
    GUI.drawHeader(renderer, header, title);
  }

  const int pad = 18;
  const int innerX = cardRect.x + pad;
  const int innerW = cardRect.w - 2 * pad;
  char summary[64];
  snprintf(summary, sizeof(summary), "%u/%u %s", static_cast<unsigned>(knownCount()),
           static_cast<unsigned>(levels.size()), tr(STR_KNOWN));

  if (finished) {
    const int midY = cardRect.y + cardRect.h / 3;
    renderer.drawCenteredText(UI_12_FONT_ID, midY, tr(STR_FLASHCARDS_DONE), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, midY + renderer.getLineHeight(UI_12_FONT_ID) * 2, summary);
    if (!order.empty()) drawButton(renderer, restartRect.x, restartRect.y, restartRect.w, restartRect.h,
                                   tr(STR_START_OVER), true);
  } else {
    renderer.drawRoundedRect(cardRect.x, cardRect.y, cardRect.w, cardRect.h, 2, kCardRadius, true);
    const int maxY = cardRect.y + cardRect.h - pad;
    if (!flipped) {
      // Front: word centred in the upper part, its sentence beneath.
      int y = cardRect.y + cardRect.h / 3 - renderer.getLineHeight(font);
      y = drawParagraphs(renderer, font, card.text, innerX, y, innerW, maxY, EpdFontFamily::BOLD, true);
      if (!card.context.empty()) {
        y += renderer.getLineHeight(font);
        drawParagraphs(renderer, font, card.context, innerX, y, innerW, maxY - renderer.getLineHeight(UI_10_FONT_ID),
                       EpdFontFamily::ITALIC, true);
      }
      const char* hint = tr(STR_TAP_TO_FLIP);
      const int hintY = cardRect.y + cardRect.h - pad - renderer.getLineHeight(UI_10_FONT_ID);
      renderer.drawText(UI_10_FONT_ID, cardRect.x + (cardRect.w - renderer.getTextWidth(UI_10_FONT_ID, hint)) / 2,
                        hintY, hint);
    } else {
      int y = cardRect.y + pad;
      y = drawParagraphs(renderer, font, card.text, innerX, y, innerW, maxY, EpdFontFamily::BOLD, false);
      renderer.drawLine(innerX, y + 4, innerX + innerW, y + 4, true);
      y += 12;
      const std::string& meaning = card.meaning.empty() ? std::string(tr(STR_NO_MEANING)) : card.meaning;
      drawParagraphs(renderer, font, meaning, innerX, y, innerW, maxY, EpdFontFamily::REGULAR, false);
      drawButton(renderer, againRect.x, againRect.y, againRect.w, againRect.h, tr(STR_AGAIN), false);
      drawButton(renderer, knowRect.x, knowRect.y, knowRect.w, knowRect.h, tr(STR_KNOW_IT), true);
    }
  }

  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
