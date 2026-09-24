#include "HighlightCategoryActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "FlashcardActivity.h"
#include "MappedInputManager.h"
#include "SdCardFontSystem.h"
#include "components/SdFontPrewarm.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

// First line of a multi-line meaning, for a list subtitle.
std::string firstLine(const std::string& text) {
  const size_t nl = text.find('\n');
  return nl == std::string::npos ? text : text.substr(0, nl);
}

// Draw `text` wrapped to `width`, honouring its line breaks. Returns the y
// after the last line, or stops early at maxY.
int drawParagraphs(const GfxRenderer& renderer, const int fontId, const std::string& text, const int x, int y,
                   const int width, const int maxY, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int lineHeight = renderer.getLineHeight(fontId);
  size_t start = 0;
  while (start <= text.size() && y + lineHeight <= maxY) {
    const size_t nl = text.find('\n', start);
    const std::string paragraph = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    const int maxLines = std::max(1, (maxY - y) / lineHeight);
    for (const auto& line : renderer.wrappedText(fontId, paragraph.c_str(), width, maxLines, style)) {
      renderer.drawText(fontId, x, y, line.c_str(), true, style);
      y += lineHeight;
    }
    if (paragraph.empty()) y += lineHeight / 2;
    if (nl == std::string::npos) break;
    start = nl + 1;
  }
  return y;
}

constexpr int kPracticeHeight = 56;

}  // namespace

HighlightCategoryActivity::HighlightCategoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const uint16_t categoryId)
    : Activity("HighlightCategory", renderer, mappedInput),
      categoryId(categoryId),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void HighlightCategoryActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  detailMode = false;
  uiReady = false;
  swipeActions.clear();
  // Entry text is drawn in the reader font (German, Persian, IPA coverage).
  sdFontSystem.ensureLoaded(renderer);
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &HighlightCategoryActivity::onRowEvent, this);
  app.setScreen(&HighlightCategoryActivity::listScreen, this);
  reload();
}

void HighlightCategoryActivity::reload() {
  {
    RenderLock lock(*this);
    if (!HighlightStore::findCategory(categoryId, info)) info.name = tr(STR_HIGHLIGHTS_HUB);
    HighlightStore::loadEntryOffsets(categoryId, offsets);
    knownCount = 0;
    if (isFlashcards()) {
      std::vector<uint8_t> levels;
      HighlightStore::loadLevels(categoryId, offsets.size(), levels);
      knownCount = static_cast<size_t>(std::count_if(
          levels.begin(), levels.end(), [](const uint8_t l) { return l >= HighlightFormat::KNOWN_LEVEL; }));
      char buf[32];
      snprintf(buf, sizeof(buf), "%u/%u %s", static_cast<unsigned>(knownCount),
               static_cast<unsigned>(offsets.size()), tr(STR_KNOWN));
      knownSummary = buf;
    }
    if (selectedIndex >= focusCount()) selectedIndex = std::max(0, focusCount() - 1);
    topIndex = scrollListBy(topIndex, 0, visibleRows, rowCount());
  }
  requestUpdate();
}

void HighlightCategoryActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<HighlightCategoryActivity*>(user);
  if (event.value < 0 || event.value >= self->rowCount()) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  self->activateRow(event.value);
}

void HighlightCategoryActivity::startPractice() {
  if (offsets.empty()) return;
  auto cards = makeUniqueNoThrow<FlashcardActivity>(renderer, mappedInput, categoryId);
  if (!cards) {
    LOG_ERR("HLC", "OOM: FlashcardActivity");
    return;
  }
  startActivityForResult(std::move(cards), [this](const ActivityResult&) { reload(); });
}

void HighlightCategoryActivity::activateRow(const int row) {
  if (hasPracticeButton() && row == rowCount()) {
    startPractice();
    return;
  }
  const size_t entry = static_cast<size_t>(row);
  if (entry >= offsets.size() || !HighlightStore::readEntry(categoryId, offsets[entry], detailEntry)) return;
  detailMode = true;
  requestUpdate();
}

