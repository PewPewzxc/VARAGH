#pragma once

#include <memory>

#include "GameThinker.h"
#include "GameUi.h"
#include "activities/Activity.h"
#include "games/GoGame.h"

// Go on 9x9, 13x13 or 19x19, against the computer or between two people on
// one device. A tap shows a faint stone on the nearest point and a second tap
// there places it (points are close together on 19x19); the buttons move the
// same faint stone and Confirm (Power) places it. Two passes end the play: tap the
// dead stones, then Count. The game is saved after every move.
class GoActivity final : public Activity {
 public:
  GoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~GoActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return thinking; }

  static constexpr char kSavePath[] = "/.crosspoint/games/go.bin";
  // A short line about the saved game for the Apps menu; empty when there is none.
  static void statusLine(char* out, size_t size);

 private:
  enum class Panel : uint8_t { Board, NewGame };
  enum Opponent : uint8_t { Computer = 0, TwoPlayers = 1 };
  enum Tool { Undo = 0, Pass = 1, Hint = 2, kToolCount = 3 };

  struct Settings {
    uint8_t opponent = Computer;
    uint8_t level = 0;  // 0 easy, 1 medium
    uint8_t human = go::Black;
    uint8_t size = 9;
  };

  struct Job {
    go::Board board;
    int level = 0;
    bool opponentPassed = false;
    uint32_t seed = 1;
    int move = go::kPass;
  };

  static constexpr size_t kFileBytes = 8 + go::Game::kSaveBytes;
  static void think(void* self);

  void layout();
  void save();
  bool load();
  void startGame();
  bool humanToMove() const;
  bool computerToMove() const;
  void tapPoint(int point);
  void placeGhost();
  void moveGhost(int step);
  void afterMove();
  void startThinking(bool forHint);
  void finishThinking();
  void undoMove();
  void passMove();
  int spacing() const;
  int pointX(int point) const;
  int pointY(int point) const;
  int pointAt(int x, int y) const;
  const char* statusText() const;
  void drawBoard() const;
  void drawStone(int cx, int cy, int radius, go::Stone stone, bool faint) const;
  void drawStrip(int y, go::Stone side) const;
  void drawNewGamePanel() const;

  std::unique_ptr<go::Game> game;
  std::unique_ptr<Job> job;
  GameThinker thinker;
  Settings settings;
  Settings draft;
  Panel panel = Panel::Board;
  bool thinking = false;
  bool hinting = false;
  bool dirty = false;
  int ghost = -1;  // the faint stone: where a second tap (or Confirm) places

  gameui::HoldButton newHold;
  gameui::Box board;
  gameui::Box tools[kToolCount];
  gameui::Box newGameBox;
  gameui::Box panelBox;
  gameui::Box opponentBoxes[2];
  gameui::Box levelBoxes[2];
  gameui::Box sizeBoxes[3];
  gameui::Box sideBoxes[2];
  gameui::Box cancelBox;
  gameui::Box startBox;
  int topStripY = 0;
  int bottomStripY = 0;
  int statusY = 0;
};
