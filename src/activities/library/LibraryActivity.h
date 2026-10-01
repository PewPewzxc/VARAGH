#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>
#include <LibraryIndexFile.h>

#include <memory>
#include <string>

#include "RecentBooksStore.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "components/SwipeRowActions.h"
#include "util/ButtonNavigator.h"

class LibraryActivity final : public Activity {
 public:
  LibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool blocksGlobalInput() const override { return actionPopup.isActive(); }

 private:
  enum class Sort : uint8_t { DateAdded, Title, AuthorLast, AuthorFirst, RecentlyRead, Series, Genre };
  // Touch header controls precede the book rows; button-only navigation visits books directly.
  static constexpr int CONTROL_COUNT = 5;
  using UiApp = freeink::ui::FreeInkApp<32, 4>;
  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  ButtonNavigator buttonNavigator;
  freeink::ui::ListNav listNav;
  OptionPopup actionPopup;
  library::LibraryIndexFile index;
  Sort sort = Sort::RecentlyRead;
  bool descending = true;
  int selection = CONTROL_COUNT;
  bool showSelection = true;
  int topIndex = 0;
  int gridPageStart = 0;
  int loadedGridPageStart = -1;
  int nextGridCoverRow = -1;
  int16_t gridCoverWidth = 0;
  int16_t gridCoverHeight = 0;
  int gridProgressRow = -1;
  float gridProgress = -1.0f;
  bool uiReady = false;
  // VARAGH: swipe a list row left to reveal Delete (see swipeDelete()).
  SwipeRowActions swipeActions;
  bool initialScanPending = false;
  bool longPressFired = false;
  bool ignoreConfirmRelease = false;
  bool scanFailed = false;
  bool filterFailed = false;
  bool pendingCacheDeletedFeedback = false;
  unsigned long cacheDeletedFeedbackShowTime = 0;
  std::string query;
  // Searches and file-type filters allocate one u16 per visible source book, fallibly.
  std::unique_ptr<uint16_t[]> filtered;
  uint16_t filteredCount = 0;
  uint16_t recentRows[RecentBooksStore::MAX_RECENT_BOOKS]{};
  size_t recentCount = 0;
  // SDK rowProvider consumes the strings before asking for the next row.
  // Reuse one row instead of retaining every title/author in the library.
  RecentBook rowScratch;
  std::string groupHeading;
  std::string previousGroupScratch;
  std::string groupKeyScratch;
  std::string previousGroupKeyScratch;
  std::string seriesScratch;
  std::string genreScratch;
  std::string subtitleScratch;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onControlEvent(const freeink::ui::ActionEvent& event, void* user);
  static void provideRow(void* user, uint16_t row, freeink::ui::ListItem& item);
  void buildListScreen(UiApp::ScreenType& screen);
  void buildGrid(UiApp::ScreenType& screen);
  void loadGridPageCovers();
  bool loadGridCover(int row);
  void loadGridProgress();
  bool gridEnabled() const;
  void buildSortHeader(UiApp::ScreenType& screen);
  const char* sortLabel() const;
  library::SortOrder indexOrder() const;
  int rowCount() const;
  uint16_t ordinalForRow(int row);
  bool readBook(int row, RecentBook& book, bool fullPath = true);
  uint32_t groupForRow(int row);
  uint16_t dateGroupForRow(int row);
  bool metadataGroupForRow(int row, std::string& out);
  bool hasActiveFilter() const;
  void refreshIndexIfNeeded();
  bool rebuildIndex(bool showScanning);
  void resolveRecents();
  void applyFilter();
  void resetViewport();
  void reloadAfterBookAction();
  void openBook(int row);
  void openSortPicker(int selectedIndex = -1);
  void openMenu();
  void refreshLibrary();
  void openSearch();
  void openSettings();
  void activateControl(int control);
  // Owns the allocation-failure check and render lock for every child result.
  void openDialog(std::unique_ptr<Activity>&& activity, ActivityResultHandler handler);
  void promptDeleteBook(const RecentBook& book);
  void swipeDelete(int row);
  void promptRemoveBook(const std::string& path, const std::string& title);
  void showBookActionMenu(size_t bookIndex, bool ignoreInitialConfirmRelease = false);
};
