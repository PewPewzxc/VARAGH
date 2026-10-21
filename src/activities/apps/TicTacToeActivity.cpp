#include "TicTacToeActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {

using tictactoe::Kind;
using tictactoe::Mark;
using tictactoe::State;

constexpr int kCell = 154;
constexpr int kStripHeight = 30;
constexpr int kTallyHeight = 70;
constexpr uint8_t kFileMagic[4] = {'V', 'T', 'T', '1'};
constexpr uint16_t kTallyMax = 999;
// The panel shows the player's own mark before the computer's answer.
constexpr unsigned long kComputerPauseMs = 450;
// A second tap of a quick double tap must not start the next round unseen.
constexpr unsigned long kNextRoundGuardMs = 700;

const char* levelName(const int level) {
  return level == 0 ? tr(STR_SUDOKU_EASY) : (level == 1 ? tr(STR_SUDOKU_MEDIUM) : tr(STR_SUDOKU_HARD));
}

const char* markName(const Mark mark) { return mark == tictactoe::X ? "X" : "O"; }

uint16_t readCount(const uint8_t* bytes) {
  const uint16_t count = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
  return count > kTallyMax ? kTallyMax : count;
}

void writeCount(uint8_t* bytes, const uint16_t count) {
  bytes[0] = static_cast<uint8_t>(count & 0xFF);
  bytes[1] = static_cast<uint8_t>(count >> 8);
}

}  // namespace

TicTacToeActivity::TicTacToeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("TicTacToe", renderer, mappedInput) {}

bool TicTacToeActivity::readFile(Settings& settings, Tally& tally, tictactoe::Game* game) {
  uint8_t bytes[kFileBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return false;
  settings.kind = bytes[4] == 1 ? 1 : 0;
  settings.opponent = bytes[5] == TwoPlayers ? TwoPlayers : Computer;
  settings.level = bytes[6] <= 2 ? bytes[6] : 1;
  settings.human = bytes[7] == tictactoe::O ? tictactoe::O : tictactoe::X;
  tally.first = readCount(bytes + 8);
  tally.second = readCount(bytes + 10);
  tally.draws = readCount(bytes + 12);
  return game == nullptr || game->load(bytes + 16, tictactoe::Game::kSaveBytes);
}

void TicTacToeActivity::statusLine(char* out, const size_t size) {
  out[0] = '\0';
  Settings saved;
  Tally rounds;
  if (!readFile(saved, rounds, nullptr) || rounds.first + rounds.second + rounds.draws == 0) return;
  const bool computer = saved.opponent == Computer;
  snprintf(out, size, "%s %u, %s %u", computer ? tr(STR_GAME_YOU) : "X", static_cast<unsigned>(rounds.first),
           computer ? tr(STR_GAME_COMPUTER) : "O", static_cast<unsigned>(rounds.second));
}

void TicTacToeActivity::save() {
  uint8_t bytes[kFileBytes];
  memset(bytes, 0, sizeof(bytes));
  memcpy(bytes, kFileMagic, 4);
  bytes[4] = settings.kind;
  bytes[5] = settings.opponent;
  bytes[6] = settings.level;
  bytes[7] = settings.human;
  writeCount(bytes + 8, tally.first);
  writeCount(bytes + 10, tally.second);
  writeCount(bytes + 12, tally.draws);
  game.save(bytes + 16);
  if (!gameui::writeSave(kSavePath, bytes, sizeof(bytes))) LOG_ERR("TTT", "Could not save the game");
}

void TicTacToeActivity::onEnter() {
  Activity::onEnter();
  pointer.begin(mappedInput);
  if (!readFile(settings, tally, &game)) LOG_DBG("TTT", "No saved game");
  draft = settings;
  panel = game.started() ? Panel::Board : Panel::NewGame;
  lastMoveMs = millis();
  requestUpdate();
}

void TicTacToeActivity::onExit() {
  if (game.started()) save();
  Activity::onExit();
}

void TicTacToeActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int size = kCell * 3;
  topStripY = gameui::contentTop(renderer, mappedInput);
  board = {(width - size) / 2, topStripY + kStripHeight + gameui::kHeaderGap, size, size};
  bottomStripY = board.y + size + gameui::kHeaderGap;
  statusY = bottomStripY + kStripHeight + 4;
  tallyY = gameui::contentBottom(renderer) - kTallyHeight;
  newHold.box = gameui::headerCornerBox(renderer, mappedInput, tr(STR_GAME_NEW_SHORT));

  constexpr int pad = 18;
  constexpr int titleHeight = 40;
  constexpr int rowHeight = 56;
  constexpr int gap = 12;
  panelBox = {board.x + 6, board.y + 28, size - 12, pad + titleHeight + 5 * rowHeight + 4 * gap + pad};
  const int innerX = panelBox.x + pad;
  const int innerWidth = panelBox.w - 2 * pad;
  const int half = (innerWidth - gap) / 2;
  const int third = (innerWidth - 2 * gap) / 3;
  int y = panelBox.y + pad + titleHeight;
  for (int i = 0; i < 2; ++i) kindBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 2; ++i) opponentBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 3; ++i) levelBoxes[i] = {innerX + i * (third + gap), y, third, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 2; ++i) sideBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  cancelBox = {innerX, y, half, rowHeight};
  startBox = {innerX + half + gap, y, half, rowHeight};
}

