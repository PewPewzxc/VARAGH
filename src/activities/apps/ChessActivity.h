#pragma once

#include <memory>

#include "GameThinker.h"
#include "GameUi.h"
#include "activities/Activity.h"
#include "games/ChessGame.h"

// Touch chess against the computer or between two people on one device. Tap a
// piece and dots show where it may go; tap one of them to move. The computer
// thinks on its own task with a time limit, so Back always answers. The game
// is saved after every move.
class ChessActivity final : public Activity {
 public:
  ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~ChessActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // The computer's move is worked out at full processor speed.
  bool skipLoopDelay() override { return thinking; }
  bool preventAutoSleep() override { return thinking; }

  static constexpr char kSavePath[] = "/.crosspoint/games/chess.bin";
  // A short line about the saved game for the Apps menu; empty when there is none.
  static void statusLine(char* out, size_t size);

 private:
  enum class Panel : uint8_t { Board, NewGame, Promotion };
  enum Opponent : uint8_t { Computer = 0, TwoPlayers = 1 };
  enum Tool { Undo = 0, Hint = 1, Flip = 2, kToolCount = 3 };

  struct Settings {
    uint8_t opponent = Computer;
    uint8_t level = 1;  // 0 easy, 1 medium, 2 hard
    uint8_t human = chess::White;
    uint8_t flipped = 0;  // black at the bottom
  };

  struct Job {
    chess::Position root;
    uint32_t hashes[chess::Game::kMaxPlies + 1];
    int hashCount = 0;
    chess::SearchOptions options;
    chess::SearchResult result;
  };

  static constexpr size_t kFileBytes = 8 + chess::Game::kSaveBytes;
  static void think(void* self);

  void layout();
  void save();
  bool load();
  void startGame();
  bool humanToMove() const;
  bool computerToMove() const;
  void select(int square);
  void tapSquare(int square);
  void afterMove();
  void startThinking(bool forHint);
  void finishThinking();
  void undoMove();
  gameui::Box squareBox(int square) const;
  int squareAt(int x, int y) const;
  const char* statusText() const;
  void drawBoard() const;
  void drawStrip(int y, chess::Colour side) const;
  void drawNewGamePanel() const;
  void drawPromotionPanel() const;

  std::unique_ptr<chess::Game> game;
  std::unique_ptr<Job> job;
  GameThinker thinker;
  Settings settings;
  Settings draft;  // being chosen in the New game panel
  Panel panel = Panel::Board;
  bool thinking = false;
  bool hinting = false;
  bool dirty = false;
  int selected = -1;
  uint8_t targets[32] = {};
  int targetCount = 0;
  int hintFrom = -1;
  int hintTo = -1;
  int promotionFrom = -1;
  int promotionTo = -1;

  gameui::HoldButton newHold;
  gameui::Box board;
  gameui::Box tools[kToolCount];
  gameui::Box newGameBox;  // shown in place of the tools once a game is over
  gameui::Box panelBox;
  gameui::Box opponentBoxes[2];
  gameui::Box levelBoxes[3];
  gameui::Box colourBoxes[2];
  gameui::Box cancelBox;
  gameui::Box startBox;
  gameui::Box promotionBoxes[4];
  int topStripY = 0;
  int bottomStripY = 0;
  int statusY = 0;
};
