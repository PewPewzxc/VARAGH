#include "WordleActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/reader/DictionaryDefinitionActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/keyboardIcons.h"
#include "fontIds.h"
#include "util/Dictionary.h"
#include "util/DictionaryRegistry.h"

namespace {

constexpr char kRows[3][11] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
constexpr int kSideMargin = 4;
constexpr int kKeyGap = 4;
constexpr int kKeyHeight = 70;
constexpr int kTileGap = 7;
constexpr int kMaxTile = 68;
constexpr int kNoticeHeight = 28;

}  // namespace

WordleActivity::WordleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Wordle", renderer, mappedInput) {}

void WordleActivity::onEnter() {
  Activity::onEnter();
  uint8_t bytes[wordle::Game::kSaveBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || !game.load(bytes, sizeof(bytes))) {
    LOG_DBG("WORDLE", "No saved round");
  }
  if (!game.started()) startRound();
  notice = Notice::None;
  requestUpdate();
}

void WordleActivity::onExit() {
  save();
  Activity::onExit();
}

void WordleActivity::save() {
  if (!dirty) return;
  uint8_t bytes[wordle::Game::kSaveBytes];
  game.save(bytes);
  if (gameui::writeSave(kSavePath, bytes, sizeof(bytes))) {
    dirty = false;
  } else {
    LOG_ERR("WORDLE", "Could not save the round");
  }
}

void WordleActivity::startRound() {
  // Never the same word twice in a row.
  uint16_t index = static_cast<uint16_t>(random(wordle::answerCount()));
  if (game.started() && index == game.answerIndex()) index = static_cast<uint16_t>((index + 1) % wordle::answerCount());
  game.start(index);
  notice = Notice::None;
  dirty = true;
}

void WordleActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int top = gameui::contentTop(renderer, mappedInput);
  const int bottom = gameui::contentBottom(renderer);

  // The keyboard ends where the lowest controls of the other apps end.
  const int keyboardHeight = 3 * kKeyHeight + 2 * kKeyGap;
  const int keyboardY = bottom - keyboardHeight;
  const int noticeY = keyboardY - kNoticeHeight;
  const int gridSpace = noticeY - top;
  const int tile = std::clamp((gridSpace - (wordle::kMaxGuesses - 1) * kTileGap) / wordle::kMaxGuesses, 24, kMaxTile);
  const int gridWidth = wordle::kWordLength * tile + (wordle::kWordLength - 1) * kTileGap;
  const int gridHeight = wordle::kMaxGuesses * tile + (wordle::kMaxGuesses - 1) * kTileGap;
  const int gridX = (width - gridWidth) / 2;
  const int gridY = top + std::max(0, (gridSpace - gridHeight) / 2);
  for (int row = 0; row < wordle::kMaxGuesses; ++row) {
    for (int column = 0; column < wordle::kWordLength; ++column) {
      tiles[row][column] = {gridX + column * (tile + kTileGap), gridY + row * (tile + kTileGap), tile, tile};
    }
  }
  noticeBox = {0, noticeY, width, kNoticeHeight};

  // Ten keys span the top row; the third row has wider Enter and Backspace keys at its ends.
  const int keyWidth = (width - 2 * kSideMargin - 9 * kKeyGap) / 10;
  const int rowWidth = 10 * keyWidth + 9 * kKeyGap;
  const int rowX = (width - rowWidth) / 2;
  for (int row = 0; row < 3; ++row) {
    const int letters = static_cast<int>(strlen(kRows[row]));
    const int lettersWidth = letters * keyWidth + (letters - 1) * kKeyGap;
    const int y = keyboardY + row * (kKeyHeight + kKeyGap);
    const int x = (width - lettersWidth) / 2;
    for (int i = 0; i < letters; ++i) {
      keys[kRows[row][i] - 'a'] = {x + i * (keyWidth + kKeyGap), y, keyWidth, kKeyHeight};
    }
    if (row == 2) {
      const int wide = x - kKeyGap - rowX;
      keys[kEnterKey] = {rowX, y, wide, kKeyHeight};
      keys[kBackspaceKey] = {x + lettersWidth + kKeyGap, y, wide, kKeyHeight};
    }
  }
  giveUpHold.box = gameui::headerCornerBox(renderer, mappedInput, tr(STR_WORDLE_GIVE_UP));

  panelBox = {rowX, noticeY, rowWidth, bottom - noticeY};
  const int half = (rowWidth - 12) / 2;
  lookUpBox = {rowX, bottom - 50, half, 50};
  newGameBox = {rowX + half + 12, bottom - 50, half, 50};
}

