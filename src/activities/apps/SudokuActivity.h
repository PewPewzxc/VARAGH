#pragma once

#include "GameUi.h"
#include "activities/Activity.h"
#include "games/SudokuGame.h"

// Touch Sudoku: tap a cell, then a number. A cell can hold several small
// numbers (notes) until its answer is written: with Notes on, the number keys
// write notes; holding a number key writes the other kind without switching.
// The pad also holds Undo, Erase, Fill (notes for every empty cell) and Hint.
// A wrong digit is not marked and mistakes are not counted on screen. New, in
// the header, has to be held so a stray tap cannot throw a puzzle away; a new puzzle or a solved one
// swaps the pad for a small panel. The grid is saved, so leaving the screen (or
// the reader going to sleep) never loses it.
class SudokuActivity final : public Activity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr char kSavePath[] = "/.crosspoint/games/sudoku.bin";
  static const char* levelName(sudoku::Difficulty difficulty);
  // "12:40", or "1:02:40" from one hour on.
  static void formatTime(uint32_t seconds, char* out, size_t size);

 private:
  enum class Panel : uint8_t { Pad, NewPuzzle, Solved };
  enum Tool { Undo = 0, Erase = 1, Notes = 2, Fill = 3, Hint = 4, kToolCount = 5 };

  void layout();
  void startPuzzle(sudoku::Difficulty difficulty);
  void save();
  // Adds the time since the last call to the puzzle clock.
  void tickClock();
  void afterMove(sudoku::Game::Change change);
  void drawGrid() const;
  void drawCell(int cell, const gameui::Box& box) const;
  void drawPad() const;
  void drawNewPuzzlePanel() const;
  void drawSolvedPanel() const;

  sudoku::Game game;
  Panel panel = Panel::Pad;
  int selected = -1;
  bool notesMode = false;
  bool dirty = false;
  uint8_t movesSinceSave = 0;
  uint32_t clockMs = 0;

  gameui::Box grid;
  int cellSize = 0;
  gameui::Box digitKeys[9];
  gameui::Box tools[kToolCount];
  gameui::HoldButton newHold;
  gameui::Box panelBox;
  gameui::Box levelBoxes[3];
  gameui::Box cancelBox;
  gameui::Box solvedNewBox;
};
