#include "AppsActivity.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <memory>

#include "CheckersActivity.h"
#include "ChessActivity.h"
#include "ClockActivity.h"
#include "MappedInputManager.h"
#include "SudokuActivity.h"
#include "WordleActivity.h"
#include "fontIds.h"

namespace {

constexpr int kColumns = 2;
constexpr int kTileMargin = 16;
constexpr int kTileGap = 16;
// Three rows of tiles fill the screen between the shared top and foot margins.
constexpr int kTileHeight = 219;
constexpr int kIconSize = 100;
constexpr int kIconTop = 21;
constexpr int kNameTop = 135;
constexpr int kStatusTop = 171;
constexpr int kRowHeight = 92;
constexpr int kRowGap = 12;
constexpr uint8_t kFileMagic[3] = {'V', 'A', 'P'};

// Three rows of three letter tiles in the game's own three looks, the last row solved.
void drawWordleIcon(const GfxRenderer& renderer, const int x, const int y) {
  enum Look : uint8_t { Open, Grey, Black };
  static constexpr Look kLooks[9] = {Open, Grey, Black, Grey, Black, Open, Black, Black, Black};
  constexpr int gap = 5;
  constexpr int tile = (kIconSize - 2 * gap) / 3;
  constexpr int radius = 5;
  for (int i = 0; i < 9; ++i) {
    const int tileX = x + (i % 3) * (tile + gap);
    const int tileY = y + (i / 3) * (tile + gap);
    if (kLooks[i] == Black) {
      renderer.fillRoundedRect(tileX, tileY, tile, tile, radius, Color::Black);
      continue;
    }
    if (kLooks[i] == Grey) renderer.fillRoundedRect(tileX, tileY, tile, tile, radius, Color::LightGray);
    renderer.drawRoundedRect(tileX, tileY, tile, tile, 2, radius, true);
  }
}

// One box of a Sudoku grid, heavy frame, with a few digits in it.
void drawSudokuIcon(const GfxRenderer& renderer, const int x, const int y) {
  constexpr int cell = kIconSize / 3;
  constexpr int size = cell * 3;
  for (int i = 0; i <= 3; ++i) {
    const int thickness = (i == 0 || i == 3) ? 3 : 1;
    renderer.fillRect(x + i * cell - thickness / 2, y - 1, thickness, size + 3, true);
    renderer.fillRect(x - 1, y + i * cell - thickness / 2, size + 3, thickness, true);
  }
  static constexpr uint8_t kDigits[9] = {5, 0, 3, 0, 7, 0, 6, 0, 9};
  for (int i = 0; i < 9; ++i) {
    if (kDigits[i] == 0) continue;
    gameui::bigDigit(renderer, kDigits[i], true, {x + (i % 3) * cell, y + (i / 3) * cell, cell, cell},
                     gameui::Ink::Black);
  }
}

// Four squares of a board, two of them hatched, under whatever stands on them.
void drawBoardCorner(const GfxRenderer& renderer, const int x, const int y) {
  constexpr int half = kIconSize / 2;
  gameui::hatch(renderer, {x + half, y, half, half});
  gameui::hatch(renderer, {x, y + half, half, half});
  renderer.drawRect(x - 2, y - 2, kIconSize + 4, kIconSize + 4, 2, true);
}

void drawChessIcon(const GfxRenderer& renderer, const int x, const int y) {
  drawBoardCorner(renderer, x, y);
  const appart::Bitmap& knight = appart::kMenuKnight;
  gameui::stampOver(renderer, knight, appart::kMenuKnightUnder, x + (kIconSize - knight.width) / 2,
                    y + (kIconSize - knight.height) / 2);
}

void drawCheckersIcon(const GfxRenderer& renderer, const int x, const int y) {
  drawBoardCorner(renderer, x, y);
  constexpr int half = kIconSize / 2;
  constexpr int radius = half / 2 - 5;
  // A dark piece and a light one on the two hatched squares.
  const int darkX = x + half + half / 2;
  const int darkY = y + half / 2;
  gameui::disc(renderer, darkX, darkY, radius + 2, false);
  gameui::disc(renderer, darkX, darkY, radius, true);
  gameui::ring(renderer, darkX, darkY, radius - 5, 2, false);
  const int lightX = x + half / 2;
  const int lightY = y + half + half / 2;
  gameui::disc(renderer, lightX, lightY, radius + 2, false);
  gameui::ring(renderer, lightX, lightY, radius, 3, true);
  gameui::ring(renderer, lightX, lightY, radius - 6, 1, true);
}

// A calendar leaf showing today: the month across the top, the day below.
void drawClockIcon(const GfxRenderer& renderer, const int x, const int y) {
  renderer.drawRoundedRect(x, y, kIconSize, kIconSize, 3, 12, true);
  renderer.fillRoundedRect(x, y, kIconSize, 30, 12, true, true, false, false, Color::Black);
  calmath::DateTime time;
  if (!clockface::now(time)) {
    gameui::centredText(renderer, UI_12_FONT_ID, {x, y + 30, kIconSize, kIconSize - 30}, "--", true,
                        EpdFontFamily::BOLD);
    return;
  }
  // The first three letters of the month.
  char month[16];
  snprintf(month, sizeof(month), "%s", clockface::monthName(time.date.month));
  size_t length = 0;
  for (int characters = 0; month[length] != '\0'; ++length) {
    if ((static_cast<uint8_t>(month[length]) & 0xC0) != 0x80 && characters++ == 3) break;
  }
  month[length] = '\0';
  gameui::centredText(renderer, SMALL_FONT_ID, {x, y + 2, kIconSize, 26}, month, false, EpdFontFamily::BOLD);
  char day[4];
  snprintf(day, sizeof(day), "%d", static_cast<int>(time.date.day));
  const gameui::DigitSet digits{appart::kClockSmallGlyphs, appart::kClockSmallBits, appart::kClockSmallRows};
  gameui::digitText(renderer, digits, day, x + kIconSize / 2, y + 30 + (kIconSize - 30 - digits.rows) / 2, 4);
}

void drawSwitch(const GfxRenderer& renderer, const int x, const int y, const bool on) {
  constexpr int width = 66;
  constexpr int height = 36;
  if (on) {
    renderer.fillRoundedRect(x, y, width, height, height / 2, Color::Black);
    gameui::disc(renderer, x + width - height / 2, y + height / 2, 13, false);
  } else {
    renderer.drawRoundedRect(x, y, width, height, 2, height / 2, true);
    gameui::disc(renderer, x + height / 2, y + height / 2, 12, true);
  }
}

}  // namespace

