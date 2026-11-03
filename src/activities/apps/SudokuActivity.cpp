#include "SudokuActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ClockFace.h"
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
constexpr int kPanelButtonHeight = 52;
// A pause longer than this (the reader put down, a call) is not solving time.
constexpr uint32_t kLongestCountedGapMs = 90000;
constexpr uint8_t kMovesBetweenSaves = 8;
constexpr unsigned long kHoldMs = 500;
constexpr uint8_t kOptionsMagic[4] = {'V', 'S', 'O', '1'};
// The level of the daily puzzle.
constexpr sudoku::Difficulty kDailyLevel = sudoku::Difficulty::Medium;

bool inUnit(const int unit, const int cell) {
  for (int i = 0; i < 9; ++i) {
    if (sudoku::unitCell(unit, i) == cell) return true;
  }
  return false;
}

// "Row 3", "Column 5" or "Box 7".
void unitName(const int unit, char* out, const size_t size) {
  const char* format = unit < 9 ? tr(STR_SUDOKU_ROW) : (unit < 18 ? tr(STR_SUDOKU_COLUMN) : tr(STR_SUDOKU_BOX));
  snprintf(out, size, format, unit % 9 + 1);
}

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
      return tr(STR_SUDOKU_HARD);
    case sudoku::Difficulty::Expert:
      return tr(STR_SUDOKU_EXPERT);
    case sudoku::Difficulty::Master:
      break;
  }
  return tr(STR_SUDOKU_MASTER);
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

bool SudokuActivity::readGame(sudoku::Game& game) {
  uint8_t bytes[sudoku::Game::kSaveBytes];
  if (gameui::readSave(kSavePath, bytes, sudoku::Game::kSaveBytes)) return game.load(bytes, sudoku::Game::kSaveBytes);
  // A file from before Expert, Master and the daily puzzle is shorter.
  return gameui::readSave(kSavePath, bytes, sudoku::Game::kSaveBytesV1) &&
         game.load(bytes, sudoku::Game::kSaveBytesV1);
}

uint16_t SudokuActivity::today() {
  calmath::DateTime now;
  if (!clockface::now(now)) return 0;
  const int days = calmath::daysFromDate(now.date);
  return days > 0 && days <= UINT16_MAX ? static_cast<uint16_t>(days) : 0;
}

void SudokuActivity::loadOptions(Options& options) {
  options = Options{};
  uint8_t bytes[sizeof(kOptionsMagic) + kOptionCount];
  if (!gameui::readSave(kOptionsPath, bytes, sizeof(bytes)) ||
      std::memcmp(bytes, kOptionsMagic, sizeof(kOptionsMagic)) != 0) {
    return;
  }
  for (int i = 0; i < kOptionCount; ++i) options.on[i] = bytes[sizeof(kOptionsMagic) + i] != 0;
}

void SudokuActivity::saveOptions(const Options& options) {
  uint8_t bytes[sizeof(kOptionsMagic) + kOptionCount];
  std::memcpy(bytes, kOptionsMagic, sizeof(kOptionsMagic));
  for (int i = 0; i < kOptionCount; ++i) bytes[sizeof(kOptionsMagic) + i] = options.on[i] ? 1 : 0;
  gameui::writeSave(kOptionsPath, bytes, sizeof(bytes));
}

SudokuActivity::Panel SudokuActivity::restingPanel() const {
  if (!game.started()) return Panel::NewPuzzle;
  if (game.solved()) return Panel::Solved;
  if (options.on[MistakeLimit] && game.mistakes() >= kMistakeLimit) return Panel::Over;
  if (game.emptyCells() == 0 && game.wrongCells() > 0) return Panel::Check;
  return Panel::Pad;
}

void SudokuActivity::onEnter() {
  Activity::onEnter();
  pointer.begin(mappedInput);
  loadOptions(options);
  if (!readGame(game)) {
    LOG_DBG("SUDOKU", "No saved puzzle");
  }
  day = today();
  panel = restingPanel();
  selected = -1;
  notesMode = false;
  showWrong = false;
  paintDigit = 0;
  pendingLevel = -1;
  clockMs = millis();
  requestUpdate();
}

