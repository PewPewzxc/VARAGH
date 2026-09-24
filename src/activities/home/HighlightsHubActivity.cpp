#include "HighlightsHubActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>

#include "HighlightCategoryActivity.h"
#include "MappedInputManager.h"
#include "SavedItemsHomeActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
constexpr size_t kMaxCategoryNameLength = 40;
}

HighlightsHubActivity::HighlightsHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const bool pickerMode)
    : Activity("HighlightsHub", renderer, mappedInput),
      pickerMode(pickerMode),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

void HighlightsHubActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  uiReady = false;
  swipeActions.clear();
  applySharedUiTheme(app, uiTarget);
  app.on(ACTION_ROW, &HighlightsHubActivity::onRowEvent, this);
  app.setScreen(&HighlightsHubActivity::listScreen, this);
  HighlightStore::ensureDefaults();
  reload();
  requestUpdate();
}

int HighlightsHubActivity::rowCount() const {
  return static_cast<int>(categories.size()) + (pickerMode ? 1 : 2);
}

HighlightsHubActivity::RowKind HighlightsHubActivity::kindOf(const int row) const {
  const int cats = static_cast<int>(categories.size());
  if (row < cats) return RowKind::Category;
  if (!pickerMode && row == cats) return RowKind::ByBook;
  return RowKind::NewCategory;
}

void HighlightsHubActivity::reload() {
  {
    // The render task reads these vectors while building rows.
    RenderLock lock(*this);
    categories = HighlightStore::listCategories();
    values.assign(categories.size(), std::string{});
    for (size_t i = 0; i < categories.size(); ++i) values[i] = std::to_string(categories[i].count);
    items.clear();
    items.reserve(static_cast<size_t>(rowCount()));
    for (int row = 0; row < rowCount(); ++row) {
      fui::ListItem item;
      switch (kindOf(row)) {
        case RowKind::Category: {
          const auto& c = categories[static_cast<size_t>(row)];
          item.label = c.name.c_str();
          if (c.type == HighlightFormat::CategoryType::Flashcards) item.subtitle = tr(STR_FLASHCARDS);
          item.value = values[static_cast<size_t>(row)].c_str();
          item.icon = listIconFor(UIIcon::Folder, 32);
          break;
        }
        case RowKind::ByBook:
          item.label = tr(STR_BY_BOOK);
          item.icon = listIconFor(UIIcon::Book, 32);
          break;
        case RowKind::NewCategory:
          item.label = tr(STR_NEW_CATEGORY);
          break;
      }
      item.actionValue = static_cast<int16_t>(row);
      items.push_back(item);
    }
    if (selectedIndex >= rowCount()) selectedIndex = std::max(0, rowCount() - 1);
    topIndex = scrollListBy(topIndex, 0, visibleRows, rowCount());
  }
  requestUpdate();
}

void HighlightsHubActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<HighlightsHubActivity*>(user);
  if (event.value < 0 || event.value >= self->rowCount()) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  if (event.longPress) {
    if (!self->pickerMode && self->kindOf(event.value) == RowKind::Category) self->promptRename(event.value);
    return;
  }
  self->activateRow(event.value);
}

void HighlightsHubActivity::activateRow(const int row) {
  switch (kindOf(row)) {
    case RowKind::Category: {
      const uint16_t id = categories[static_cast<size_t>(row)].id;
      if (pickerMode) {
        setResult(ActivityResult{IntervalResult{id}});
        finish();
        return;
      }
      auto category = makeUniqueNoThrow<HighlightCategoryActivity>(renderer, mappedInput, id);
      if (!category) {
        LOG_ERR("HLH", "OOM: HighlightCategoryActivity");
        return;
      }
      startActivityForResult(std::move(category), [this](const ActivityResult&) { reload(); });
      return;
    }
    case RowKind::ByBook: {
      auto byBook = makeUniqueNoThrow<SavedItemsHomeActivity>(renderer, mappedInput);
      if (!byBook) {
        LOG_ERR("HLH", "OOM: SavedItemsHomeActivity");
        return;
      }
      startActivityForResult(std::move(byBook), [this](const ActivityResult&) { reload(); });
      return;
    }
    case RowKind::NewCategory:
      promptNewCategory();
      return;
  }
}

