#include "GoActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {

constexpr int kBoardMargin = 10;
constexpr int kStripHeight = 30;
constexpr int kToolHeight = 90;
constexpr int kToolGap = 8;
constexpr uint8_t kFileMagic[4] = {'V', 'G', 'O', '1'};

const char* levelName(const int level) { return level == 0 ? tr(STR_SUDOKU_EASY) : tr(STR_SUDOKU_MEDIUM); }
const char* colourName(const go::Stone stone) { return stone == go::Black ? tr(STR_GO_BLACK) : tr(STR_GO_WHITE); }

// Star points (hoshi) for the three board sizes.
bool isStarPoint(const int size, const int row, const int column) {
  const auto on = [&](const int v) {
    if (size == 9) return v == 2 || v == 6 || v == 4;
    if (size == 13) return v == 3 || v == 9 || v == 6;
    return v == 3 || v == 9 || v == 15;
  };
  if (!on(row) || !on(column)) return false;
  // The 9x9 and 13x13 boards have only the centre besides the four corners.
  const int centre = size / 2;
  if (size != 19 && (row == centre) != (column == centre)) return false;
  return true;
}

}  // namespace

GoActivity::GoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Go", renderer, mappedInput), game(std::make_unique<go::Game>()), job(std::make_unique<Job>()) {}

GoActivity::~GoActivity() { thinker.cancel(); }

void GoActivity::statusLine(char* out, const size_t size) {
  out[0] = '\0';
  std::vector<uint8_t> bytes(kFileBytes);
  if (!gameui::readSave(kSavePath, bytes.data(), bytes.size()) || memcmp(bytes.data(), kFileMagic, 4) != 0) return;
  const auto saved = std::make_unique<go::Game>();
  if (!saved->load(bytes.data() + 8, go::Game::kSaveBytes) || !saved->started()) return;
  if (saved->state() == go::Game::State::Finished) {
    snprintf(out, size, "%s", tr(STR_GAME_OVER));
  } else {
    snprintf(out, size, tr(STR_GO_STATUS), saved->size(), saved->size(), saved->moveCount());
  }
}

bool GoActivity::load() {
  std::vector<uint8_t> bytes(kFileBytes);
  if (!gameui::readSave(kSavePath, bytes.data(), bytes.size()) || memcmp(bytes.data(), kFileMagic, 4) != 0) {
    return false;
  }
  settings.opponent = bytes[4] == TwoPlayers ? TwoPlayers : Computer;
  settings.level = bytes[5] <= 1 ? bytes[5] : 0;
  settings.human = bytes[6] == go::White ? go::White : go::Black;
  settings.size = bytes[7] == 13 || bytes[7] == 19 ? bytes[7] : 9;
  return game->load(bytes.data() + 8, go::Game::kSaveBytes);
}

void GoActivity::save() {
  if (!dirty) return;
  std::vector<uint8_t> bytes(kFileBytes, 0);
  memcpy(bytes.data(), kFileMagic, 4);
  bytes[4] = settings.opponent;
  bytes[5] = settings.level;
  bytes[6] = settings.human;
  bytes[7] = settings.size;
  game->save(bytes.data() + 8);
  if (gameui::writeSave(kSavePath, bytes.data(), bytes.size())) {
    dirty = false;
  } else {
    LOG_ERR("GO", "Could not save the game");
  }
}

void GoActivity::onEnter() {
  Activity::onEnter();
  pointer.begin(mappedInput);
  if (!load()) LOG_DBG("GO", "No saved game");
  draft = settings;
  panel = game->started() ? Panel::Board : Panel::NewGame;
  ghost = -1;
  requestUpdate();
  if (panel == Panel::Board && computerToMove()) startThinking(false);
}

void GoActivity::onExit() {
  thinker.cancel();
  thinking = false;
  save();
  Activity::onExit();
}

int GoActivity::spacing() const { return board.w / std::max<int>(1, game->size()); }
int GoActivity::pointX(const int point) const { return board.x + (point % game->size()) * spacing() + spacing() / 2; }
int GoActivity::pointY(const int point) const { return board.y + (point / game->size()) * spacing() + spacing() / 2; }

int GoActivity::pointAt(const int x, const int y) const {
  if (!board.contains(x, y)) return -1;
  const int sp = spacing();
  const int column = std::min<int>(game->size() - 1, (x - board.x) / sp);
  const int row = std::min<int>(game->size() - 1, (y - board.y) / sp);
  return row * game->size() + column;
}

void GoActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int size = game->started() ? game->size() : settings.size;
  const int sp = (width - 2 * kBoardMargin) / size;
  const int boardPx = sp * size;
  topStripY = gameui::contentTop(renderer, mappedInput);
  board = {(width - boardPx) / 2, topStripY + kStripHeight + gameui::kHeaderGap, boardPx, boardPx};
  bottomStripY = board.y + boardPx + gameui::kHeaderGap;
  statusY = bottomStripY + kStripHeight + 4;
  const int toolsY = std::max(statusY + 34 + gameui::kSectionGap, gameui::contentBottom(renderer) - kToolHeight);
  const int toolWidth = (boardPx - (kToolCount - 1) * kToolGap) / kToolCount;
  for (int i = 0; i < kToolCount; ++i) {
    tools[i] = {board.x + i * (toolWidth + kToolGap), toolsY, toolWidth, kToolHeight};
  }
  newGameBox = {board.x + (boardPx - 260) / 2, toolsY + 14, 260, 64};
  newHold.box = gameui::headerCornerBox(renderer, mappedInput, tr(STR_GAME_NEW_SHORT));

  constexpr int pad = 18;
  constexpr int titleHeight = 40;
  constexpr int rowHeight = 56;
  constexpr int gap = 12;
  panelBox = {board.x + 6, board.y + 20, boardPx - 12, pad + titleHeight + 5 * rowHeight + 4 * gap + pad};
  const int innerX = panelBox.x + pad;
  const int innerWidth = panelBox.w - 2 * pad;
  const int half = (innerWidth - gap) / 2;
  const int third = (innerWidth - 2 * gap) / 3;
  int y = panelBox.y + pad + titleHeight;
  for (int i = 0; i < 2; ++i) opponentBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 3; ++i) sizeBoxes[i] = {innerX + i * (third + gap), y, third, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 2; ++i) levelBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 2; ++i) sideBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  cancelBox = {innerX, y, half, rowHeight};
  startBox = {innerX + half + gap, y, half, rowHeight};
}

bool GoActivity::humanToMove() const {
  return settings.opponent == TwoPlayers || game->toMove() == settings.human;
}

bool GoActivity::computerToMove() const {
  return settings.opponent == Computer && game->state() == go::Game::State::Playing &&
         game->toMove() != settings.human;
}