SudokuActivity::~SudokuActivity() { thinker.cancel(); }

void SudokuActivity::onExit() {
  thinker.cancel();
  tickClock();
  save();
  Activity::onExit();
}

void SudokuActivity::tickClock() {
  const uint32_t now = millis();
  const uint32_t gap = now - clockMs;
  // Time spent reading a hint is solving time too.
  const bool solving = panel == Panel::Pad || panel == Panel::Hint;
  if (game.started() && !game.solved() && solving && gap >= 1000) {
    const uint32_t counted = std::min(gap, kLongestCountedGapMs);
    game.addElapsedSeconds(counted / 1000);
    // Carry the unused part of a second into the next tick.
    clockMs = now - (gap <= kLongestCountedGapMs ? gap % 1000 : 0);
    dirty = true;
  } else if (gap >= 1000 || !solving) {
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

void SudokuActivity::requestPuzzle(const sudoku::Difficulty difficulty, const uint16_t dailyDay) {
  pendingLevel = static_cast<int>(difficulty);
  pendingDaily = dailyDay;
  // The same day gives every reader the same daily puzzle.
  pendingSeed = dailyDay != 0 ? static_cast<uint32_t>(dailyDay) * 2654435761u ^ 0xD411u
                              : static_cast<uint32_t>(random(0x7FFFFFFF)) ^ (millis() << 8);
  pendingMade = false;
  panel = Panel::Making;
  requestUpdate();
  // No time limit: only leaving the screen stops it.
  if (!thinker.start(&SudokuActivity::makePuzzle, this, 0)) {
    makePuzzle(this);  // no task to spare: draw it up here
    startPendingPuzzle();
  }
}

void SudokuActivity::makePuzzle(void* self) {
  auto* activity = static_cast<SudokuActivity*>(self);
  activity->pendingMade =
      sudoku::generate(static_cast<sudoku::Difficulty>(activity->pendingLevel), activity->pendingSeed,
                       activity->pendingPuzzle, activity->thinker.running() ? &GameThinker::stopCallback : nullptr,
                       &activity->thinker);
}

void SudokuActivity::startPendingPuzzle() {
  const auto difficulty = static_cast<sudoku::Difficulty>(pendingLevel);
  if (!pendingMade) {
    panel = restingPanel();
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    game.start(pendingPuzzle, difficulty, pendingDaily);
    pendingLevel = -1;
    panel = Panel::Pad;
    selected = -1;
    notesMode = false;
    showWrong = false;
    paintDigit = 0;
  }
  clockMs = millis();
  dirty = true;
  save();
  requestUpdate();
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
  const int thirdWidth = (gridSize - 2 * kToolGap) / 3;
  const int footY = panelBox.y + panelBox.h - kPanelButtonHeight;
  // New puzzle: two rows of three (five levels, then Daily) above Cancel,
  // which is a little lower than the other panels' buttons to leave them room.
  constexpr int cancelHeight = 46;
  const int cancelY = panelBox.y + panelBox.h - cancelHeight;
  const int levelsY = panelBox.y + 38;
  const int levelHeight = std::min(66, (cancelY - kToolGap - levelsY - kToolGap) / 2);
  for (int i = 0; i < kLevelBoxCount; ++i) {
    levelBoxes[i] = {grid.x + (i % 3) * (thirdWidth + kToolGap), levelsY + (i / 3) * (levelHeight + kToolGap),
                     thirdWidth, levelHeight};
  }
  if (options.on[RestartButton] && game.started() && !game.solved()) {
    cancelBox = {grid.x + gridSize / 2 - kToolGap / 2 - 170, cancelY, 170, cancelHeight};
    restartBox = {grid.x + gridSize / 2 + kToolGap / 2, cancelY, 170, cancelHeight};
  } else {
    cancelBox = {grid.x + (gridSize - 180) / 2, cancelY, 180, cancelHeight};
    restartBox = {};
  }
  solvedNewBox = {grid.x + (gridSize - 220) / 2, panelBox.y + panelBox.h - 58, 220, 56};
  for (int i = 0; i < kCheckButtonCount; ++i) {
    checkBoxes[i] = {grid.x + i * (thirdWidth + kToolGap), panelBox.y + 60, thirdWidth, 80};
  }
  const int halfWidth = (gridSize - kToolGap) / 2;
  for (int i = 0; i < 2; ++i) pairBoxes[i] = {grid.x + i * (halfWidth + kToolGap), footY, halfWidth, kPanelButtonHeight};
}

void SudokuActivity::enter(const int cell, const uint8_t digit, const bool asNote) {
  const auto change = asNote ? game.toggleNote(cell, digit) : game.place(cell, digit);
  if (change == sudoku::Game::Change::Placed && options.on[FillLast]) game.fillLastCells(cell);
  afterMove(change);
}

void SudokuActivity::afterMove(const sudoku::Game::Change change) {
  if (change == sudoku::Game::Change::None) return;
  dirty = true;
  if (change == sudoku::Game::Change::Solved || game.solved()) {
    panel = Panel::Solved;
    selected = -1;
    paintDigit = 0;
    save();
  } else if (change == sudoku::Game::Change::Placed && options.on[MistakeLimit] &&
             game.mistakes() >= kMistakeLimit) {
    panel = Panel::Over;
    selected = -1;
    save();
  } else if (change == sudoku::Game::Change::Placed && game.emptyCells() == 0) {
    // Full but not solved: say how many answers are wrong.
    panel = Panel::Check;
    selected = -1;
    showWrong = false;
    save();
  } else if (++movesSinceSave >= kMovesBetweenSaves) {
    save();
  }
  requestUpdate();
}

void SudokuActivity::showHint() {
  if (!options.on[ExplainHints]) {
    const int cell = game.hint(selected);
    if (cell >= 0) {
      selected = cell;
      afterMove(sudoku::Game::Change::Placed);
    }
    return;
  }
  hintStep = sudoku::nextStep(game.values(), game.solutionGrid(), selected);
  if (hintStep.cell < 0) return;
  selected = hintStep.cell;
  panel = Panel::Hint;
  requestUpdate();
}

void SudokuActivity::closeHint(const bool primary) {
  panel = Panel::Pad;
  const int cell = hintStep.cell;
  if (primary && hintStep.kind != sudoku::Step::Kind::Wrong) {
    // Show answer: the cell is filled the way a plain hint fills it.
    if (game.hint(cell) >= 0) {
      selected = cell;
      afterMove(sudoku::Game::Change::Placed);
    } else {
      requestUpdate();
    }
    return;
  }
  game.countHint();
  dirty = true;
  if (primary) {
    afterMove(game.erase(cell));  // Erase it
    return;
  }
  requestUpdate();
}

void SudokuActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();  // also stops a puzzle being drawn up (onExit)
    return;
  }
  if (panel == Panel::Making) {
    if (thinker.takeFinished()) startPendingPuzzle();
    return;
  }

  // New is held, not tapped: it fills while the finger rests on it.
  bool repaint = false;
  if (panel == Panel::Pad && newHold.update(mappedInput, repaint, &pointer)) {
    tickClock();
    day = today();
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
  // hold anywhere else stays an ordinary tap. With number painting the hold is
  // on the cell instead.
  if (panel == Panel::Pad && mappedInput.isScreenTouchLongPress(tx, ty, kHoldMs)) {
    if (options.on[NumberPainting] && paintDigit != 0 && grid.contains(tx, ty)) {
      mappedInput.suppressCurrentTouchContact();
      tickClock();
      selected = ((ty - grid.y) / cellSize) * 9 + (tx - grid.x) / cellSize;
      enter(selected, paintDigit, !notesMode);
      requestUpdate();
      return;
    }
    for (int i = 0; i < 9 && selected >= 0 && !options.on[NumberPainting]; ++i) {
      if (!digitKeys[i].grown(kDigitKeyGap / 2).contains(tx, ty)) continue;
      mappedInput.suppressCurrentTouchContact();
      tickClock();
      enter(selected, static_cast<uint8_t>(i + 1), !notesMode);
      return;
    }
  }
  bool moved = false;
  bool hit;
  {
    RenderLock lock(*this);
    hit = pointer.tapped(mappedInput, tx, ty, moved);
  }
  if (moved) requestUpdate();
  if (!hit) return;

  if (panel == Panel::NewPuzzle) {
    for (int i = 0; i < sudoku::kLevelCount; ++i) {
      if (levelBoxes[i].contains(tx, ty)) {
        requestPuzzle(static_cast<sudoku::Difficulty>(i), 0);
        return;
      }
    }
    if (options.on[DailyPuzzle] && day != 0 && levelBoxes[kDailyBox].contains(tx, ty)) {
      if (game.started() && !game.solved() && game.dailyDay() == day) {
        panel = restingPanel();  // today's is the one already open
        clockMs = millis();
        requestUpdate();
      } else if (!game.dailySolved(day)) {
        requestPuzzle(kDailyLevel, day);
      }
      return;
    }
    if (restartBox.w > 0 && restartBox.contains(tx, ty)) {
      game.restart();
      dirty = true;
      save();
      panel = Panel::Pad;
      selected = -1;
      showWrong = false;
      clockMs = millis();
      requestUpdate();
      return;
    }
    if (game.started() && cancelBox.contains(tx, ty)) {
      panel = restingPanel();
      clockMs = millis();
      requestUpdate();
    }
    return;
  }

  if (panel == Panel::Solved) {
    if (solvedNewBox.contains(tx, ty)) {
      day = today();
      panel = Panel::NewPuzzle;
      requestUpdate();
    }
    return;
  }

  if (panel == Panel::Over) {
    if (pairBoxes[0].contains(tx, ty)) {
      game.restart();
      dirty = true;
      save();
      panel = Panel::Pad;
      clockMs = millis();
    } else if (pairBoxes[1].contains(tx, ty)) {
      day = today();
      panel = Panel::NewPuzzle;
    } else {
      return;
    }
    requestUpdate();
    return;
  }

  if (panel == Panel::Hint) {
    tickClock();
    for (int i = 0; i < 2; ++i) {
      if (pairBoxes[i].contains(tx, ty)) closeHint(i == 0);
    }
    return;
  }

  if (panel == Panel::Check) {
    if (checkBoxes[ShowWrong].contains(tx, ty)) {
      showWrong = true;
    } else if (checkBoxes[ClearWrong].contains(tx, ty)) {
      if (game.clearWrong() > 0) dirty = true;
    } else if (!checkBoxes[KeepLooking].contains(tx, ty)) {
      return;
    }
    panel = Panel::Pad;
    clockMs = millis();
    requestUpdate();
    return;
  }

  if (newHold.box.contains(tx, ty)) return;  // a tap on New does nothing
  tickClock();
  if (grid.contains(tx, ty)) {
    const int cell = ((ty - grid.y) / cellSize) * 9 + (tx - grid.x) / cellSize;
    if (options.on[NumberPainting] && paintDigit != 0) {
      selected = cell;
      if (!game.isLocked(cell)) {
        enter(cell, paintDigit, notesMode);
        if (panel == Panel::Pad) requestUpdate();
        return;
      }
      requestUpdate();
      return;
    }
    if (cell != selected) {
      selected = cell;
      requestUpdate();
    }
    return;
  }
  for (int i = 0; i < 9; ++i) {
    if (!digitKeys[i].grown(kDigitKeyGap / 2).contains(tx, ty)) continue;
    const auto digit = static_cast<uint8_t>(i + 1);
    if (options.on[NumberPainting]) {
      // Painting: the key is picked up (or put down), the cells come next.
      paintDigit = paintDigit == digit ? 0 : digit;
      requestUpdate();
      return;
    }
    if (selected < 0) return;
    enter(selected, digit, notesMode);
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
    showHint();
  }
}

void SudokuActivity::drawCell(const int cell, const gameui::Box& box) const {
  const uint8_t value = game.value(cell);
  const bool isSelected = cell == selected;
  const bool hinting = panel == Panel::Hint;
  // Cells holding this digit get a ring: the selected cell's digit, the
  // painted digit, or the digit a hint says has one place left.
  uint8_t ringDigit = selected >= 0 ? game.value(selected) : 0;
  if (paintDigit != 0) ringDigit = paintDigit;
  if (hinting) ringDigit = hintStep.kind == sudoku::Step::Kind::OnlyPlace ? hintStep.digit : 0;
  const gameui::Box inner{box.x + 1, box.y + 1, box.w - 1, box.h - 1};

  if (isSelected) {
    renderer.fillRect(inner.x, inner.y, inner.w, inner.h, true);
  } else {
    // A hint shades the unit it speaks of (all three for the only number that
    // fits); otherwise the selected cell's row, column and box.
    const bool sameUnit = selected >= 0 && (selected / 9 == cell / 9 || selected % 9 == cell % 9 ||
                                            ((selected / 27 == cell / 27) && ((selected % 9) / 3 == (cell % 9) / 3)));
    const bool shaded =
        hinting && hintStep.kind != sudoku::Step::Kind::OnlyNumber ? inUnit(hintStep.unit, cell) : sameUnit;
    if (shaded) gameui::tint(renderer, inner);
    if (value != 0 && value == ringDigit) {
      renderer.drawRoundedRect(box.x + 4, box.y + 4, box.w - 7, box.h - 7, 3, 8, true);
    }
  }

  if (value != 0) {
    const bool ink = !isSelected;
    // Printed clues are bold, your own digits lighter; a hint is bold with a corner mark.
    gameui::bigDigit(renderer, value, game.isLocked(cell), box, isSelected ? gameui::Ink::White : gameui::Ink::Black);
    if (game.isHinted(cell)) {
      for (int i = 0; i < 9; ++i) {
        renderer.drawLine(box.x + box.w - 3 - i, box.y + 3, box.x + box.w - 3, box.y + 3 + i, ink);
      }
    }
    // A wrong digit looks like any other (finding it is part of the puzzle)
    // until "Show wrong" is pressed on a full grid, or a setting asks for it:
    // then it is crossed out until it is changed.
    if (marksWrong() && game.isWrong(cell)) {
      for (int t = 0; t < 2; ++t) {
        renderer.drawLine(inner.x + 3 + t, inner.y + 3, inner.x + inner.w - 4 + t, inner.y + inner.h - 4, ink);
        renderer.drawLine(inner.x + inner.w - 4 - t, inner.y + 3, inner.x + 3 - t, inner.y + inner.h - 4, ink);
      }
    }
    // Repeated numbers: a double line under a digit of yours that its row, column or box already has.
    if (options.on[RepeatedNumbers] && !game.isLocked(cell) && game.repeats(cell)) {
      const int lineX = inner.x + inner.w / 5;
      const int lineW = inner.w - 2 * (inner.w / 5);
      renderer.fillRect(lineX, inner.y + inner.h - 8, lineW, 2, ink);
      renderer.fillRect(lineX, inner.y + inner.h - 4, lineW, 2, ink);
    }
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
    // The painted digit's key is shown pressed.
    const bool picked = paintDigit == i + 1;
    if (picked) {
      renderer.fillRoundedRect(box.x, box.y, box.w, box.h, 6, Color::Black);
    } else {
      renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 6, true);
    }
    gameui::bigDigit(renderer, i + 1, true, {box.x, box.y + 10, box.w, 38},
                     picked ? gameui::Ink::White : gameui::Ink::Black);
    // How many of this digit the grid still needs.
    char count[4];
    snprintf(count, sizeof(count), "%d", left);
    gameui::centredText(renderer, SMALL_FONT_ID, {box.x, box.y + box.h - 28, box.w, 22}, count, !picked);
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
  for (int i = 0; i < sudoku::kLevelCount; ++i) {
    const auto level = static_cast<sudoku::Difficulty>(i);
    const gameui::Box& box = levelBoxes[i];
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 8, true);
    gameui::fittedText(renderer, {box.x + 4, box.y + 3, box.w - 8, 30}, levelName(level));
    // Best time on this level, once there is one (and with More statistics, how many were solved).
    char best[24] = "-";
    if (game.bestSeconds(level) > 0) formatTime(game.bestSeconds(level), best, sizeof(best));
    char line[40];
    if (options.on[MoreStats] && game.solvedCount(level) > 0) {
      snprintf(line, sizeof(line), "%s (%u)", best, static_cast<unsigned>(game.solvedCount(level)));
    } else {
      snprintf(line, sizeof(line), "%s", best);
    }
    gameui::centredText(renderer, SMALL_FONT_ID, {box.x, box.y + box.h - 25, box.w, 22}, line);
  }
  if (options.on[DailyPuzzle] && day != 0) {
    const gameui::Box& box = levelBoxes[kDailyBox];
    const bool done = game.dailySolved(day);
    if (done) {
      renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 8, true);
    } else {
      renderer.fillRoundedRect(box.x, box.y, box.w, box.h, 8, Color::Black);
    }
    gameui::fittedText(renderer, {box.x + 4, box.y + 3, box.w - 8, 30}, tr(STR_SUDOKU_DAILY), done);
    char line[40];
    const unsigned streak = game.dailyStreak(day);
    if (done) {
      snprintf(line, sizeof(line), "%s", tr(STR_SUDOKU_DAILY_DONE));
    } else if (streak > 0) {
      snprintf(line, sizeof(line), tr(STR_SUDOKU_DAYS_IN_ROW), streak);
    } else {
      snprintf(line, sizeof(line), "%s", tr(STR_SUDOKU_TODAY));
    }
    gameui::centredText(renderer, SMALL_FONT_ID, {box.x, box.y + box.h - 25, box.w, 22},
                        renderer.truncatedText(SMALL_FONT_ID, line, box.w - 6).c_str(), done);
    if (done) gameui::fade(renderer, box);
  }
  if (game.started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
  if (restartBox.w > 0) gameui::button(renderer, restartBox, tr(STR_SUDOKU_RESTART), false);
}