// Opens the answer in an installed dictionary whose headwords are English
// (English-English first), whatever dictionary the reader currently uses.
void WordleActivity::lookUpAnswer() {
  std::string englishPath;
  dictionaryRegistry.discover();
  for (const DictionaryEntry& entry : dictionaryRegistry.getEntries()) {
    const DictInfo info = Dictionary::readInfo(entry.basePath.c_str());
    if (!info.valid || strncmp(info.lang, "en", 2) != 0) continue;
    if (englishPath.empty() || strcmp(info.lang, "en-en") == 0) englishPath = entry.basePath;
  }
  dictionaryRegistry.clear();

  DictLocation location;
  if (!englishPath.empty()) {
    // The definition screen clears this override again when it closes.
    Dictionary::setLookupDictPathOverride(englishPath.c_str());
    location = Dictionary::locate(game.answer());
  }
  if (!location.found) {
    Dictionary::clearLookupDictPathOverride();
    notice = Notice::NoDictionary;
    requestUpdate();
    return;
  }
  startActivityForResult(
      std::make_unique<DictionaryDefinitionActivity>(renderer, mappedInput, location.headword, location),
      [this](const ActivityResult&) { requestUpdate(); });
}

void WordleActivity::pressKey(const int key) {
  notice = Notice::None;
  if (key == kBackspaceKey) {
    dirty = game.removeLetter() || dirty;
  } else if (key == kEnterKey) {
    switch (game.submit()) {
      case wordle::Game::Submit::TooShort:
        notice = Notice::TooShort;
        break;
      case wordle::Game::Submit::NotAWord:
        notice = Notice::NotAWord;
        break;
      case wordle::Game::Submit::Accepted:
        dirty = true;
        break;
      case wordle::Game::Submit::Won:
      case wordle::Game::Submit::Lost:
        dirty = true;
        save();
        break;
    }
  } else {
    dirty = game.addLetter(static_cast<char>('a' + key)) || dirty;
  }
  requestUpdate();
}

void WordleActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  // Give up is held, not tapped: it fills while the finger rests on it.
  bool repaint = false;
  if (game.state() == wordle::Game::State::Playing && game.guessCount() > 0 &&
      giveUpHold.update(mappedInput, repaint)) {
    game.giveUp();
    dirty = true;
    save();
    requestUpdate();
    return;
  }
  if (repaint) requestUpdate();

  int tx = 0;
  int ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return;

  if (game.state() != wordle::Game::State::Playing) {
    if (newGameBox.contains(tx, ty)) {
      startRound();
      save();
      requestUpdate();
    } else if (lookUpBox.contains(tx, ty)) {
      lookUpAnswer();
    }
    return;
  }

  // A tile of the row being typed takes the marker.
  for (int column = 0; column < wordle::kWordLength; ++column) {
    if (!tiles[game.guessCount()][column].grown(kTileGap / 2).contains(tx, ty)) continue;
    if (game.cursor() != column && game.setCursor(column)) {
      notice = Notice::None;
      requestUpdate();
    }
    return;
  }

  // Keys share the gap between them, so a tap never falls between two keys.
  for (int key = 0; key < kKeyCount; ++key) {
    if (keys[key].grown(kKeyGap / 2 + 1).contains(tx, ty)) {
      pressKey(key);
      return;
    }
  }
}

