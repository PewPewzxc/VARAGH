#include "SudokuActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr int kSideMargin = 6;
constexpr int kMaxCell = 52;
constexpr int kDigitKeyHeight = 80;
constexpr int kDigitKeyGap = 4;
constexpr int kToolHeight = 118;
constexpr int kToolGap = 6;
constexpr int kBoxLine = 3;
// A pause longer than this (the reader put down, a call) is not solving time.
constexpr uint32_t kLongestCountedGapMs = 90000;
constexpr uint8_t kMovesBetweenSaves = 8;
constexpr unsigned long kHoldMs = 500;

}  // namespace

SudokuActivity::SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Sudoku", renderer, mappedInput) {}

const char* SudokuActivity::levelName(const sudoku::Difficulty difficulty) {
  switch (difficulty) {
    case sudoku::Difficulty::Easy:
      return tr(STR_SUDOKU_EASY);
    case sudoku::Difficulty::Medium:
      return tr(STR_SUDOKU_MEDIUM);
    case sudoku::Difficulty::Hard:
      break;
  }
  return tr(STR_SUDOKU_HARD);
}

void SudokuActivity::formatTime(const uint32_t seconds, char* out, const size_t size) {
  const unsigned hours = static_cast<unsigned>(seconds / 3600);
  const unsigned minutes = static_cast<unsigned>((seconds / 60) % 60);
  const unsigned secs = static_cast<unsigned>(seconds % 60);
  if (hours > 0) {
    snprintf(out, size, "%u:%02u:%02u", hours, minutes, secs);
  } else {
    snprintf(out, size, "%u:%02u", minutes, secs);
  }
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  uint8_t bytes[sudoku::Game::kSaveBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || !game.load(bytes, sizeof(bytes))) {
    LOG_DBG("SUDOKU", "No saved puzzle");
  }
  panel = !game.started() ? Panel::NewPuzzle : (game.solved() ? Panel::Solved : Panel::Pad);
  selected = -1;
  notesMode = false;
  clockMs = millis();
  requestUpdate();
}

void SudokuActivity::onExit() {
  tickClock();
  save();
  Activity::onExit();
}

void SudokuActivity::tickClock() {
  const uint32_t now = millis();
  const uint32_t gap = now - clockMs;
  if (game.started() && !game.solved() && panel == Panel::Pad && gap >= 1000) {
    const uint32_t counted = std::min(gap, kLongestCountedGapMs);
    game.addElapsedSeconds(counted / 1000);
    // Carry the unused part of a second into the next tick.
    clockMs = now - (gap <= kLongestCountedGapMs ? gap % 1000 : 0);
    dirty = true;
  } else if (gap >= 1000 || panel != Panel::Pad) {
    clockMs = now;
  }
}

void SudokuActivity::save() {
  if (!dirty) return;
  uint8_t bytes[sudoku::Game::kSaveBytes];
  game.save(bytes);
  if (gameui::writeSave(kSavePath, bytes, sizeof(bytes))) {
    dirty = false;
    movesSinceSave = 0;
  } else {
    LOG_ERR("SUDOKU", "Could not save the puzzle");
  }
}

void SudokuActivity::startPuzzle(const sudoku::Difficulty difficulty) {
  sudoku::Puzzle puzzle;
  sudoku::generate(difficulty, static_cast<uint32_t>(random(0x7FFFFFFF)) ^ (millis() << 8), puzzle);
  game.start(puzzle, difficulty);
  panel = Panel::Pad;
  selected = -1;
  notesMode = false;
  clockMs = millis();
  dirty = true;
  save();
}

void SudokuActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int top = gameui::contentTop(renderer, mappedInput);
  const int controls = kDigitKeyHeight + kToolHeight + 2 * gameui::kSectionGap + gameui::kBottomMargin;

  cellSize = std::clamp(std::min((width - 2 * kSideMargin) / 9, (height - top - controls) / 9), 20, kMaxCell);
  const int gridSize = cellSize * 9;
  grid = {(width - gridSize) / 2, top, gridSize, gridSize};

  const int padY = grid.y + grid.h + gameui::kSectionGap;
  const int keyWidth = (gridSize - 8 * kDigitKeyGap) / 9;
  const int keysX = grid.x + (gridSize - (9 * keyWidth + 8 * kDigitKeyGap)) / 2;
  for (int i = 0; i < 9; ++i) {
    digitKeys[i] = {keysX + i * (keyWidth + kDigitKeyGap), padY, keyWidth, kDigitKeyHeight};
  }

  // The tools sit low, near the foot of the screen, where a thumb rests.
  const int toolsY =
      std::max(padY + kDigitKeyHeight + gameui::kSectionGap, gameui::contentBottom(renderer) - kToolHeight);
  const int toolWidth = (gridSize - (kToolCount - 1) * kToolGap) / kToolCount;
  for (int i = 0; i < kToolCount; ++i) {
    tools[i] = {grid.x + i * (toolWidth + kToolGap), toolsY, toolWidth, kToolHeight};
  }
  newHold.box = gameui::headerCornerBox(renderer, mappedInput, tr(STR_GAME_NEW_SHORT));

  panelBox = {grid.x, padY, gridSize, toolsY + kToolHeight - padY};
  const int levelWidth = (gridSize - 2 * kToolGap) / 3;
  for (int i = 0; i < 3; ++i) {
    levelBoxes[i] = {grid.x + i * (levelWidth + kToolGap), panelBox.y + 50, levelWidth, 80};
  }
  cancelBox = {grid.x + (gridSize - 180) / 2, panelBox.y + panelBox.h - 54, 180, 52};
  solvedNewBox = {grid.x + (gridSize - 220) / 2, panelBox.y + panelBox.h - 58, 220, 56};
}

void SudokuActivity::afterMove(const sudoku::Game::Change change) {
  if (change == sudoku::Game::Change::None) return;
  dirty = true;
  if (change == sudoku::Game::Change::Solved || game.solved()) {
    panel = Panel::Solved;
    selected = -1;
    save();
  } else if (++movesSinceSave >= kMovesBetweenSaves) {
    save();
  }
  requestUpdate();
}

void SudokuActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  // New is held, not tapped: it fills while the finger rests on it.
  bool repaint = false;
  if (panel == Panel::Pad && newHold.update(mappedInput, repaint)) {
    tickClock();
    panel = Panel::NewPuzzle;
    requestUpdate();
    return;
  }
  if (repaint) requestUpdate();

  int tx = 0;
  int ty = 0;
  // Holding a number key writes the kind of entry the Notes switch is not set
  // to: a note while answering, an answer while taking notes. The rest of that
  // touch is then ignored, so lifting the finger is not also read as a tap; a
  // hold anywhere else stays an ordinary tap.
  if (panel == Panel::Pad && selected >= 0 && mappedInput.isScreenTouchLongPress(tx, ty, kHoldMs)) {
    for (int i = 0; i < 9; ++i) {
      if (!digitKeys[i].grown(kDigitKeyGap / 2).contains(tx, ty)) continue;
      mappedInput.suppressCurrentTouchContact();
      tickClock();
      const auto digit = static_cast<uint8_t>(i + 1);
      afterMove(notesMode ? game.place(selected, digit) : game.toggleNote(selected, digit));
      return;
    }
  }
  if (!mappedInput.wasScreenTapped(tx, ty)) return;

  if (panel == Panel::NewPuzzle) {
    for (int i = 0; i < 3; ++i) {
      if (levelBoxes[i].contains(tx, ty)) {
        startPuzzle(static_cast<sudoku::Difficulty>(i));
        requestUpdate();
        return;
      }
    }
    if (game.started() && cancelBox.contains(tx, ty)) {
      panel = game.solved() ? Panel::Solved : Panel::Pad;
      clockMs = millis();
      requestUpdate();
    }
    return;
  }

  if (panel == Panel::Solved) {
    if (solvedNewBox.contains(tx, ty)) {
      panel = Panel::NewPuzzle;
      requestUpdate();
    }
    return;
  }

  if (newHold.box.contains(tx, ty)) return;  // a tap on New does nothing
  tickClock();
  if (grid.contains(tx, ty)) {
    const int cell = ((ty - grid.y) / cellSize) * 9 + (tx - grid.x) / cellSize;
    if (cell != selected) {
      selected = cell;
      requestUpdate();
    }
    return;
  }
  for (int i = 0; i < 9; ++i) {
    if (!digitKeys[i].grown(kDigitKeyGap / 2).contains(tx, ty)) continue;
    if (selected < 0) return;
    const auto digit = static_cast<uint8_t>(i + 1);
    afterMove(notesMode ? game.toggleNote(selected, digit) : game.place(selected, digit));
    return;
  }
  if (tools[Undo].contains(tx, ty)) {
    if (game.undo()) {
      dirty = true;
      requestUpdate();
    }
  } else if (tools[Erase].contains(tx, ty)) {
    if (selected >= 0) afterMove(game.erase(selected));
  } else if (tools[Notes].contains(tx, ty)) {
    notesMode = !notesMode;
    requestUpdate();
  } else if (tools[Fill].contains(tx, ty)) {
    if (game.fillNotes() > 0) afterMove(sudoku::Game::Change::NoteChanged);
  } else if (tools[Hint].contains(tx, ty)) {
    const int cell = game.hint(selected);
    if (cell >= 0) {
      selected = cell;
      afterMove(sudoku::Game::Change::Placed);
    }
  }
}

