#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/SwipeRowActions.h"
#include "highlights/HighlightStore.h"
#include "util/ButtonNavigator.h"

// Home > Highlights: the user's categories (Favorite Lines, Flashcards, and
// any they create), "By Book" (the per-book bookmarks and highlights screen)
// and "+ New Category". Swipe a category left to delete it, long-press to
// rename it.
//
// Picker mode is the same list for "Add to...": tapping a category returns its
// id as IntervalResult{id}; "+ New Category" creates one and returns it.
class HighlightsHubActivity final : public Activity {
 public:
  HighlightsHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool pickerMode = false);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  using UiApp = freeink::ui::FreeInkApp<24, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;

  enum class RowKind : uint8_t { Category, ByBook, NewCategory };

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void reload();
  int rowCount() const;
  RowKind kindOf(int row) const;
  void activateRow(int row);
  void promptNewCategory();
  void promptRename(int row);
  void moveSelection(int index);

  const bool pickerMode;
  std::vector<HighlightStore::CategoryInfo> categories;
  std::vector<std::string> values;
  std::vector<freeink::ui::ListItem> items;
  int selectedIndex = 0;
  int topIndex = 0;
  int visibleRows = 1;

  ButtonNavigator buttonNavigator;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  SwipeRowActions swipeActions;
};
