#pragma once

#include <HalClock.h>

#include <string>

#include "../Activity.h"
#include "BookReadingStats.h"
#include "GlobalReadingStats.h"

class BookStatsActivity final : public Activity {
  enum class Page : uint8_t { PerBook, ThisDevice, AllDevices, EditDates };

  std::string bookTitle;
  std::string bookCachePath;
  BookReadingStats stats;
  GlobalReadingStats globalStats;
  GlobalReadingStats allDevicesStats;
  bool showAllDevicesStats = false;
  bool returnToHomeOnExit = false;
  float progressPercent = -1.0f;
  bool hasEstimatedTimeLeft = false;
  uint32_t estimatedTimeLeftSeconds = 0;
  GfxRenderer::Orientation previousOrientation = GfxRenderer::Orientation::Portrait;
  Page page = Page::PerBook;
  int selectedEditField = 0;
  bool didChangeStats = false;
  BookReadingStats dateEditStatsSnapshot;
  GlobalReadingStats dateEditGlobalStatsSnapshot;
  bool dateEditSnapshotValid = false;
  bool didChangeStatsBeforeDateEdit = false;
  bool ignoreInitialBackRelease = false;
  bool ignoreInitialConfirmRelease = false;
  bool ignoreInitialPowerRelease = false;
  // VARAGH: the book's rating, shown as stars in the title strip of its card; a
  // tap on the strip opens the rating screen. Only when the book's file is known.
  std::string ratedBookPath;
  std::string ratedBookAuthor;
  uint8_t rating = 0;

  bool hasEditableBook() const { return !bookCachePath.empty() && halClock.isAvailable(); }
  bool usesNoRtcSingleScreenLayout() const { return !halClock.isAvailable(); }
  void refreshAllDevicesStats();
  void saveStats();
  void beginDateEditing();
  void finishDateEditing(bool saveChanges);
  void cycleEditField();
  void adjustSelectedDateField(int delta);
  void applyCompletedState(bool completed);
  ReadingStatsDate defaultDateForField(bool finishedField) const;
  void clearEditedDate(bool finishedField);
  bool shouldClearDateOnAdjust(const ReadingStatsDate& date, bool finishedField, int fieldIndex, int delta) const;
  void normalizeEditedDates(const bool editedFinishedField);
  void exitStatsActivity();
  bool showPreviousStatsPage();
  bool showNextStatsPage();
  bool selectEditFieldFromTouchTarget(int touchTarget);

 public:
  BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                    const std::string& bookCachePath, const BookReadingStats& stats, float progressPercent,
                    bool hasEstimatedTimeLeft, uint32_t estimatedTimeLeftSeconds, const GlobalReadingStats& globalStats,
                    bool returnToHomeOnExit = false);
  BookStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                    const std::string& bookCachePath, const BookReadingStats& stats, float progressPercent,
                    bool hasEstimatedTimeLeft, uint32_t estimatedTimeLeftSeconds, const GlobalReadingStats& globalStats,
                    const GlobalReadingStats& allDevicesStats, bool returnToHomeOnExit = false);

  // VARAGH: names the book file, which turns the rating on (see ratedBookPath).
  void setRatedBook(std::string bookPath, std::string author) {
    ratedBookPath = std::move(bookPath);
    ratedBookAuthor = std::move(author);
  }

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
};