void WordleActivity::drawTile(const gameui::Box& box, const char letter, const wordle::Mark mark,
                              const bool activeRow, const bool marked) const {
  constexpr int radius = 8;
  switch (mark) {
    case wordle::Mark::Correct:
      renderer.fillRoundedRect(box.x, box.y, box.w, box.h, radius, Color::Black);
      gameui::bigLetter(renderer, letter, box, gameui::Ink::White);
      return;
    case wordle::Mark::Present:
      renderer.fillRoundedRect(box.x, box.y, box.w, box.h, radius, Color::LightGray);
      renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 3, radius, true);
      gameui::bigLetter(renderer, letter, box, gameui::Ink::Rimmed);
      return;
    case wordle::Mark::Absent:
      renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, radius, true);
      gameui::bigLetter(renderer, letter, box, gameui::Ink::Black);
      gameui::fade(renderer, box);
      return;
    case wordle::Mark::None:
      break;
  }
  if (activeRow) {
    // Holding a letter or marked: a heavy frame. The marked tile also carries
    // a bar along its foot, with or without a letter above it.
    const bool filled = letter != '\0';
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, (filled || marked) ? 3 : 2, radius, true);
    if (filled) gameui::bigLetter(renderer, letter, box, gameui::Ink::Black);
    if (marked) renderer.fillRect(box.x + 9, box.y + box.h - 10, box.w - 18, 5, true);
  } else {
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 1, radius, true);
  }
}

void WordleActivity::drawKeyboard() const {
  constexpr int radius = 6;
  for (int key = 0; key < kKeyCount; ++key) {
    const gameui::Box& box = keys[key];
    if (key == kEnterKey) {
      gameui::button(renderer, box, tr(STR_WORDLE_ENTER), game.typedLength() == wordle::kWordLength, UI_10_FONT_ID);
      continue;
    }
    if (key == kBackspaceKey) {
      renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, radius, true);
      drawLucideIcon(renderer, icon_backspace_28, box.x + (box.w - icon_backspace_28.w) / 2,
                     box.y + (box.h - icon_backspace_28.h) / 2);
      continue;
    }
    const char text[2] = {static_cast<char>('A' + key), '\0'};
    switch (game.letterMark(static_cast<char>('a' + key))) {
      case wordle::Mark::Correct:
        renderer.fillRoundedRect(box.x, box.y, box.w, box.h, radius, Color::Black);
        gameui::centredText(renderer, UI_12_FONT_ID, box, text, false, EpdFontFamily::BOLD);
        break;
      case wordle::Mark::Present:
        renderer.fillRoundedRect(box.x, box.y, box.w, box.h, radius, Color::LightGray);
        renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, radius, true);
        gameui::rimmedText(renderer, UI_12_FONT_ID, box, text);
        break;
      case wordle::Mark::Absent:
        renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, radius, true);
        gameui::centredText(renderer, UI_12_FONT_ID, box, text, true, EpdFontFamily::REGULAR);
        gameui::fade(renderer, box);
        break;
      case wordle::Mark::None:
        renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, radius, true);
        gameui::centredText(renderer, UI_12_FONT_ID, box, text, true, EpdFontFamily::BOLD);
        break;
    }
  }
}

