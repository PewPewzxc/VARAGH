#pragma once

#include "GameThinker.h"
#include "GameUi.h"
#include "activities/Activity.h"
#include "games/SudokuGame.h"

// Touch Sudoku: tap a cell, then a number. A cell can hold several small
// numbers (notes) until its answer is written: with Notes on, the number keys
// write notes; holding a number key writes the other kind without switching.
// The pad also holds Undo, Erase, Fill (notes for every empty cell) and Hint.
// A wrong digit is not marked and mistakes are not counted on screen, until a
// full grid is not solved: then a panel says how many answers are wrong and can
// show (cross out) or clear them. New, in
// the header, has to be held so a stray tap cannot throw a puzzle away; a new puzzle or a solved one
// swaps the pad for a small panel. The grid is saved, so leaving the screen (or
// the reader going to sleep) never loses it. Apps > Manage > Sudoku settings
// switch the extras on or off (Options).
class SudokuActivity final : public Activity {
 public:
  SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~SudokuActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr char kSavePath[] = "/.crosspoint/games/sudoku.bin";
  static constexpr char kOptionsPath[] = "/.crosspoint/games/sudoku-options.bin";
  static const char* levelName(sudoku::Difficulty difficulty);
  // "12:40", or "1:02:40" from one hour on.
  static void formatTime(uint32_t seconds, char* out, size_t size);
  // Reads the saved game in either save format; false when there is none.
  static bool readGame(sudoku::Game& game);
  // Today's day number (CalendarMath daysFromDate), 0 while the clock has never been set.
  static uint16_t today();

  // The switches, in the order of the settings rows. Kept in a file by number:
  // a new one goes at the end.
  enum Option {
    ExplainHints = 0,
    DailyPuzzle,
    LiveCheck,
    RepeatedNumbers,
    NumberPainting,
    FillLast,
    MistakeLimit,
    RestartButton,
    MoreStats,
    kOptionCount
  };
  struct Options {
    bool on[kOptionCount] = {true, true, false, false, false, false, false, false, false};
  };
  static void loadOptions(Options& options);
  static void saveOptions(const Options& options);

 private:
  // Check: the grid is full but some answers are wrong. Making: a puzzle is
  // being drawn up. Over: the third mistake under the mistake limit.
  enum class Panel : uint8_t { Pad, NewPuzzle, Solved, Check, Hint, Making, Over };
  enum CheckButton { ShowWrong = 0, ClearWrong = 1, KeepLooking = 2, kCheckButtonCount = 3 };
  enum Tool { Undo = 0, Erase = 1, Notes = 2, Fill = 3, Hint = 4, kToolCount = 5 };
  // The New puzzle panel: a box per level, then Daily.
  static constexpr int kDailyBox = sudoku::kLevelCount;
  static constexpr int kLevelBoxCount = sudoku::kLevelCount + 1;
  static constexpr int kMistakeLimit = 3;

  void layout();
  // Shows Making while the thinker task draws up the puzzle.
  void requestPuzzle(sudoku::Difficulty difficulty, uint16_t dailyDay);
  static void makePuzzle(void* self);
  void startPendingPuzzle();
  void save();
  // Adds the time since the last call to the puzzle clock.
  void tickClock();
  // A digit or note from the pad (or painting) into a cell.
  void enter(int cell, uint8_t digit, bool asNote);
  void afterMove(sudoku::Game::Change change);
  void showHint();
  void closeHint(bool showAnswer);
  // The panel a game that is not being played shows when the screen opens.
  Panel restingPanel() const;
  bool marksWrong() const { return showWrong || options.on[LiveCheck] || options.on[MistakeLimit]; }
  void drawHeaderLine() const;
  void drawGrid() const;
  void drawCell(int cell, const gameui::Box& box) const;
  void drawPad() const;
  void drawNewPuzzlePanel() const;
  void drawSolvedPanel() const;
  void drawCheckPanel() const;
  void drawHintPanel() const;
  void drawOverPanel() const;
  // Wrapped lines from the top of the panel; returns the y below them.
  int drawPanelLines(const char* title, const char* body, int maxLines) const;

  sudoku::Game game;
  Options options;
  Panel panel = Panel::Pad;
  int selected = -1;
  bool notesMode = false;
  bool showWrong = false;  // "Show wrong" pressed: wrong digits are marked until fixed
  bool dirty = false;
  uint8_t movesSinceSave = 0;
  uint32_t clockMs = 0;
  uint8_t paintDigit = 0;  // Number painting: the digit tapped cells get, 0 = none
  sudoku::Step hintStep;
  // The puzzle being drawn up, off the main task: Hard and harder can take a few seconds.
  GameThinker thinker;
  int pendingLevel = -1;
  uint16_t pendingDaily = 0;
  uint32_t pendingSeed = 0;
  sudoku::Puzzle pendingPuzzle;
  bool pendingMade = false;
  uint16_t day = 0;  // today(), read when the screen opens and when New is chosen

  gameui::Box grid;
  int cellSize = 0;
  gameui::Box digitKeys[9];
  gameui::Box tools[kToolCount];
  gameui::HoldButton newHold;
  gameui::Box panelBox;
  gameui::Box levelBoxes[kLevelBoxCount];
  gameui::Box cancelBox;
  gameui::Box restartBox;
  gameui::Box solvedNewBox;
  gameui::Box checkBoxes[kCheckButtonCount];
  gameui::Box pairBoxes[2];  // the two buttons of the Hint and Over panels
  gameui::ButtonPointer pointer;  // VARAGH: apps on button readers
};