void SudokuActivity::drawSolvedPanel() const {
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y, panelBox.w, 36}, tr(STR_SUDOKU_SOLVED), true,
                      EpdFontFamily::BOLD);
  const sudoku::Difficulty level = game.difficulty();
  char time[24];
  formatTime(game.elapsedSeconds(), time, sizeof(time));
  char lines[4][96] = {};
  int count = 0;
  snprintf(lines[count++], sizeof(lines[0]), tr(STR_SUDOKU_RESULT), time, static_cast<unsigned>(game.mistakes()),
           static_cast<unsigned>(game.hintsUsed()));
  if (game.dailyDay() != 0) {
    snprintf(lines[count++], sizeof(lines[0]), tr(STR_SUDOKU_DAYS_IN_ROW),
             static_cast<unsigned>(game.dailyStreak(std::max(day, game.dailyDay()))));
  } else {
    formatTime(game.bestSeconds(level), time, sizeof(time));
    snprintf(lines[count++], sizeof(lines[0]), tr(STR_SUDOKU_BEST), levelName(level), time);
  }
  if (options.on[MoreStats]) {
    formatTime(game.averageSeconds(level), time, sizeof(time));
    snprintf(lines[count++], sizeof(lines[0]), tr(STR_SUDOKU_AVERAGE), levelName(level),
             static_cast<unsigned>(game.solvedCount(level)), game.averageSeconds(level) > 0 ? time : "-");
    if (game.dailyCount() > 0) {
      snprintf(lines[count++], sizeof(lines[0]), tr(STR_SUDOKU_DAILY_RECORD), static_cast<unsigned>(game.dailyCount()),
               static_cast<unsigned>(game.bestDailyStreak()));
    }
  }
  for (int i = 0; i < count; ++i) {
    gameui::centredText(renderer, UI_10_FONT_ID, {panelBox.x, panelBox.y + 36 + i * 26, panelBox.w, 26},
                        renderer.truncatedText(UI_10_FONT_ID, lines[i], panelBox.w).c_str());
  }
  gameui::button(renderer, solvedNewBox, tr(STR_SUDOKU_NEW), true);
}