bool TicTacToeActivity::computerToMove() const {
  return settings.opponent == Computer && game.state() == State::Playing && game.toMove() != settings.human;
}

void TicTacToeActivity::startRound() {
  game.start(static_cast<Kind>(settings.kind));
  panel = Panel::Board;
  lastMoveMs = millis();
  save();
  requestUpdate();
}

void TicTacToeActivity::afterMove() {
  const State state = game.state();
  uint16_t* won = nullptr;
  if (state == State::Draw) {
    won = &tally.draws;
  } else if (state != State::Playing) {
    const Mark winner = state == State::XWins ? tictactoe::X : tictactoe::O;
    const bool first = settings.opponent == Computer ? winner == settings.human : winner == tictactoe::X;
    won = first ? &tally.first : &tally.second;
  }
  if (won != nullptr && *won < kTallyMax) ++*won;
  lastMoveMs = millis();
  save();
  requestUpdate();
}

void TicTacToeActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  if (panel == Panel::Board && computerToMove()) {
    if (millis() - lastMoveMs < kComputerPauseMs) return;
    const int depth = tictactoe::Game::depthForLevel(settings.level, game.kind());
    if (game.play(game.bestMove(depth, esp_random()))) afterMove();
    return;
  }

  bool repaint = false;
  if (panel == Panel::Board && game.started() && newHold.update(mappedInput, repaint, &pointer)) {
    draft = settings;
    panel = Panel::NewGame;
    requestUpdate();
    return;
  }
  if (repaint) requestUpdate();

  int tx = 0;
  int ty = 0;
  bool moved = false;
  bool hit;
  {
    RenderLock lock(*this);
    hit = pointer.tapped(mappedInput, tx, ty, moved);
  }
  if (moved) requestUpdate();
  if (!hit) return;

  if (panel == Panel::NewGame) {
    const bool computer = draft.opponent == Computer;
    for (int i = 0; i < 2; ++i) {
      if (kindBoxes[i].contains(tx, ty)) draft.kind = static_cast<uint8_t>(i);
      if (opponentBoxes[i].contains(tx, ty)) draft.opponent = static_cast<uint8_t>(i);
      if (computer && sideBoxes[i].contains(tx, ty)) draft.human = i == 0 ? tictactoe::X : tictactoe::O;
    }
    for (int i = 0; i < 3; ++i) {
      if (computer && levelBoxes[i].contains(tx, ty)) draft.level = static_cast<uint8_t>(i);
    }
    if (startBox.contains(tx, ty)) {
      // The tally belongs to one kind of game against one opponent.
      const bool sameMatch = draft.kind == settings.kind && draft.opponent == settings.opponent &&
                             (draft.opponent != Computer || draft.level == settings.level);
      if (!sameMatch) tally = Tally{};
      settings = draft;
      startRound();
      return;
    }
    if (game.started() && cancelBox.contains(tx, ty)) panel = Panel::Board;
    requestUpdate();
    return;
  }

  if (!board.contains(tx, ty)) return;
  if (game.state() != State::Playing) {
    if (millis() - lastMoveMs >= kNextRoundGuardMs) startRound();
    return;
  }
  if (game.play(((ty - board.y) / kCell) * 3 + (tx - board.x) / kCell)) afterMove();
}