void HighlightCategoryActivity::moveSelection(const int index) {
  selectedIndex = index;
  if (!practiceFocused()) topIndex = followListSelection(selectedIndex, topIndex, visibleRows, rowCount());
  requestUpdate();
}

HighlightCategoryActivity::Rect4 HighlightCategoryActivity::practiceRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int margin = metrics.contentSidePadding;
  const int y = renderer.getScreenHeight() - metrics.buttonHintsHeight - metrics.verticalSpacing - kPracticeHeight;
  return {margin, y, renderer.getScreenWidth() - 2 * margin, kPracticeHeight};
}

void HighlightCategoryActivity::drawPracticeButton() {
  const Rect4 r = practiceRect();
  renderer.fillRoundedRect(r.x, r.y, r.w, r.h, 10, Color::Black);
  // Ring the button when side-button navigation has moved focus onto it.
  if (practiceFocused() && !mappedInput.hasTouchHardware()) {
    renderer.drawRoundedRect(r.x - 4, r.y - 4, r.w + 8, r.h + 8, 2, 13, true);
  }
  const char* label = tr(STR_STUDY_FLASHCARDS);
  const int labelW = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD);
  const int labelX = r.x + (r.w - labelW) / 2;
  renderer.drawText(UI_12_FONT_ID, labelX, r.y + (r.h - renderer.getLineHeight(UI_12_FONT_ID)) / 2, label, false,
                    EpdFontFamily::BOLD);
  // The known count sits at the right edge when it fits beside the label.
  const int summaryW = renderer.getTextWidth(UI_10_FONT_ID, knownSummary.c_str());
  const int summaryX = r.x + r.w - 16 - summaryW;
  if (summaryX > labelX + labelW + 12) {
    renderer.drawText(UI_10_FONT_ID, summaryX, r.y + (r.h - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                      knownSummary.c_str(), false);
  }
}

void HighlightCategoryActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<HighlightCategoryActivity*>(user)->buildListScreen(screen);
}

void HighlightCategoryActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int practiceBand = hasPracticeButton() ? kPracticeHeight + metrics.verticalSpacing : 0;
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       metrics.verticalSpacing),
                  0, static_cast<int16_t>(metrics.buttonHintsHeight + metrics.verticalSpacing + practiceBand), 0});
  if (rowCount() == 0) {
    screen.centeredText(tr(STR_CATEGORY_EMPTY), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 2;
  const auto rows = configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, rowCount());

  // Read just this screenful of entries from the card.
  const int drawCount = std::min({visibleRows, static_cast<int>(WINDOW), rowCount() - topIndex});
  HighlightStore::readEntries(categoryId, offsets, static_cast<size_t>(topIndex),
                              static_cast<size_t>(std::max(0, drawCount)), windowEntries);
  for (int i = 0; i < drawCount; ++i) {
    const int row = topIndex + i;
    auto& item = windowItems[static_cast<size_t>(i)];
    item = fui::ListItem{};
    item.label = "";
    item.actionValue = static_cast<int16_t>(row);
    const size_t w = static_cast<size_t>(i);
    if (w >= windowEntries.size()) continue;
    const auto& e = windowEntries[w];
    item.label = e.text.c_str();
    windowSubtitles[static_cast<size_t>(i)] = isFlashcards() ? firstLine(e.meaning) : e.source;
    if (!windowSubtitles[static_cast<size_t>(i)].empty()) item.subtitle = windowSubtitles[static_cast<size_t>(i)].c_str();
  }
  props.items = windowItems.data();
  props.count = static_cast<uint16_t>(drawCount);
  props.selectedIndex = static_cast<int16_t>(practiceFocused() ? -1 : selectedIndex - topIndex);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.topIndex = 0;
  screen.list(props);
  fui::drawListScrollIndicator(screen.target(), screen.body(), static_cast<size_t>(rowCount()), visibleRows, topIndex,
                               screen.theme().listScrollWidth, screen.theme().listScrollSide,
                               screen.theme().listScrollInset);
}

