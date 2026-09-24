#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <array>
#include <atomic>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "components/SwipeRowActions.h"
#include "highlights/HighlightStore.h"
#include "util/ButtonNavigator.h"

// One highlight category. Rows are read from the SD card a screenful at a
// time (the store keeps only a 4-byte offset per entry in RAM). Flashcard
// categories show a "Practice" button under the list. Tap an entry to read it
// in full; swipe it left to delete it.
class HighlightCategoryActivity final : public Activity {
 public:
  HighlightCategoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint16_t categoryId);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  using UiApp = freeink::ui::FreeInkApp<24, 4>;
  static constexpr freeink::ui::ActionId ACTION_ROW = 1;
  static constexpr size_t WINDOW = 16;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void reload();
  bool isFlashcards() const { return info.type == HighlightFormat::CategoryType::Flashcards; }
  bool hasPracticeButton() const { return isFlashcards() && !offsets.empty(); }
  int rowCount() const { return static_cast<int>(offsets.size()); }
  // Button navigation also stops on the Practice button, after the last row.
  int focusCount() const { return rowCount() + (hasPracticeButton() ? 1 : 0); }
  bool practiceFocused() const { return hasPracticeButton() && selectedIndex == rowCount(); }
  struct Rect4 {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  };
  Rect4 practiceRect() const;
  void activateRow(int row);
  void startPractice();
  void moveSelection(int index);
  void drawPracticeButton();
  void renderDetail(int textFont);

  const uint16_t categoryId;
  HighlightStore::CategoryInfo info;
  std::vector<uint32_t> offsets;
  size_t knownCount = 0;
  std::string knownSummary;

  // Screenful of rows, rebuilt on the render task for the visible window.
  std::vector<HighlightFormat::Entry> windowEntries;
  std::array<freeink::ui::ListItem, WINDOW> windowItems{};
  std::array<std::string, WINDOW> windowSubtitles{};

  bool detailMode = false;
  HighlightFormat::Entry detailEntry;

  int selectedIndex = 0;
  int topIndex = 0;
  int visibleRows = 1;
  ButtonNavigator buttonNavigator;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};
  SwipeRowActions swipeActions;
};