void SudokuActivity::drawCell(const int cell, const gameui::Box& box) const {
  const uint8_t value = game.value(cell);
  const bool isSelected = cell == selected;
  const uint8_t selectedValue = selected >= 0 ? game.value(selected) : 0;
  const gameui::Box inner{box.x + 1, box.y + 1, box.w - 1, box.h - 1};

  if (isSelected) {
    renderer.fillRect(inner.x, inner.y, inner.w, inner.h, true);
  } else if (selected >= 0) {
    const bool sameUnit = selected / 9 == cell / 9 || selected % 9 == cell % 9 ||
                          ((selected / 27 == cell / 27) && ((selected % 9) / 3 == (cell % 9) / 3));
    if (sameUnit) gameui::tint(renderer, inner);
    // Every other cell holding the selected digit gets a ring.
    if (value != 0 && value == selectedValue) {
      renderer.drawRoundedRect(box.x + 4, box.y + 4, box.w - 7, box.h - 7, 3, 8, true);
    }
  }

  if (value != 0) {
    // Printed clues are bold, your own digits lighter; a hint is bold with a corner mark.
    gameui::bigDigit(renderer, value, game.isLocked(cell), box, isSelected ? gameui::Ink::White : gameui::Ink::Black);
    if (game.isHinted(cell)) {
      for (int i = 0; i < 9; ++i) {
        renderer.drawLine(box.x + box.w - 3 - i, box.y + 3, box.x + box.w - 3, box.y + 3 + i, !isSelected);
      }
    }
    // A wrong digit looks like any other: finding it is part of the puzzle.
    return;
  }

  const uint16_t notes = game.notes(cell);
  if (notes == 0) return;
  const int sub = box.w / 3;
  for (int digit = 1; digit <= 9; ++digit) {
    if (!(notes & (1u << digit))) continue;
    const char text[2] = {static_cast<char>('0' + digit), '\0'};
    const gameui::Box noteBox{box.x + ((digit - 1) % 3) * sub + 1, box.y + ((digit - 1) / 3) * sub + 1, sub, sub};
    gameui::centredText(renderer, SMALL_FONT_ID, noteBox, text, !isSelected);
  }
}

void SudokuActivity::drawGrid() const {
  for (int cell = 0; cell < sudoku::kCells; ++cell) {
    drawCell(cell, {grid.x + (cell % 9) * cellSize, grid.y + (cell / 9) * cellSize, cellSize, cellSize});
  }
  for (int i = 0; i <= 9; ++i) {
    const int offset = i * cellSize;
    if (i % 3 == 0) {
      renderer.fillRect(grid.x + offset - kBoxLine / 2, grid.y - kBoxLine / 2, kBoxLine, grid.h + kBoxLine, true);
      renderer.fillRect(grid.x - kBoxLine / 2, grid.y + offset - kBoxLine / 2, grid.w + kBoxLine, kBoxLine, true);
    } else {
      renderer.drawLine(grid.x + offset, grid.y, grid.x + offset, grid.y + grid.h, true);
      renderer.drawLine(grid.x, grid.y + offset, grid.x + grid.w, grid.y + offset, true);
    }
  }
}