const char* TicTacToeActivity::statusText() const {
  static char line[48];
  switch (game.state()) {
    case State::NotStarted:
      return "";
    case State::Playing:
      if (settings.opponent == TwoPlayers) {
        snprintf(line, sizeof(line), tr(STR_TTT_TO_MOVE), markName(game.toMove()));
        return line;
      }
      return computerToMove() ? tr(STR_GAME_THINKING) : tr(STR_GAME_YOUR_MOVE);
    case State::XWins:
    case State::OWins: {
      const Mark winner = game.state() == State::XWins ? tictactoe::X : tictactoe::O;
      if (settings.opponent == Computer) return winner == settings.human ? tr(STR_GAME_YOU_WIN) : tr(STR_GAME_YOU_LOSE);
      snprintf(line, sizeof(line), tr(STR_GAME_SIDE_WINS), markName(winner));
      return line;
    }
    case State::Draw:
      break;
  }
  return tr(STR_GAME_DRAW);
}

const char* TicTacToeActivity::noteText() const {
  if (!game.started()) return "";
  if (game.state() != State::Playing) return tr(STR_TTT_PLAY_AGAIN);
  return game.kind() == Kind::Infinite ? tr(STR_TTT_OLDEST_LEAVES) : "";
}

void TicTacToeActivity::drawMark(const gameui::Box& box, const Mark mark) const {
  if (mark == tictactoe::O) {
    gameui::ring(renderer, box.x + box.w / 2, box.y + box.h / 2, box.w / 2 - 34, 11, true);
    return;
  }
  // The renderer thickens a line downwards only, so it starts half its width higher.
  constexpr int pad = 38;
  constexpr int thick = 15;
  const int left = box.x + pad;
  const int right = box.x + box.w - pad;
  const int top = box.y + pad - thick / 2;
  const int bottom = box.y + box.h - pad - thick / 2;
  renderer.drawLine(left, top, right, bottom, thick, true);
  renderer.drawLine(left, bottom, right, top, thick, true);
}

void TicTacToeActivity::drawBoard() const {
  for (int i = 1; i < 3; ++i) {
    renderer.fillRect(board.x + i * kCell - 2, board.y + 6, 5, board.h - 12, true);
    renderer.fillRect(board.x + 6, board.y + i * kCell - 2, board.w - 12, 5, true);
  }
  if (!game.started()) return;

  const auto cellBox = [this](const int cell) {
    return gameui::Box{board.x + (cell % 3) * kCell, board.y + (cell / 3) * kCell, kCell, kCell};
  };
  for (int cell = 0; cell < 9; ++cell) {
    const Mark mark = game.at(cell);
    if (mark == tictactoe::None) continue;
    drawMark(cellBox(cell), mark);
    if (cell == game.fading()) gameui::fade(renderer, cellBox(cell).grown(-8));
  }
  // Brackets on the winning row, or else on the mark just made.
  int cells[3];
  if (game.winningCells(cells)) {
    for (const int cell : cells) gameui::brackets(renderer, cellBox(cell).grown(-8));
  } else if (game.lastCell() >= 0) {
    gameui::brackets(renderer, cellBox(game.lastCell()).grown(-8));
  }
}

void TicTacToeActivity::drawStrip(const int y, const Mark mark) const {
  const char* name = markName(mark);
  const char* detail = "";
  const char* letter = "";
  if (settings.opponent == Computer) {
    const bool human = mark == settings.human;
    name = human ? tr(STR_GAME_YOU) : tr(STR_GAME_COMPUTER);
    detail = human ? "" : levelName(settings.level);
    letter = markName(mark);
  }
  const gameui::Box line{board.x, y, board.w, kStripHeight};
  const int textY = gameui::centredTextY(renderer, UI_10_FONT_ID, line);
  renderer.drawText(UI_10_FONT_ID, board.x, textY, name, true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, board.x + renderer.getTextWidth(UI_10_FONT_ID, name, EpdFontFamily::BOLD) + 10,
                    textY, detail);
  renderer.drawText(UI_10_FONT_ID,
                    board.x + board.w - renderer.getTextWidth(UI_10_FONT_ID, letter, EpdFontFamily::BOLD), textY,
                    letter, true, EpdFontFamily::BOLD);
}

