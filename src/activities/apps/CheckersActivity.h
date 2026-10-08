#pragma once

#include <memory>

#include "GameThinker.h"
#include "GameUi.h"
#include "activities/Activity.h"
#include "games/CheckersGame.h"

// Touch checkers (American / English rules) against the computer or between
// two people on one device. Tap a piece and dots show where it may go; a jump
// that can be continued keeps the piece selected until it is finished. The
// computer thinks on its own task with a time limit. The game is saved after
// every step.
class CheckersActivity final : public Activity {
 public:
  CheckersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~CheckersActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return thinking; }
  bool preventAutoSleep() override { return thinking; }

  static constexpr char kSavePath[] = "/.crosspoint/games/checkers.bin";
  // A short line about the saved game for the Apps menu; empty when there is none.
  static void statusLine(char* out, size_t size);

 private:
  enum class Panel : uint8_t { Board, NewGame };
  enum Opponent : uint8_t { Computer = 0, TwoPlayers = 1 };
  enum Tool { Undo = 0, Hint = 1, kToolCount = 2 };

  struct Settings {
    uint8_t opponent = Computer;
    uint8_t level = 1;  // 0 easy, 1 medium, 2 hard
    uint8_t human = checkers::Dark;
  };

  struct Job {
    checkers::Position root;
    checkers::SearchOptions options;
    checkers::SearchResult result;
  };

  static constexpr size_t kFileBytes = 8 + checkers::Game::kSaveBytes;
  static void think(void* self);

  void layout();
  void save();
  bool load();
  void startGame();
  bool flipped() const;
  bool humanToMove() const;
  bool computerToMove() const;
  void select(int square);
  void tapSquare(int square);
  void afterStep();
  void startThinking(bool forHint);
  void finishThinking();
  void undoMove();
  gameui::Box squareBox(int square) const;
  int squareAt(int x, int y) const;
  const char* statusText() const;
  void drawBoard() const;
  void drawPiece(const gameui::Box& box, uint8_t content) const;
  void drawStrip(int y, checkers::Side side) const;
  void drawNewGamePanel() const;

  std::unique_ptr<checkers::Game> game;
  std::unique_ptr<Job> job;
  GameThinker thinker;
  Settings settings;
  Settings draft;
  Panel panel = Panel::Board;
  bool thinking = false;
  bool hinting = false;
  bool dirty = false;
  int selected = -1;
  uint8_t targets[8] = {};
  int targetCount = 0;
  int hintFrom = -1;
  int hintTo = -1;

  gameui::HoldButton newHold;
  gameui::Box board;
  gameui::Box tools[kToolCount];
  gameui::Box newGameBox;
  gameui::Box panelBox;
  gameui::Box opponentBoxes[2];
  gameui::Box levelBoxes[3];
  gameui::Box sideBoxes[2];
  gameui::Box cancelBox;
  gameui::Box startBox;
  int topStripY = 0;
  int bottomStripY = 0;
  int statusY = 0;
};
