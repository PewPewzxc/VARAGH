#pragma once

#include "GameUi.h"
#include "activities/Activity.h"
#include "games/TicTacToeGame.h"

// Touch Tic Tac Toe, Classic or Infinite (three marks a side, the oldest one
// leaves), against the computer or between two people on one device. A round
// is short, so a tap on the board after it starts the next one, and a tally of
// the rounds is kept. The game and the tally are saved after every move.
class TicTacToeActivity final : public Activity {
 public:
  TicTacToeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr char kSavePath[] = "/.crosspoint/games/tictactoe.bin";
  // A short line about the tally for the Apps menu; empty before the first round ends.
  static void statusLine(char* out, size_t size);

 private:
  enum class Panel : uint8_t { Board, NewGame };
  enum Opponent : uint8_t { Computer = 0, TwoPlayers = 1 };

  struct Settings {
    uint8_t kind = 0;  // tictactoe::Kind
    uint8_t opponent = Computer;
    uint8_t level = 1;  // 0 easy, 1 medium, 2 hard
    uint8_t human = tictactoe::X;
  };

  // Rounds won by the first side (you, or X between two players), by the
  // second (the computer, or O), and drawn.
  struct Tally {
    uint16_t first = 0;
    uint16_t second = 0;
    uint16_t draws = 0;
  };

  static constexpr size_t kFileBytes = 16 + tictactoe::Game::kSaveBytes;
  static bool readFile(Settings& settings, Tally& tally, tictactoe::Game* game);

  void layout();
  void save();
  void startRound();
  bool computerToMove() const;
  void afterMove();
  const char* statusText() const;
  const char* noteText() const;
  void drawMark(const gameui::Box& box, tictactoe::Mark mark) const;
  void drawBoard() const;
  void drawStrip(int y, tictactoe::Mark mark) const;
  void drawTally() const;
  void drawNewGamePanel() const;

  tictactoe::Game game;
  Settings settings;
  Settings draft;
  Tally tally;
  Panel panel = Panel::Board;
  unsigned long lastMoveMs = 0;

  gameui::HoldButton newHold;
  gameui::Box board;
  gameui::Box panelBox;
  gameui::Box kindBoxes[2];
  gameui::Box opponentBoxes[2];
  gameui::Box levelBoxes[3];
  gameui::Box sideBoxes[2];
  gameui::Box cancelBox;
  gameui::Box startBox;
  int topStripY = 0;
  int bottomStripY = 0;
  int statusY = 0;
  int tallyY = 0;
  gameui::ButtonPointer pointer;  // VARAGH: apps on button readers
};