void TicTacToeActivity::drawTally() const {
  const bool computer = settings.opponent == Computer;
  const char* labels[3] = {computer ? tr(STR_GAME_YOU) : "X", tr(STR_TTT_DRAWS),
                           computer ? tr(STR_GAME_COMPUTER) : "O"};
  const uint16_t counts[3] = {tally.first, tally.draws, tally.second};
  // An Infinite round always has a winner.
  const bool withDraws = settings.kind == static_cast<uint8_t>(Kind::Classic);
  const int width = board.w / (withDraws ? 3 : 2);
  renderer.fillRect(board.x, tallyY - 8, board.w, 1, true);
  int column = 0;
  for (int i = 0; i < 3; ++i) {
    if (i == 1 && !withDraws) continue;
    const int x = board.x + column++ * width;
    char count[8];
    snprintf(count, sizeof(count), "%u", static_cast<unsigned>(counts[i]));
    gameui::centredText(renderer, UI_12_FONT_ID, {x, tallyY, width, 36}, count, true, EpdFontFamily::BOLD);
    gameui::centredText(renderer, UI_10_FONT_ID, {x, tallyY + 38, width, 30},
                        renderer.truncatedText(UI_10_FONT_ID, labels[i], width - 8).c_str());
  }
}

void TicTacToeActivity::drawNewGamePanel() const {
  gameui::panelFrame(renderer, panelBox);
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y + 14, panelBox.w, 36}, tr(STR_GAME_NEW), true,
                      EpdFontFamily::BOLD);
  const bool computer = draft.opponent == Computer;
  gameui::choice(renderer, kindBoxes[0], tr(STR_TTT_CLASSIC), draft.kind == 0);
  gameui::choice(renderer, kindBoxes[1], tr(STR_TTT_INFINITE), draft.kind == 1);
  gameui::choice(renderer, opponentBoxes[0], tr(STR_GAME_COMPUTER), computer);
  gameui::choice(renderer, opponentBoxes[1], tr(STR_GAME_TWO_PLAYERS), !computer);
  for (int i = 0; i < 3; ++i) gameui::choice(renderer, levelBoxes[i], levelName(i), draft.level == i, computer);
  gameui::choice(renderer, sideBoxes[0], tr(STR_TTT_X_FIRST), draft.human == tictactoe::X, computer);
  gameui::choice(renderer, sideBoxes[1], "O", draft.human == tictactoe::O, computer);
  if (game.started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
  gameui::button(renderer, startBox, tr(STR_GAME_START), true);
}

void TicTacToeActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_TICTACTOE));
  if (panel == Panel::Board && game.started()) newHold.draw(renderer, tr(STR_GAME_NEW_SHORT), tr(STR_HOLD_FOR_NEW_GAME));

  const Mark bottom = settings.opponent == Computer ? static_cast<Mark>(settings.human) : tictactoe::X;
  drawStrip(topStripY, tictactoe::other(bottom));
  drawBoard();
  drawStrip(bottomStripY, bottom);
  const int width = renderer.getScreenWidth() - 16;
  gameui::fittedText(renderer, {8, statusY, width, 34}, statusText());
  gameui::centredText(renderer, SMALL_FONT_ID, {8, statusY + 34, width, 24},
                      renderer.truncatedText(SMALL_FONT_ID, noteText(), width).c_str());
  drawTally();

  if (panel == Panel::NewGame) drawNewGamePanel();
  pointer.clear();
  if (panel == Panel::NewGame) {
    const bool computer = draft.opponent == Computer;
    for (const auto& b : kindBoxes) pointer.add(b);
    for (const auto& b : opponentBoxes) pointer.add(b);
    if (computer) {
      for (const auto& b : levelBoxes) pointer.add(b);
      for (const auto& b : sideBoxes) pointer.add(b);
    }
    if (game.started()) pointer.add(cancelBox);
    pointer.add(startBox);
  } else {
    for (int cell = 0; cell < 9; ++cell) pointer.add({board.x + (cell % 3) * kCell, board.y + (cell / 3) * kCell, kCell, kCell});
    if (game.started()) pointer.add(newHold.box);
  }
  pointer.draw(renderer);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
