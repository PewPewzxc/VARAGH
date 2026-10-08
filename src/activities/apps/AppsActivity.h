#pragma once

#include <string>

#include "ClockFace.h"
#include "GameUi.h"
#include "activities/Activity.h"

// Home > Apps: a tile per built-in app with its icon, its name and a short
// line about what is going on in it. Manage (top right) lists every app with a
// switch; an app that is switched off is only hidden from the tiles - it stays
// in the firmware and costs no memory or battery while it is not open. The
// Clock's row also leads to its settings.
class AppsActivity final : public Activity {
 public:
  AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum AppId { Wordle = 0, Sudoku = 1, Chess = 2, Checkers = 3, Clock = 4, kAppCount = 5 };
  static constexpr char kSavePath[] = "/.crosspoint/games/apps.bin";

  void layout();
  void readStatus();
  void open(AppId app);
  bool hidden(int app) const { return (hiddenMask & (1u << app)) != 0; }
  void saveHidden() const;
  void drawIcon(int app, int x, int y) const;
  void drawTiles() const;
  void drawManage() const;
  void drawClockSettings() const;
  void changeClockSetting(int row);

  enum class Mode : uint8_t { Tiles, Manage, ClockSettings };
  enum ClockRow { FaceRow = 0, PersianRow = 1, WeekStartRow = 2, WeekNumberRow = 3, kClockRowCount = 4 };

  Mode mode = Mode::Tiles;
  clockface::Options clockOptions;
  gameui::Box clockSettingsBox;
  gameui::Box clockRows[kClockRowCount];
  uint8_t hiddenMask = 0;
  int shownCount = 0;
  int shown[kAppCount] = {};
  gameui::Box tiles[kAppCount];
  gameui::Box rows[kAppCount];
  gameui::Box manageBox;
  std::string status[kAppCount];
};