AppsActivity::AppsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Apps", renderer, mappedInput) {}

void AppsActivity::onEnter() {
  Activity::onEnter();
  uint8_t bytes[4];
  if (gameui::readSave(kSavePath, bytes, sizeof(bytes)) && memcmp(bytes, kFileMagic, 3) == 0) hiddenMask = bytes[3];
  mode = Mode::Tiles;
  clockface::loadOptions(clockOptions);
  readStatus();
  requestUpdate();
}

void AppsActivity::saveHidden() const {
  const uint8_t bytes[4] = {kFileMagic[0], kFileMagic[1], kFileMagic[2], hiddenMask};
  gameui::writeSave(kSavePath, bytes, sizeof(bytes));
}

void AppsActivity::readStatus() {
  char line[96];

  wordle::Game wordleGame;
  uint8_t wordleBytes[wordle::Game::kSaveBytes];
  if (gameui::readSave(WordleActivity::kSavePath, wordleBytes, sizeof(wordleBytes))) {
    wordleGame.load(wordleBytes, sizeof(wordleBytes));
  }
  if (wordleGame.started() && wordleGame.state() == wordle::Game::State::Playing && wordleGame.guessCount() > 0) {
    snprintf(line, sizeof(line), tr(STR_WORDLE_IN_PROGRESS), wordleGame.guessCount() + 1);
  } else if (wordleGame.stats().played > 0) {
    snprintf(line, sizeof(line), tr(STR_WORDLE_RECORD), static_cast<unsigned>(wordleGame.stats().played),
             static_cast<unsigned>(wordleGame.stats().streak));
  } else {
    line[0] = '\0';
  }
  status[Wordle] = line;

  {
    // Larger than the main task's stack should carry next to the lines above.
    const auto sudokuGame = std::make_unique<sudoku::Game>();
    uint8_t sudokuBytes[sudoku::Game::kSaveBytes];
    if (gameui::readSave(SudokuActivity::kSavePath, sudokuBytes, sizeof(sudokuBytes))) {
      sudokuGame->load(sudokuBytes, sizeof(sudokuBytes));
    }
    if (sudokuGame->started() && !sudokuGame->solved()) {
      snprintf(line, sizeof(line), tr(STR_SUDOKU_IN_PROGRESS), SudokuActivity::levelName(sudokuGame->difficulty()),
               sudokuGame->emptyCells());
    } else {
      const unsigned solved = sudokuGame->solvedCount(sudoku::Difficulty::Easy) +
                              sudokuGame->solvedCount(sudoku::Difficulty::Medium) +
                              sudokuGame->solvedCount(sudoku::Difficulty::Hard);
      if (solved > 0) {
        snprintf(line, sizeof(line), tr(STR_SUDOKU_RECORD), solved);
      } else {
        line[0] = '\0';
      }
    }
    status[Sudoku] = line;
  }

  ChessActivity::statusLine(line, sizeof(line));
  status[Chess] = line;
  CheckersActivity::statusLine(line, sizeof(line));
  status[Checkers] = line;
  ClockActivity::statusLine(line, sizeof(line));
  status[Clock] = line;
}

void AppsActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int top = gameui::contentTop(renderer, mappedInput);
  const int tileWidth = (width - 2 * kTileMargin - (kColumns - 1) * kTileGap) / kColumns;
  shownCount = 0;
  for (int app = 0; app < kAppCount; ++app) {
    if (hidden(app)) continue;
    tiles[shownCount] = {kTileMargin + (shownCount % kColumns) * (tileWidth + kTileGap),
                         top + (shownCount / kColumns) * (kTileHeight + kTileGap), tileWidth, kTileHeight};
    shown[shownCount++] = app;
  }
  for (int app = 0; app < kAppCount; ++app) {
    rows[app] = {kTileMargin, top + app * (kRowHeight + kRowGap), width - 2 * kTileMargin, kRowHeight};
  }
  manageBox = gameui::headerCornerBox(renderer, mappedInput, tr(STR_APPS_MANAGE));

  // Settings, on the Clock's row in Manage, to the left of its switch.
  const gameui::Box& clockRow = rows[Clock];
  const int settingsWidth = renderer.getTextWidth(UI_10_FONT_ID, tr(STR_SETTINGS_SHORT), EpdFontFamily::BOLD) + 40;
  clockSettingsBox = {clockRow.x + clockRow.w - 66 - 22 - 14 - settingsWidth, clockRow.y + (clockRow.h - 50) / 2,
                      settingsWidth, 50};
  for (int row = 0; row < kClockRowCount; ++row) {
    clockRows[row] = {kTileMargin, top + row * (84 + kRowGap), width - 2 * kTileMargin, 84};
  }
}

void AppsActivity::changeClockSetting(const int row) {
  switch (row) {
    case FaceRow:
      // The face with Persian days needs the Persian date on.
      do {
        clockOptions.face = static_cast<uint8_t>((clockOptions.face + 1) % clockface::kFaceCount);
      } while (clockOptions.face == clockface::PersianDays && !clockOptions.persianDate);
      break;
    case PersianRow:
      clockOptions.persianDate = !clockOptions.persianDate;
      if (!clockOptions.persianDate && clockOptions.face == clockface::PersianDays) {
        clockOptions.face = clockface::Digital;
      }
      break;
    case WeekStartRow:
      clockOptions.weekStart = static_cast<uint8_t>((clockOptions.weekStart + 1) % clockface::kWeekStartCount);
      break;
    default:
      clockOptions.weekNumber = !clockOptions.weekNumber;
      break;
  }
  clockface::saveOptions(clockOptions);
}