void HighlightsHubActivity::promptNewCategory() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_CATEGORY_NAME), "",
                                                           kMaxCategoryNameLength);
  if (!keyboard) {
    LOG_ERR("HLH", "OOM: KeyboardEntryActivity");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (result.isCancelled || !text || text->text.empty()) {
      requestUpdate();
      return;
    }
    const int id = HighlightStore::createCategory(text->text, HighlightFormat::CategoryType::Notes);
    if (id < 0) {
      LOG_ERR("HLH", "Could not create category");
      reload();
      return;
    }
    if (pickerMode) {
      setResult(ActivityResult{IntervalResult{static_cast<uint32_t>(id)}});
      finish();
      return;
    }
    reload();
  });
}

void HighlightsHubActivity::promptRename(const int row) {
  const auto category = categories[static_cast<size_t>(row)];
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RENAME), category.name,
                                                           kMaxCategoryNameLength);
  if (!keyboard) {
    LOG_ERR("HLH", "OOM: KeyboardEntryActivity");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, id = category.id](const ActivityResult& result) {
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!result.isCancelled && text && !text->text.empty()) HighlightStore::renameCategory(id, text->text);
    reload();
  });
}

void HighlightsHubActivity::moveSelection(const int index) {
  selectedIndex = index;
  topIndex = followListSelection(selectedIndex, topIndex, visibleRows, rowCount());
  requestUpdate();
}

void HighlightsHubActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<HighlightsHubActivity*>(user)->buildListScreen(screen);
}

void HighlightsHubActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput) +
                                       metrics.verticalSpacing),
                  0, static_cast<int16_t>(metrics.buttonHintsHeight + metrics.verticalSpacing), 0});
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.action = ACTION_ROW;
  props.inputMask = static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress);
  props.labelText = screen.theme().bodyText;
  const auto rows = configureUiList(props, screen.theme(), screen.body(), UiListRowType::WithSubtitle);
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, rowCount());
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void HighlightsHubActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  if (uiReady) {
    if (!pickerMode) {
      int swipedRow = -1;
      const auto swipe = swipeActions.handleInput(app, mappedInput, ACTION_ROW, swipedRow);
      if (swipe != SwipeRowActions::Result::None) {
        // Only categories can be deleted; the fixed rows never show Delete.
        if (swipeActions.active() && kindOf(swipeActions.index()) != RowKind::Category) swipeActions.clear();
        if (swipe == SwipeRowActions::Result::Delete && kindOf(swipedRow) == RowKind::Category) {
          HighlightStore::deleteCategory(categories[static_cast<size_t>(swipedRow)].id);
          reload();
        }
        requestUpdate();
        return;
      }
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

  const int total = rowCount();
  buttonNavigator.onNextRelease([this, total] { moveSelection(ButtonNavigator::nextIndex(selectedIndex, total)); });
  buttonNavigator.onPreviousRelease(
      [this, total] { moveSelection(ButtonNavigator::previousIndex(selectedIndex, total)); });
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) activateRow(selectedIndex);
}

void HighlightsHubActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect header{0, metrics.topPadding, renderer.getScreenWidth(),
                    TouchHeaderBackButton::height(metrics, mappedInput)};
  const char* title = pickerMode ? tr(STR_ADD_TO) : tr(STR_HIGHLIGHTS_HUB);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, title, true);
  } else {
    GUI.drawHeader(renderer, header, title);
  }
  uiReady = false;
  app.render();
  swipeActions.draw(renderer);
  uiReady = true;
  const auto labels =
      mappedInput.mapLabels(mappedInput.withBackArrow(tr(STR_BACK)), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