void HighlightCategoryActivity::loop() {
  const bool back = TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
                    mappedInput.wasReleased(MappedInputManager::Button::Back);
  if (detailMode) {
    int tx = 0;
    int ty = 0;
    if (back || mappedInput.wasScreenTapped(tx, ty) || mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      detailMode = false;
      requestUpdate();
    }
    return;
  }
  if (back) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  if (uiReady) {
    int swipedRow = -1;
    const auto swipe = swipeActions.handleInput(app, mappedInput, ACTION_ROW, swipedRow);
    if (swipe != SwipeRowActions::Result::None) {
      if (swipe == SwipeRowActions::Result::Delete && swipedRow >= 0) {
        HighlightStore::deleteEntry(categoryId, static_cast<size_t>(swipedRow));
        reload();
      }
      requestUpdate();
      return;
    }
    int tx = 0;
    int ty = 0;
    if (hasPracticeButton() && mappedInput.wasScreenTapped(tx, ty) && practiceRect().contains(tx, ty)) {
      startPractice();
      return;
    }
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int delta = swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows;
    const int next = scrollListBy(topIndex, delta, visibleRows, rowCount());
    if (next != topIndex) {
      topIndex = next;
      requestUpdate();
    }
    return;
  }

  const int total = focusCount();
  if (total == 0) return;
  buttonNavigator.onNextRelease([this, total] { moveSelection(ButtonNavigator::nextIndex(selectedIndex, total)); });
  buttonNavigator.onPreviousRelease(
      [this, total] { moveSelection(ButtonNavigator::previousIndex(selectedIndex, total)); });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activateRow(selectedIndex);
}

void HighlightCategoryActivity::renderDetail(const int textFont) {
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int margin = metrics.contentSidePadding;
  const int x = safe.x + margin;
  const int width = safe.width - margin * 2;
  const int maxY = safe.y + safe.height - metrics.buttonHintsHeight;
  int y = safe.y + metrics.topPadding + metrics.verticalSpacing;
  y = drawParagraphs(renderer, textFont, detailEntry.text, x, y, width, maxY, EpdFontFamily::BOLD);
  y += metrics.verticalSpacing;
  if (!detailEntry.meaning.empty()) {
    y = drawParagraphs(renderer, textFont, detailEntry.meaning, x, y, width, maxY);
    y += metrics.verticalSpacing;
  }
  if (!detailEntry.context.empty()) {
    y = drawParagraphs(renderer, textFont, detailEntry.context, x, y, width, maxY, EpdFontFamily::ITALIC);
    y += metrics.verticalSpacing;
  }
  if (!detailEntry.source.empty()) drawParagraphs(renderer, UI_10_FONT_ID, detailEntry.source, x, y, width, maxY);
  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void HighlightCategoryActivity::render(RenderLock&&) {
  if (detailMode) {
    // The reader font carries the scripts the user reads (German, Persian, IPA).
    int textFont = SETTINGS.getReaderFontId();
    if (renderer.getFontMap().count(textFont) == 0) textFont = UI_10_FONT_ID;
    if (renderer.isSdCardFont(textFont)) {
      renderer.ensureSdCardFontReady(textFont, detailEntry.text.c_str(), 0x03);
      renderer.ensureSdCardFontReady(textFont, detailEntry.meaning.c_str(), 0x01);
      if (!detailEntry.context.empty()) renderer.ensureSdCardFontReady(textFont, detailEntry.context.c_str(), 0x04);
    }
    if (!drawWithSdFontPrewarm(renderer, textFont, [&] { renderDetail(textFont); })) {
      LOG_ERR("HLC", "SD-font prewarm failed; drawing the entry with the UI font");
      renderer.clearScreen();
      renderDetail(UI_10_FONT_ID);
    }
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(),
                    TouchHeaderBackButton::height(metrics, mappedInput)};
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, info.name.c_str(), true);
  } else {
    GUI.drawHeader(renderer, header, info.name.c_str());
  }
  uiReady = false;
  app.render();
  swipeActions.draw(renderer);
  if (hasPracticeButton()) drawPracticeButton();
  uiReady = true;
  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