void AppsActivity::open(const AppId app) {
  std::unique_ptr<Activity> activity;
  switch (app) {
    case Wordle:
      activity = std::make_unique<WordleActivity>(renderer, mappedInput);
      break;
    case Sudoku:
      activity = std::make_unique<SudokuActivity>(renderer, mappedInput);
      break;
    case Chess:
      activity = std::make_unique<ChessActivity>(renderer, mappedInput);
      break;
    case Checkers:
      activity = std::make_unique<CheckersActivity>(renderer, mappedInput);
      break;
    default:
      activity = std::make_unique<ClockActivity>(renderer, mappedInput);
      break;
  }
  startActivityForResult(std::move(activity), [this](const ActivityResult&) {
    readStatus();
    requestUpdate();
  });
}

void AppsActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (mode != Mode::Tiles) {
      // One step back: from the Clock settings to the switches, from there to the tiles.
      mode = mode == Mode::ClockSettings ? Mode::Manage : Mode::Tiles;
      requestUpdate();
      return;
    }
    setResult(ActivityResult{});
    finish();
    return;
  }

  int tx = 0;
  int ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return;
  if (mode == Mode::ClockSettings) {
    for (int row = 0; row < kClockRowCount; ++row) {
      if (!clockRows[row].contains(tx, ty)) continue;
      changeClockSetting(row);
      requestUpdate();
      return;
    }
    return;
  }
  if (mode == Mode::Manage) {
    if (clockSettingsBox.grown(6).contains(tx, ty)) {
      mode = Mode::ClockSettings;
      requestUpdate();
      return;
    }
    for (int app = 0; app < kAppCount; ++app) {
      if (!rows[app].contains(tx, ty)) continue;
      hiddenMask ^= static_cast<uint8_t>(1u << app);
      saveHidden();
      requestUpdate();
      return;
    }
    return;
  }
  if (manageBox.grown(6).contains(tx, ty)) {
    mode = Mode::Manage;
    requestUpdate();
    return;
  }
  for (int i = 0; i < shownCount; ++i) {
    if (tiles[i].contains(tx, ty)) {
      open(static_cast<AppId>(shown[i]));
      return;
    }
  }
}

void AppsActivity::drawIcon(const int app, const int x, const int y) const {
  switch (app) {
    case Wordle:
      drawWordleIcon(renderer, x, y);
      break;
    case Sudoku:
      drawSudokuIcon(renderer, x, y);
      break;
    case Chess:
      drawChessIcon(renderer, x, y);
      break;
    case Checkers:
      drawCheckersIcon(renderer, x, y);
      break;
    default:
      drawClockIcon(renderer, x, y);
      break;
  }
}

void AppsActivity::drawTiles() const {
  const char* names[kAppCount] = {tr(STR_GAME_WORDLE), tr(STR_GAME_SUDOKU), tr(STR_GAME_CHESS),
                                  tr(STR_GAME_CHECKERS), tr(STR_APP_CLOCK)};
  for (int i = 0; i < shownCount; ++i) {
    const int app = shown[i];
    const gameui::Box& tile = tiles[i];
    renderer.drawRoundedRect(tile.x, tile.y, tile.w, tile.h, 2, 12, true);
    drawIcon(app, tile.x + (tile.w - kIconSize) / 2, tile.y + kIconTop);
    gameui::centredText(renderer, UI_12_FONT_ID, {tile.x, tile.y + kNameTop, tile.w, 30}, names[app], true,
                        EpdFontFamily::BOLD);
    if (!status[app].empty()) {
      gameui::centredText(renderer, SMALL_FONT_ID, {tile.x, tile.y + kStatusTop, tile.w, 22},
                          renderer.truncatedText(SMALL_FONT_ID, status[app].c_str(), tile.w - 16).c_str());
    }
  }
  if (shownCount == 0) {
    gameui::centredText(renderer, UI_10_FONT_ID, {0, tiles[0].y + 40, renderer.getScreenWidth(), 30},
                        tr(STR_APPS_ALL_HIDDEN));
  }
}