void SudokuActivity::drawPad() const {
  for (int i = 0; i < 9; ++i) {
    const gameui::Box& box = digitKeys[i];
    const int left = game.remaining(static_cast<uint8_t>(i + 1));
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 6, true);
    gameui::bigDigit(renderer, i + 1, true, {box.x, box.y + 10, box.w, 38}, gameui::Ink::Black);
    // How many of this digit the grid still needs.
    char count[4];
    snprintf(count, sizeof(count), "%d", left);
    gameui::centredText(renderer, SMALL_FONT_ID, {box.x, box.y + box.h - 28, box.w, 22}, count);
    if (left == 0) gameui::fade(renderer, box);
  }

  const char* labels[kToolCount] = {tr(STR_SUDOKU_UNDO), tr(STR_SUDOKU_ERASE), tr(STR_SUDOKU_NOTES),
                                    tr(STR_SUDOKU_FILL), tr(STR_SUDOKU_HINT)};
  const appart::Bitmap* icons[kToolCount] = {&appart::kIconUndo, &appart::kIconErase, &appart::kIconNotes,
                                             &appart::kIconFill, &appart::kIconHint};
  for (int i = 0; i < kToolCount; ++i) {
    gameui::iconButton(renderer, tools[i], *icons[i], labels[i], i == Notes && notesMode);
  }
}

void SudokuActivity::drawNewPuzzlePanel() const {
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y, panelBox.w, 36}, tr(STR_SUDOKU_NEW), true,
                      EpdFontFamily::BOLD);
  for (int i = 0; i < 3; ++i) {
    const auto level = static_cast<sudoku::Difficulty>(i);
    const gameui::Box& box = levelBoxes[i];
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 8, true);
    gameui::centredText(renderer, UI_12_FONT_ID, {box.x, box.y + 8, box.w, 30}, levelName(level), true,
                        EpdFontFamily::BOLD);
    // Best time on this level, once there is one.
    char best[24] = "";
    if (game.bestSeconds(level) > 0) formatTime(game.bestSeconds(level), best, sizeof(best));
    gameui::centredText(renderer, SMALL_FONT_ID, {box.x, box.y + 42, box.w, 22}, best[0] ? best : "-");
  }
  if (game.started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
}

void SudokuActivity::drawSolvedPanel() const {
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y, panelBox.w, 36}, tr(STR_SUDOKU_SOLVED), true,
                      EpdFontFamily::BOLD);
  char time[24];
  formatTime(game.elapsedSeconds(), time, sizeof(time));
  char line[96];
  snprintf(line, sizeof(line), tr(STR_SUDOKU_RESULT), time, static_cast<unsigned>(game.mistakes()),
           static_cast<unsigned>(game.hintsUsed()));
  gameui::centredText(renderer, UI_10_FONT_ID, {panelBox.x, panelBox.y + 42, panelBox.w, 28}, line);
  formatTime(game.bestSeconds(game.difficulty()), time, sizeof(time));
  snprintf(line, sizeof(line), tr(STR_SUDOKU_BEST), levelName(game.difficulty()), time);
  gameui::centredText(renderer, UI_10_FONT_ID, {panelBox.x, panelBox.y + 72, panelBox.w, 28}, line);
  gameui::button(renderer, solvedNewBox, tr(STR_SUDOKU_NEW), true);
}

void SudokuActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();

  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_SUDOKU));
  if (panel == Panel::Pad && game.started()) {
    // The level, small, between the title and New. Nothing here counts
    // mistakes: that would say a digit is wrong before the player has seen it.
    const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
    const int left = TouchHeaderBackButton::layout(header).titleX +
                     renderer.getTextWidth(UI_12_FONT_ID, tr(STR_GAME_SUDOKU), EpdFontFamily::BOLD) + 12;
    const int centreY = gameui::headerTitleCentreY(renderer, mappedInput);
    gameui::centredText(renderer, UI_10_FONT_ID, {left, centreY - 16, newHold.box.x - 8 - left, 34},
                        levelName(game.difficulty()));
    newHold.draw(renderer, tr(STR_GAME_NEW_SHORT));
  }

  drawGrid();
  switch (panel) {
    case Panel::Pad:
      drawPad();
      break;
    case Panel::NewPuzzle:
      drawNewPuzzlePanel();
      break;
    case Panel::Solved:
      drawSolvedPanel();
      break;
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