void SudokuActivity::drawCheckPanel() const {
  const int wrong = game.wrongCells();
  char title[64];
  if (wrong == 1) {
    snprintf(title, sizeof(title), "%s", tr(STR_SUDOKU_ONE_WRONG));
  } else {
    snprintf(title, sizeof(title), tr(STR_SUDOKU_WRONG_COUNT), static_cast<unsigned>(wrong));
  }
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y, panelBox.w, 36}, title, true,
                      EpdFontFamily::BOLD);
  const char* labels[kCheckButtonCount] = {tr(STR_SUDOKU_SHOW_WRONG), tr(STR_SUDOKU_CLEAR_WRONG),
                                           tr(STR_SUDOKU_KEEP_LOOKING)};
  for (int i = 0; i < kCheckButtonCount; ++i) gameui::button(renderer, checkBoxes[i], labels[i], i == ShowWrong);
}

int SudokuActivity::drawPanelLines(const char* title, const char* body, const int maxLines) const {
  gameui::fittedText(renderer, {panelBox.x, panelBox.y, panelBox.w, 36}, title);
  int y = panelBox.y + 40;
  if (body == nullptr) return y;
  for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, body, panelBox.w - 16, maxLines)) {
    gameui::centredText(renderer, UI_10_FONT_ID, {panelBox.x, y, panelBox.w, 26}, line.c_str());
    y += 26;
  }
  return y;
}