void AppsActivity::drawManage() const {
  const char* names[kAppCount] = {tr(STR_GAME_WORDLE), tr(STR_GAME_SUDOKU), tr(STR_GAME_CHESS),
                                  tr(STR_GAME_CHECKERS), tr(STR_APP_CLOCK)};
  for (int app = 0; app < kAppCount; ++app) {
    const gameui::Box& row = rows[app];
    renderer.drawRoundedRect(row.x, row.y, row.w, row.h, 2, 12, true);
    const gameui::Box label{row.x + 24, row.y + 6, 190, row.h - 12};
    renderer.drawText(UI_12_FONT_ID, label.x, gameui::centredTextY(renderer, UI_12_FONT_ID, label), names[app], true,
                      EpdFontFamily::BOLD);
    if (hidden(app)) gameui::fade(renderer, label);
    drawSwitch(renderer, row.x + row.w - 66 - 22, row.y + (row.h - 36) / 2, !hidden(app));
    if (app == Clock) gameui::button(renderer, clockSettingsBox, tr(STR_SETTINGS_SHORT), false);
  }
  const int y = rows[kAppCount - 1].y + kRowHeight + 14;
  int lineY = y;
  for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, tr(STR_APPS_HIDDEN_NOTE),
                                               renderer.getScreenWidth() - 60, 3)) {
    gameui::centredText(renderer, SMALL_FONT_ID, {0, lineY, renderer.getScreenWidth(), 24}, line.c_str());
    lineY += 24;
  }
}

void AppsActivity::drawClockSettings() const {
  const char* faces[clockface::kFaceCount] = {tr(STR_CLOCK_FACE_DIGITAL), tr(STR_CLOCK_FACE_ANALOG),
                                              tr(STR_CLOCK_FACE_PERSIAN)};
  // The weekdays are numbered from Monday.
  const int firstDay = clockOptions.weekStart == clockface::Saturday
                           ? 5
                           : (clockOptions.weekStart == clockface::Sunday ? 6 : 0);
  const char* labels[kClockRowCount] = {tr(STR_CLOCK_FACE), tr(STR_CLOCK_PERSIAN_DATE), tr(STR_CLOCK_WEEK_START),
                                        tr(STR_CLOCK_WEEK_NUMBER)};
  const char* values[kClockRowCount] = {faces[clockOptions.shownFace()],
                                        clockOptions.persianDate ? tr(STR_STATE_ON) : tr(STR_STATE_OFF),
                                        clockface::weekdayName(firstDay),
                                        clockOptions.weekNumber ? tr(STR_STATE_ON) : tr(STR_STATE_OFF)};
  for (int row = 0; row < kClockRowCount; ++row) {
    const gameui::Box& box = clockRows[row];
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 12, true);
    renderer.drawText(UI_12_FONT_ID, box.x + 24, gameui::centredTextY(renderer, UI_12_FONT_ID, box), labels[row], true,
                      EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, box.x + box.w - 24 - renderer.getTextWidth(UI_10_FONT_ID, values[row]),
                      gameui::centredTextY(renderer, UI_10_FONT_ID, box), values[row]);
  }
  int y = clockRows[kClockRowCount - 1].y + 84 + 14;
  for (const char* note : {tr(STR_CLOCK_TAP_ROW), tr(STR_CLOCK_FORMAT_NOTE)}) {
    for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, note, renderer.getScreenWidth() - 60, 3)) {
      gameui::centredText(renderer, SMALL_FONT_ID, {0, y, renderer.getScreenWidth(), 24}, line.c_str());
      y += 24;
    }
  }
}

void AppsActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  const char* title = mode == Mode::ClockSettings
                          ? tr(STR_CLOCK_SETTINGS)
                          : (mode == Mode::Manage ? tr(STR_APPS_MANAGE_TITLE) : tr(STR_APPS));
  gameui::drawHeader(renderer, mappedInput, title);
  if (mode == Mode::ClockSettings) {
    drawClockSettings();
  } else if (mode == Mode::Manage) {
    drawManage();
  } else {
    gameui::cornerButton(renderer, manageBox, tr(STR_APPS_MANAGE));
    drawTiles();
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