void WordleActivity::drawResult() const {
  const wordle::Stats& stats = game.stats();
  char line[64];
  if (game.state() == wordle::Game::State::Won) {
    snprintf(line, sizeof(line), tr(STR_WORDLE_SOLVED_IN), game.guessCount());
  } else {
    char upper[wordle::kWordLength + 1];
    for (int i = 0; i <= wordle::kWordLength; ++i) {
      const char c = game.answer()[i];
      upper[i] = c ? static_cast<char>(c - 'a' + 'A') : '\0';
    }
    snprintf(line, sizeof(line), tr(STR_WORDLE_WORD_WAS), upper);
  }
  int y = panelBox.y + 2;
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, y, panelBox.w, 30}, line, true, EpdFontFamily::BOLD);
  y += 34;

  // Four figures side by side: a bold number over its label.
  const unsigned winPercent = stats.played ? (100u * stats.won + stats.played / 2) / stats.played : 0;
  const char* labels[4] = {tr(STR_GAME_PLAYED), tr(STR_GAME_WON), tr(STR_GAME_STREAK), tr(STR_GAME_BEST)};
  char values[4][8];
  snprintf(values[0], sizeof(values[0]), "%u", static_cast<unsigned>(stats.played));
  snprintf(values[1], sizeof(values[1]), "%u%%", winPercent);
  snprintf(values[2], sizeof(values[2]), "%u", static_cast<unsigned>(stats.streak));
  snprintf(values[3], sizeof(values[3]), "%u", static_cast<unsigned>(stats.bestStreak));
  const int columnWidth = panelBox.w / 4;
  for (int i = 0; i < 4; ++i) {
    const int x = panelBox.x + i * columnWidth;
    gameui::centredText(renderer, UI_12_FONT_ID, {x, y, columnWidth, 26}, values[i], true, EpdFontFamily::BOLD);
    gameui::centredText(renderer, SMALL_FONT_ID, {x, y + 26, columnWidth, 20}, labels[i]);
  }
  y += 50;

  // How many rounds were solved with 1..6 guesses; this round's bar is solid.
  uint16_t most = 1;
  for (const uint16_t count : stats.solvedIn) most = std::max(most, count);
  const int barsBottom = lookUpBox.y - 8;
  const int rowHeight = std::clamp((barsBottom - y) / wordle::kMaxGuesses, 12, 22);
  const int labelWidth = 22;
  const int countWidth = 44;
  const int barSpace = panelBox.w - labelWidth - countWidth;
  for (int i = 0; i < wordle::kMaxGuesses; ++i) {
    const int rowY = y + i * rowHeight;
    char text[8];
    snprintf(text, sizeof(text), "%d", i + 1);
    gameui::centredText(renderer, SMALL_FONT_ID, {panelBox.x, rowY, labelWidth, rowHeight}, text);
    const int barWidth = std::max(4, barSpace * stats.solvedIn[i] / most);
    const gameui::Box bar{panelBox.x + labelWidth, rowY + 2, barWidth, rowHeight - 4};
    const bool thisRound = game.state() == wordle::Game::State::Won && game.guessCount() == i + 1;
    renderer.fillRect(bar.x, bar.y, bar.w, bar.h, true);
    if (!thisRound) gameui::fade(renderer, bar);
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(stats.solvedIn[i]));
    renderer.drawText(SMALL_FONT_ID, bar.x + bar.w + 8,
                      gameui::centredTextY(renderer, SMALL_FONT_ID, {0, rowY, 0, rowHeight}), text);
  }

  gameui::button(renderer, lookUpBox, tr(STR_WORDLE_LOOK_UP), false);
  gameui::button(renderer, newGameBox, tr(STR_GAME_NEW), true);
}

void WordleActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();

  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_WORDLE));

  const bool playing = game.state() == wordle::Game::State::Playing;
  for (int row = 0; row < wordle::kMaxGuesses; ++row) {
    for (int column = 0; column < wordle::kWordLength; ++column) {
      if (row < game.guessCount()) {
        drawTile(tiles[row][column], game.guess(row)[column], game.mark(row, column), false, false);
      } else if (row == game.guessCount() && playing) {
        drawTile(tiles[row][column], game.typedAt(column), wordle::Mark::None, true, column == game.cursor());
      } else {
        drawTile(tiles[row][column], '\0', wordle::Mark::None, false, false);
      }
    }
  }

  if (playing) {
    if (notice != Notice::None) {
      gameui::centredText(renderer, UI_10_FONT_ID, noticeBox,
                          notice == Notice::NotAWord ? tr(STR_WORDLE_NOT_IN_LIST) : tr(STR_WORDLE_TOO_SHORT), true,
                          EpdFontFamily::BOLD);
    }
    drawKeyboard();
    if (game.guessCount() > 0) giveUpHold.draw(renderer, tr(STR_WORDLE_GIVE_UP));
  } else {
    drawResult();
    if (notice == Notice::NoDictionary) {
      // Shown under the last row of tiles, above the result.
      const gameui::Box& lastTile = tiles[wordle::kMaxGuesses - 1][0];
      const int y = lastTile.y + lastTile.h;
      gameui::centredText(renderer, SMALL_FONT_ID, {0, y, renderer.getScreenWidth(), panelBox.y - y + 4},
                          tr(STR_WORDLE_NO_DICTIONARY));
    }
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