void SudokuActivity::drawHintPanel() const {
  char title[64];
  char body[200];
  char unit[24];
  unitName(hintStep.unit, unit, sizeof(unit));
  const unsigned digit = hintStep.digit;
  // After a harder rule the units no longer hold every other number themselves.
  const bool after = hintStep.before != sudoku::Step::Rule::None;
  switch (hintStep.kind) {
    case sudoku::Step::Kind::Wrong:
      snprintf(title, sizeof(title), "%s", tr(STR_SUDOKU_HINT_WRONG_T));
      snprintf(body, sizeof(body), tr(STR_SUDOKU_HINT_WRONG), digit);
      break;
    case sudoku::Step::Kind::LastInUnit:
      snprintf(title, sizeof(title), "%s", tr(STR_SUDOKU_HINT_LAST_T));
      snprintf(body, sizeof(body), tr(STR_SUDOKU_HINT_LAST), unit, digit);
      break;
    case sudoku::Step::Kind::OnlyNumber:
      snprintf(title, sizeof(title), "%s", tr(STR_SUDOKU_HINT_NUMBER_T));
      snprintf(body, sizeof(body), after ? tr(STR_SUDOKU_HINT_NUMBER_AFTER) : tr(STR_SUDOKU_HINT_NUMBER), digit);
      break;
    case sudoku::Step::Kind::OnlyPlace:
      snprintf(title, sizeof(title), tr(STR_SUDOKU_HINT_PLACE_T), digit);
      snprintf(body, sizeof(body), after ? tr(STR_SUDOKU_HINT_PLACE_AFTER) : tr(STR_SUDOKU_HINT_PLACE), unit, digit);
      break;
    case sudoku::Step::Kind::None:
      snprintf(title, sizeof(title), "%s", tr(STR_SUDOKU_HINT_NONE_T));
      snprintf(body, sizeof(body), "%s", tr(STR_SUDOKU_HINT_NONE));
      break;
  }
  // A single that only shows once a harder rule has ruled numbers out says so first.
  if (after) {
    const char* rule = hintStep.before == sudoku::Step::Rule::Pair ? tr(STR_SUDOKU_HINT_AFTER_PAIR)
                                                                   : tr(STR_SUDOKU_HINT_AFTER_LINE);
    char both[sizeof(body)];
    snprintf(both, sizeof(both), "%s %s", rule, body);
    std::memcpy(body, both, sizeof(body));
  }
  drawPanelLines(title, body, 4);
  const bool wrong = hintStep.kind == sudoku::Step::Kind::Wrong;
  gameui::button(renderer, pairBoxes[0], wrong ? tr(STR_SUDOKU_ERASE_IT) : tr(STR_SUDOKU_SHOW_ANSWER), false);
  gameui::button(renderer, pairBoxes[1], tr(STR_SUDOKU_GOT_IT), true);
}