void GoActivity::startGame() {
  thinker.cancel();
  thinking = false;
  game->start(settings.size);
  panel = Panel::Board;
  ghost = -1;
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void GoActivity::placeGhost() {
  if (ghost < 0 || !game->play(ghost)) return;
  ghost = -1;
  afterMove();
}

void GoActivity::tapPoint(const int point) {
  if (game->state() == go::Game::State::MarkingDead) {
    game->toggleDead(point);
    dirty = true;
    requestUpdate();
    return;
  }
  if (game->state() != go::Game::State::Playing || !humanToMove()) return;
  if (point == ghost) {
    placeGhost();
    return;
  }
  // First tap: the faint stone, only where a stone may go.
  ghost = game->board().legal(point) ? point : -1;
  requestUpdate();
}

// Buttons move the faint stone: Up/Down (the X4 Pro's side buttons) one point
// back or on, row after row; Left/Right a whole row. Confirm (Power) places it.
void GoActivity::moveGhost(const int step) {
  if (game->state() != go::Game::State::Playing || !humanToMove()) return;
  const int points = game->size() * game->size();
  ghost = ghost < 0 ? points / 2 : ((ghost + step) % points + points) % points;
  requestUpdate();
}

void GoActivity::afterMove() {
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void GoActivity::think(void* self) {
  auto* activity = static_cast<GoActivity*>(self);
  Job& work = *activity->job;
  work.move = go::chooseMove(work.board, work.level, work.opponentPassed, work.seed);
}

void GoActivity::startThinking(const bool forHint) {
  if (thinking || game->state() != go::Game::State::Playing) return;
  job->board = game->board();
  job->level = forHint ? 1 : settings.level;
  job->opponentPassed = game->lastWasPass();
  job->seed = esp_random() | 1U;
  job->move = go::kPass;
  hinting = forHint;
  thinking = thinker.start(&GoActivity::think, this, 4000);
  requestUpdate();
}

void GoActivity::finishThinking() {
  thinking = false;
  const bool wasHint = hinting;
  hinting = false;
  if (game->state() != go::Game::State::Playing) {
    requestUpdate();
    return;
  }
  if (wasHint) {
    ghost = job->move;  // a second tap (or Confirm) plays it
    requestUpdate();
    return;
  }
  if (job->move == go::kPass || !game->play(job->move)) {
    game->pass();
  }
  dirty = true;
  save();
  requestUpdate();
}

void GoActivity::undoMove() {
  if (!game->canUndo()) return;
  game->undo();
  // Against the computer, its reply goes too, so the player is to move again.
  if (settings.opponent == Computer && game->state() == go::Game::State::Playing &&
      game->toMove() != settings.human && game->canUndo()) {
    game->undo();
  }
  ghost = -1;
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void GoActivity::passMove() {
  if (game->state() != go::Game::State::Playing || !humanToMove()) return;
  ghost = -1;
  game->pass();
  afterMove();
}

void GoActivity::loop() {
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }

  if (thinking) {
    if (thinker.takeFinished()) {
      finishThinking();
    } else {
      delay(5);
    }
    return;
  }

  bool repaint = false;
  if (panel == Panel::Board && game->started() && newHold.update(mappedInput, repaint, &pointer)) {
    draft = settings;
    panel = Panel::NewGame;
    requestUpdate();
    return;
  }
  if (repaint) requestUpdate();

  // On button readers the pointer below moves over the points instead.
  if (panel == Panel::Board && !pointer.enabled) {
    using B = MappedInputManager::Button;
    if (mappedInput.wasReleased(B::Up)) moveGhost(-1);
    if (mappedInput.wasReleased(B::Down)) moveGhost(1);
    if (mappedInput.wasReleased(B::Left)) moveGhost(-game->size());
    if (mappedInput.wasReleased(B::Right)) moveGhost(game->size());
    if (mappedInput.wasReleased(B::Confirm)) placeGhost();
  }

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
    for (int i = 0; i < 2; ++i) {
      if (opponentBoxes[i].contains(tx, ty)) draft.opponent = static_cast<uint8_t>(i);
      if (draft.opponent == Computer && levelBoxes[i].contains(tx, ty)) draft.level = static_cast<uint8_t>(i);
      if (draft.opponent == Computer && sideBoxes[i].contains(tx, ty)) {
        draft.human = i == 0 ? go::Black : go::White;
      }
    }
    static constexpr uint8_t kSizes[3] = {9, 13, 19};
    for (int i = 0; i < 3; ++i) {
      if (sizeBoxes[i].contains(tx, ty)) draft.size = kSizes[i];
    }
    if (startBox.contains(tx, ty)) {
      settings = draft;
      startGame();
      return;
    }
    if (game->started() && cancelBox.contains(tx, ty)) panel = Panel::Board;
    requestUpdate();
    return;
  }

  if (newHold.box.contains(tx, ty)) return;  // New needs a hold; a tap does nothing
  const int point = pointAt(tx, ty);
  if (point >= 0) {
    tapPoint(point);
    return;
  }
  using State = go::Game::State;
  switch (game->state()) {
    case State::Finished:
      if (newGameBox.contains(tx, ty)) {
        draft = settings;
        panel = Panel::NewGame;
        requestUpdate();
      }
      return;
    case State::MarkingDead:
      if (tools[Undo].contains(tx, ty)) {
        game->resume();  // "Play on"
        dirty = true;
        save();
        requestUpdate();
        if (computerToMove()) startThinking(false);
      } else if (tools[Hint].contains(tx, ty)) {
        game->finish();  // "Count"
        dirty = true;
        save();
        requestUpdate();
      }
      return;
    default:
      break;
  }
  if (tools[Undo].contains(tx, ty)) {
    undoMove();
  } else if (tools[Pass].contains(tx, ty)) {
    passMove();
  } else if (tools[Hint].contains(tx, ty)) {
    if (humanToMove()) startThinking(true);
  }
}

const char* GoActivity::statusText() const {
  static char line[96];
  using State = go::Game::State;
  switch (game->state()) {
    case State::NotStarted:
      return "";
    case State::Playing:
      if ((thinking && !hinting) || !humanToMove()) return tr(STR_GAME_THINKING);
      if (ghost >= 0) return tr(STR_GO_TAP_AGAIN);
      if (game->lastWasPass()) {
        snprintf(line, sizeof(line), tr(STR_GO_PASSED), colourName(go::other(game->toMove())));
        return line;
      }
      if (settings.opponent == TwoPlayers) {
        snprintf(line, sizeof(line), tr(STR_GO_TO_MOVE), colourName(game->toMove()));
        return line;
      }
      return tr(STR_GAME_YOUR_MOVE);
    case State::MarkingDead:
      return tr(STR_GO_MARK_DEAD);
    case State::Finished: {
      const go::Score score = game->result();
      const int margin = score.blackTenths - score.whiteTenths;
      const go::Stone winner = margin > 0 ? go::Black : go::White;
      const int by = margin > 0 ? margin : -margin;
      snprintf(line, sizeof(line), tr(STR_GO_RESULT), score.blackTenths / 10, score.blackTenths % 10,
               score.whiteTenths / 10, score.whiteTenths % 10, colourName(winner), by / 10, by % 10);
      return line;
    }
  }
  return "";
}

void GoActivity::drawStone(const int cx, const int cy, const int radius, const go::Stone stone, const bool faint) const {
  if (faint) {
    // The faint stone: a dotted disc with a thin rim.
    renderer.fillRoundedRect(cx - radius, cy - radius, radius * 2, radius * 2, radius, Color::LightGray);
    gameui::ring(renderer, cx, cy, radius, 1, true);
    return;
  }
  gameui::disc(renderer, cx, cy, radius, stone == go::Black);
  gameui::ring(renderer, cx, cy, radius, stone == go::Black ? 1 : 2, true);
}

void GoActivity::drawBoard() const {
  const int size = game->started() ? game->size() : settings.size;
  const int sp = board.w / size;
  const int first = sp / 2;
  const int last = first + (size - 1) * sp;
  for (int i = 0; i < size; ++i) {
    const int at = first + i * sp;
    const int width = (i == 0 || i == size - 1) ? 2 : 1;
    renderer.fillRect(board.x + first, board.y + at, last - first + 1, width, true);
    renderer.fillRect(board.x + at, board.y + first, width, last - first + 1, true);
  }
  for (int row = 0; row < size; ++row) {
    for (int column = 0; column < size; ++column) {
      if (isStarPoint(size, row, column)) {
        gameui::disc(renderer, board.x + first + column * sp, board.y + first + row * sp, sp >= 30 ? 4 : 3, true);
      }
    }
  }
  if (!game->started()) return;

  const int radius = std::max(6, sp / 2 - 1);
  const bool marking = game->state() == go::Game::State::MarkingDead || game->state() == go::Game::State::Finished;
  for (int p = 0; p < size * size; ++p) {
    const go::Stone stone = game->at(p);
    if (stone == go::Empty) continue;
    const int cx = pointX(p);
    const int cy = pointY(p);
    drawStone(cx, cy, radius, stone, false);
    if (marking && game->isDead(p)) {
      // A dead stone gets a cross.
      const int k = radius / 2;
      const bool ink = stone != go::Black;
      renderer.drawLine(cx - k, cy - k, cx + k, cy + k, 2, ink);
      renderer.drawLine(cx - k, cy + k, cx + k, cy - k, 2, ink);
    }
  }
  const int lastStone = game->lastMove();
  if (lastStone >= 0 && game->at(lastStone) != go::Empty) {
    // The last stone played: a small mark of the other colour.
    gameui::ring(renderer, pointX(lastStone), pointY(lastStone), std::max(3, radius / 3), 2, game->at(lastStone) != go::Black);
  }
  if (ghost >= 0 && game->at(ghost) == go::Empty) drawStone(pointX(ghost), pointY(ghost), radius, game->toMove(), true);
  renderer.drawRect(board.x - 2, board.y - 2, board.w + 4, board.h + 4, 1, true);
}

void GoActivity::drawStrip(const int y, const go::Stone side) const {
  const char* name = colourName(side);
  const char* detail = "";
  if (settings.opponent == Computer) {
    const bool human = side == settings.human;
    name = human ? tr(STR_GAME_YOU) : tr(STR_GAME_COMPUTER);
    detail = human ? colourName(side) : levelName(settings.level);
  }
  const gameui::Box line{board.x, y, board.w, kStripHeight};
  const int textY = gameui::centredTextY(renderer, UI_10_FONT_ID, line);
  renderer.drawText(UI_10_FONT_ID, board.x, textY, name, true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, board.x + renderer.getTextWidth(UI_10_FONT_ID, name, EpdFontFamily::BOLD) + 10,
                    textY, detail);
  if (!game->started()) return;
  char taken[32];
  snprintf(taken, sizeof(taken), tr(STR_GO_CAPTURES), game->board().captured[side]);
  renderer.drawText(UI_10_FONT_ID, board.x + board.w - renderer.getTextWidth(UI_10_FONT_ID, taken), textY, taken);
}

void GoActivity::drawNewGamePanel() const {
  gameui::panelFrame(renderer, panelBox);
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y + 14, panelBox.w, 36}, tr(STR_GAME_NEW), true,
                      EpdFontFamily::BOLD);
  const bool computer = draft.opponent == Computer;
  gameui::choice(renderer, opponentBoxes[0], tr(STR_GAME_COMPUTER), computer);
  gameui::choice(renderer, opponentBoxes[1], tr(STR_GAME_TWO_PLAYERS), !computer);
  static constexpr const char* kSizeNames[3] = {"9 x 9", "13 x 13", "19 x 19"};
  static constexpr uint8_t kSizes[3] = {9, 13, 19};
  for (int i = 0; i < 3; ++i) gameui::choice(renderer, sizeBoxes[i], kSizeNames[i], draft.size == kSizes[i]);
  for (int i = 0; i < 2; ++i) gameui::choice(renderer, levelBoxes[i], levelName(i), draft.level == i, computer);
  gameui::choice(renderer, sideBoxes[0], tr(STR_GO_BLACK_FIRST), draft.human == go::Black, computer);
  gameui::choice(renderer, sideBoxes[1], tr(STR_GO_WHITE), draft.human == go::White, computer);
  if (game->started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
  gameui::button(renderer, startBox, tr(STR_GAME_START), true);
}

void GoActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_GO));
  if (panel == Panel::Board && game->started()) newHold.draw(renderer, tr(STR_GAME_NEW_SHORT), tr(STR_HOLD_FOR_NEW_GAME));

  const go::Stone top = settings.opponent == Computer ? go::other(static_cast<go::Stone>(settings.human)) : go::White;
  drawStrip(topStripY, top);
  drawBoard();
  drawStrip(bottomStripY, go::other(top));
  gameui::fittedText(renderer, {8, statusY, renderer.getScreenWidth() - 16, 34}, statusText());

  using State = go::Game::State;
  if (game->state() == State::Finished) {
    gameui::button(renderer, newGameBox, tr(STR_GAME_NEW), true);
  } else if (game->state() == State::MarkingDead) {
    gameui::button(renderer, tools[Undo], tr(STR_GO_PLAY_ON), false);
    gameui::button(renderer, tools[Hint], tr(STR_GO_COUNT), true);
  } else {
    gameui::iconButton(renderer, tools[Undo], appart::kIconUndo, tr(STR_SUDOKU_UNDO), false);
    gameui::button(renderer, tools[Pass], tr(STR_GO_PASS), false);
    gameui::iconButton(renderer, tools[Hint], appart::kIconHint, tr(STR_SUDOKU_HINT), false);
  }

  if (panel == Panel::NewGame) drawNewGamePanel();
  pointer.clear();
  if (panel == Panel::NewGame) {
    for (const auto& b : opponentBoxes) pointer.add(b);
    if (draft.opponent == Computer) {
      for (const auto& b : levelBoxes) pointer.add(b);
      for (const auto& b : sideBoxes) pointer.add(b);
    }
    for (const auto& b : sizeBoxes) pointer.add(b);
    if (game->started()) pointer.add(cancelBox);
    pointer.add(startBox);
  } else {
    const int sp = spacing();
    for (int point = 0; point < game->size() * game->size(); ++point) {
      pointer.add({board.x + (point % game->size()) * sp, board.y + (point / game->size()) * sp, sp, sp});
    }
    if (game->state() == State::Finished) {
      pointer.add(newGameBox);
    } else if (game->state() == State::MarkingDead) {
      pointer.add(tools[Undo]);
      pointer.add(tools[Hint]);
    } else {
      for (const auto& b : tools) pointer.add(b);
    }
    if (game->started()) pointer.add(newHold.box);
  }
  pointer.draw(renderer);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