void SudokuActivity::drawOverPanel() const {
  drawPanelLines(tr(STR_SUDOKU_GAME_OVER), nullptr, 0);
  gameui::button(renderer, pairBoxes[0], tr(STR_SUDOKU_TRY_AGAIN), true);
  gameui::button(renderer, pairBoxes[1], tr(STR_SUDOKU_NEW), false);
}

void SudokuActivity::drawHeaderLine() const {
  // The level (or Daily), small, between the title and New. Nothing here counts
  // mistakes, which would say a digit is wrong before the player has seen it,
  // unless the mistake limit is on.
  char text[40];
  if (options.on[MistakeLimit]) {
    snprintf(text, sizeof(text), tr(STR_SUDOKU_MISTAKES), static_cast<unsigned>(game.mistakes()));
  } else {
    snprintf(text, sizeof(text), "%s", game.dailyDay() != 0 ? tr(STR_SUDOKU_DAILY) : levelName(game.difficulty()));
  }
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  const int left = TouchHeaderBackButton::layout(header).titleX +
                   renderer.getTextWidth(UI_12_FONT_ID, tr(STR_GAME_SUDOKU), EpdFontFamily::BOLD) + 12;
  const int centreY = gameui::headerTitleCentreY(renderer, mappedInput);
  const int width = newHold.box.x - 8 - left;
  gameui::centredText(renderer, UI_10_FONT_ID, {left, centreY - 16, width, 34},
                      renderer.truncatedText(UI_10_FONT_ID, text, width).c_str());
}

void SudokuActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();

  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_SUDOKU));
  if ((panel == Panel::Pad || panel == Panel::Hint) && game.started()) {
    drawHeaderLine();
    if (panel == Panel::Pad) newHold.draw(renderer, tr(STR_GAME_NEW_SHORT), tr(STR_HOLD_FOR_NEW_GAME));
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
    case Panel::Check:
      drawCheckPanel();
      break;
    case Panel::Hint:
      drawHintPanel();
      break;
    case Panel::Making:
      gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y + 40, panelBox.w, 36},
                          tr(STR_SUDOKU_MAKING), true, EpdFontFamily::BOLD);
      break;
    case Panel::Over:
      drawOverPanel();
      break;
  }

  pointer.clear();
  if (panel == Panel::NewPuzzle) {
    for (int i = 0; i < sudoku::kLevelCount; ++i) pointer.add(levelBoxes[i]);
    if (options.on[DailyPuzzle] && day != 0 && !game.dailySolved(day)) pointer.add(levelBoxes[kDailyBox]);
    if (game.started()) pointer.add(cancelBox);
    if (restartBox.w > 0) pointer.add(restartBox);
  } else if (panel == Panel::Solved) {
    pointer.add(solvedNewBox);
  } else if (panel == Panel::Check) {
    for (const auto& b : checkBoxes) pointer.add(b);
  } else if (panel == Panel::Hint || panel == Panel::Over) {
    for (const auto& b : pairBoxes) pointer.add(b);
  } else if (panel == Panel::Pad) {
    for (int cell = 0; cell < 81; ++cell) {
      pointer.add({grid.x + (cell % 9) * cellSize, grid.y + (cell / 9) * cellSize, cellSize, cellSize});
    }
    for (const auto& b : digitKeys) pointer.add(b);
    for (const auto& b : tools) pointer.add(b);
    if (game.started()) pointer.add(newHold.box);
  }
  pointer.draw(renderer);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
