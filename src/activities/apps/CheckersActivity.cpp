#include "CheckersActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {

constexpr int kSquare = 58;
constexpr int kStripHeight = 30;
constexpr int kToolHeight = 100;
constexpr int kToolGap = 8;
constexpr uint8_t kFileMagic[4] = {'V', 'K', 'A', '1'};

const char* levelName(const int level) {
  return level == 0 ? tr(STR_SUDOKU_EASY) : (level == 1 ? tr(STR_SUDOKU_MEDIUM) : tr(STR_SUDOKU_HARD));
}

const char* sideName(const checkers::Side side) {
  return side == checkers::Dark ? tr(STR_CHECKERS_DARK) : tr(STR_CHECKERS_LIGHT);
}

checkers::Side otherSide(const checkers::Side side) { return side == checkers::Dark ? checkers::Light : checkers::Dark; }

}  // namespace

CheckersActivity::CheckersActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Checkers", renderer, mappedInput),
      game(std::make_unique<checkers::Game>()),
      job(std::make_unique<Job>()) {}

CheckersActivity::~CheckersActivity() { thinker.cancel(); }

void CheckersActivity::statusLine(char* out, const size_t size) {
  out[0] = '\0';
  uint8_t bytes[kFileBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return;
  // A game object is several kilobytes: not for the stack of the main task.
  const auto saved = std::make_unique<checkers::Game>();
  if (!saved->load(bytes + 8, checkers::Game::kSaveBytes) || !saved->started()) return;
  if (saved->state() == checkers::Game::State::Playing) {
    snprintf(out, size, tr(STR_CHECKERS_SCORE), saved->position().count(checkers::Dark),
             saved->position().count(checkers::Light));
  } else {
    snprintf(out, size, "%s", tr(STR_GAME_OVER));
  }
}

bool CheckersActivity::load() {
  uint8_t bytes[kFileBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return false;
  settings.opponent = bytes[4] == TwoPlayers ? TwoPlayers : Computer;
  settings.level = bytes[5] <= 2 ? bytes[5] : 1;
  settings.human = bytes[6] == checkers::Light ? checkers::Light : checkers::Dark;
  return game->load(bytes + 8, checkers::Game::kSaveBytes);
}

void CheckersActivity::save() {
  if (!dirty) return;
  uint8_t bytes[kFileBytes];
  memset(bytes, 0, sizeof(bytes));
  memcpy(bytes, kFileMagic, 4);
  bytes[4] = settings.opponent;
  bytes[5] = settings.level;
  bytes[6] = settings.human;
  game->save(bytes + 8);
  if (gameui::writeSave(kSavePath, bytes, sizeof(bytes))) {
    dirty = false;
  } else {
    LOG_ERR("CHECKERS", "Could not save the game");
  }
}

void CheckersActivity::onEnter() {
  Activity::onEnter();
  if (!load()) LOG_DBG("CHECKERS", "No saved game");
  draft = settings;
  panel = game->started() ? Panel::Board : Panel::NewGame;
  selected = -1;
  targetCount = 0;
  hintFrom = hintTo = -1;
  // A jump left unfinished keeps its piece selected.
  if (game->state() == checkers::Game::State::Playing && humanToMove() &&
      game->position().jumper != checkers::kNone) {
    select(game->position().jumper);
  }
  requestUpdate();
  if (panel == Panel::Board && computerToMove()) startThinking(false);
}

void CheckersActivity::onExit() {
  thinker.cancel();
  thinking = false;
  save();
  Activity::onExit();
}

void CheckersActivity::layout() {
  const int width = renderer.getScreenWidth();
  const int size = kSquare * 8;
  topStripY = gameui::contentTop(renderer, mappedInput);
  board = {(width - size) / 2, topStripY + kStripHeight + gameui::kHeaderGap, size, size};
  bottomStripY = board.y + size + gameui::kHeaderGap;
  statusY = bottomStripY + kStripHeight + 4;
  const int toolsY = std::max(statusY + 34 + gameui::kSectionGap, gameui::contentBottom(renderer) - kToolHeight);
  const int toolWidth = (size - (kToolCount - 1) * kToolGap) / kToolCount;
  for (int i = 0; i < kToolCount; ++i) {
    tools[i] = {board.x + i * (toolWidth + kToolGap), toolsY, toolWidth, kToolHeight};
  }
  newGameBox = {board.x + (size - 260) / 2, toolsY + 18, 260, 64};
  newHold.box = gameui::headerCornerBox(renderer, mappedInput, tr(STR_GAME_NEW_SHORT));

  constexpr int pad = 18;
  constexpr int titleHeight = 40;
  constexpr int rowHeight = 56;
  constexpr int gap = 12;
  panelBox = {board.x + 6, board.y + 40, size - 12, pad + titleHeight + 4 * rowHeight + 3 * gap + pad};
  const int innerX = panelBox.x + pad;
  const int innerWidth = panelBox.w - 2 * pad;
  const int half = (innerWidth - gap) / 2;
  const int third = (innerWidth - 2 * gap) / 3;
  int y = panelBox.y + pad + titleHeight;
  for (int i = 0; i < 2; ++i) opponentBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 3; ++i) levelBoxes[i] = {innerX + i * (third + gap), y, third, rowHeight};
  y += rowHeight + gap;
  for (int i = 0; i < 2; ++i) sideBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  cancelBox = {innerX, y, half, rowHeight};
  startBox = {innerX + half + gap, y, half, rowHeight};
}

// Light starts at the bottom of the stored board; a player with the dark
// pieces sees it turned round.
bool CheckersActivity::flipped() const { return settings.opponent == Computer && settings.human == checkers::Dark; }

bool CheckersActivity::humanToMove() const {
  return settings.opponent == TwoPlayers || game->sideToMove() == settings.human;
}

bool CheckersActivity::computerToMove() const {
  return settings.opponent == Computer && game->state() == checkers::Game::State::Playing &&
         game->sideToMove() != settings.human;
}

void CheckersActivity::startGame() {
  thinker.cancel();
  thinking = false;
  game->start();
  panel = Panel::Board;
  selected = -1;
  targetCount = 0;
  hintFrom = hintTo = -1;
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void CheckersActivity::select(const int square) {
  selected = square;
  targetCount = game->targets(square, targets);
  if (targetCount == 0) selected = -1;
}

void CheckersActivity::tapSquare(const int square) {
  if (game->state() != checkers::Game::State::Playing || !humanToMove()) return;
  hintFrom = hintTo = -1;
  if (selected >= 0) {
    for (int i = 0; i < targetCount; ++i) {
      if (targets[i] != square) continue;
      game->play(selected, square);
      afterStep();
      return;
    }
  }
  // In the middle of a jump only that piece may go on.
  if (game->position().jumper != checkers::kNone) {
    select(game->position().jumper);
  } else {
    const uint8_t piece = game->at(square);
    if (piece != checkers::Empty && checkers::sideOf(piece) == game->sideToMove() && square != selected) {
      select(square);
    } else {
      selected = -1;
      targetCount = 0;
    }
  }
  requestUpdate();
}

void CheckersActivity::afterStep() {
  dirty = true;
  save();
  selected = -1;
  targetCount = 0;
  if (game->state() == checkers::Game::State::Playing && humanToMove() &&
      game->position().jumper != checkers::kNone) {
    select(game->position().jumper);
  }
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void CheckersActivity::think(void* self) {
  auto* activity = static_cast<CheckersActivity*>(self);
  Job& work = *activity->job;
  work.result = checkers::search(work.root, work.options, &GameThinker::stopCallback, &activity->thinker);
}

void CheckersActivity::startThinking(const bool forHint) {
  if (thinking || game->state() != checkers::Game::State::Playing) return;
  job->root = game->position();
  job->result = checkers::SearchResult{};

  // The first moves get some variety, or every game would open the same way.
  const bool opening = game->position().count(checkers::Dark) + game->position().count(checkers::Light) >= 23;
  const int level = forHint ? 1 : settings.level;
  uint32_t timeLimitMs = 2000;
  checkers::SearchOptions& options = job->options;
  options = checkers::SearchOptions{};
  if (level == 0) {
    options.maxDepth = 2;
    options.margin = 30;
    timeLimitMs = 1200;
  } else if (level == 1) {
    options.maxDepth = 6;
    options.margin = opening ? 6 : 0;
  } else {
    options.maxDepth = 14;
    options.margin = opening ? 4 : 0;
    timeLimitMs = 3500;
  }
  if (forHint) options.margin = 0;
  options.seed = esp_random() | 1U;

  hinting = forHint;
  thinking = thinker.start(&CheckersActivity::think, this, timeLimitMs);
  requestUpdate();
}

void CheckersActivity::finishThinking() {
  thinking = false;
  const checkers::SearchResult& result = job->result;
  LOG_DBG("CHECKERS", "Search depth %d, %lu positions, score %d", result.depth,
          static_cast<unsigned long>(result.nodes), result.score);
  const bool wasHint = hinting;
  hinting = false;
  if (!result.found || game->state() != checkers::Game::State::Playing) {
    requestUpdate();
    return;
  }
  if (wasHint) {
    hintFrom = result.best.from;
    hintTo = result.best.to;
    requestUpdate();
    return;
  }
  // The computer's own jump may go on: afterStep() starts the next search.
  if (game->play(result.best)) afterStep();
}

void CheckersActivity::undoMove() {
  bool undone = false;
  if (settings.opponent == Computer) {
    undone = game->undoTo(static_cast<checkers::Side>(settings.human));
  } else {
    // The move in progress, or else the last one made.
    const checkers::Side mover = game->position().jumper != checkers::kNone ? game->sideToMove()
                                                                            : otherSide(game->sideToMove());
    undone = game->undoTo(mover);
  }
  if (!undone) return;
  selected = -1;
  targetCount = 0;
  hintFrom = hintTo = -1;
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

gameui::Box CheckersActivity::squareBox(const int square) const {
  const int row = flipped() ? 7 - (square >> 3) : square >> 3;
  const int column = flipped() ? 7 - (square & 7) : square & 7;
  return {board.x + column * kSquare, board.y + row * kSquare, kSquare, kSquare};
}

int CheckersActivity::squareAt(const int x, const int y) const {
  if (!board.contains(x, y)) return -1;
  const int column = (x - board.x) / kSquare;
  const int row = (y - board.y) / kSquare;
  return flipped() ? (7 - row) * 8 + (7 - column) : row * 8 + column;
}

void CheckersActivity::loop() {
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
  if (panel == Panel::Board && game->started() && newHold.update(mappedInput, repaint)) {
    draft = settings;
    panel = Panel::NewGame;
    requestUpdate();
    return;
  }
  if (repaint) requestUpdate();

  int tx = 0;
  int ty = 0;
  if (!mappedInput.wasScreenTapped(tx, ty)) return;

  if (panel == Panel::NewGame) {
    for (int i = 0; i < 2; ++i) {
      if (opponentBoxes[i].contains(tx, ty)) draft.opponent = static_cast<uint8_t>(i);
      if (draft.opponent == Computer && sideBoxes[i].contains(tx, ty)) draft.human = static_cast<uint8_t>(i);
    }
    for (int i = 0; i < 3; ++i) {
      if (draft.opponent == Computer && levelBoxes[i].contains(tx, ty)) draft.level = static_cast<uint8_t>(i);
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
  const int square = squareAt(tx, ty);
  if (square >= 0) {
    tapSquare(square);
    return;
  }
  if (game->state() != checkers::Game::State::Playing) {
    if (newGameBox.contains(tx, ty)) {
      draft = settings;
      panel = Panel::NewGame;
      requestUpdate();
    }
    return;
  }
  if (tools[Undo].contains(tx, ty)) {
    undoMove();
  } else if (tools[Hint].contains(tx, ty)) {
    if (humanToMove()) startThinking(true);
  }
}

const char* CheckersActivity::statusText() const {
  static char line[72];
  using State = checkers::Game::State;
  switch (game->state()) {
    case State::NotStarted:
      return "";
    case State::Playing:
      if (settings.opponent == TwoPlayers) {
        return game->sideToMove() == checkers::Dark ? tr(STR_CHECKERS_DARK_TO_MOVE) : tr(STR_CHECKERS_LIGHT_TO_MOVE);
      }
      if ((thinking && !hinting) || !humanToMove()) return tr(STR_GAME_THINKING);
      if (game->position().jumper != checkers::kNone) return tr(STR_CHECKERS_JUMP_AGAIN);
      return game->mustCapture() ? tr(STR_CHECKERS_MUST_CAPTURE) : tr(STR_GAME_YOUR_MOVE);
    case State::DarkWins:
    case State::LightWins: {
      const checkers::Side winner = game->state() == State::DarkWins ? checkers::Dark : checkers::Light;
      if (settings.opponent == Computer) return winner == settings.human ? tr(STR_GAME_YOU_WIN) : tr(STR_GAME_YOU_LOSE);
      snprintf(line, sizeof(line), tr(STR_GAME_SIDE_WINS), sideName(winner));
      return line;
    }
    case State::Draw:
      break;
  }
  return tr(STR_GAME_DRAW);
}

void CheckersActivity::drawPiece(const gameui::Box& box, const uint8_t content) const {
  const int cx = box.x + box.w / 2;
  const int cy = box.y + box.h / 2;
  constexpr int radius = 23;
  const bool light = checkers::isLight(content);
  // A white rim sets the piece off from the hatching.
  gameui::disc(renderer, cx, cy, radius + 2, false);
  if (light) {
    gameui::ring(renderer, cx, cy, radius, 3, true);
    gameui::ring(renderer, cx, cy, radius - 7, 1, true);
  } else {
    gameui::disc(renderer, cx, cy, radius, true);
    gameui::ring(renderer, cx, cy, radius - 6, 2, false);
  }
  if (checkers::isKing(content)) {
    const appart::Bitmap& crown = appart::kSmallPieceBlackQueen;
    if (!light) gameui::disc(renderer, cx, cy, radius - 8, true);
    gameui::stamp(renderer, crown, cx - crown.width / 2, cy - crown.height / 2, light);
  }
}

void CheckersActivity::drawBoard() const {
  for (int square = 0; square < 64; ++square) {
    if (checkers::playable(square)) gameui::hatch(renderer, squareBox(square));
  }
  renderer.drawRect(board.x - 2, board.y - 2, board.w + 4, board.h + 4, 2, true);
  if (!game->started()) return;

  const checkers::Step last = game->lastStep();
  if (last.from != last.to) {
    gameui::brackets(renderer, squareBox(last.from));
    gameui::brackets(renderer, squareBox(last.to));
  }
  for (int square = 0; square < 64; ++square) {
    const uint8_t content = game->at(square);
    if (content != checkers::Empty) drawPiece(squareBox(square), content);
  }
  if (selected >= 0) {
    const gameui::Box box = squareBox(selected);
    renderer.drawRect(box.x, box.y, box.w, box.h, 4, true);
  }
  for (int i = 0; i < targetCount; ++i) {
    const gameui::Box box = squareBox(targets[i]);
    gameui::disc(renderer, box.x + box.w / 2, box.y + box.h / 2, 10, false);
    gameui::disc(renderer, box.x + box.w / 2, box.y + box.h / 2, 7, true);
  }
  for (const int square : {hintFrom, hintTo}) {
    if (square < 0) continue;
    const gameui::Box box = squareBox(square);
    renderer.drawRect(box.x + 5, box.y + 5, box.w - 10, box.h - 10, 3, true);
  }
}

void CheckersActivity::drawStrip(const int y, const checkers::Side side) const {
  const char* name = sideName(side);
  const char* detail = "";
  if (settings.opponent == Computer) {
    const bool human = side == settings.human;
    name = human ? tr(STR_GAME_YOU) : tr(STR_GAME_COMPUTER);
    detail = human ? sideName(side) : levelName(settings.level);
  }
  const gameui::Box line{board.x, y, board.w, kStripHeight};
  const int textY = gameui::centredTextY(renderer, UI_10_FONT_ID, line);
  renderer.drawText(UI_10_FONT_ID, board.x, textY, name, true, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, board.x + renderer.getTextWidth(UI_10_FONT_ID, name, EpdFontFamily::BOLD) + 10,
                    textY, detail);
  if (!game->started()) return;
  char pieces[32];
  snprintf(pieces, sizeof(pieces), tr(STR_CHECKERS_PIECES), game->position().count(side));
  renderer.drawText(UI_10_FONT_ID, board.x + board.w - renderer.getTextWidth(UI_10_FONT_ID, pieces), textY, pieces);
}

void CheckersActivity::drawNewGamePanel() const {
  gameui::panelFrame(renderer, panelBox);
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y + 14, panelBox.w, 36}, tr(STR_GAME_NEW), true,
                      EpdFontFamily::BOLD);
  const bool computer = draft.opponent == Computer;
  gameui::choice(renderer, opponentBoxes[0], tr(STR_GAME_COMPUTER), computer);
  gameui::choice(renderer, opponentBoxes[1], tr(STR_GAME_TWO_PLAYERS), !computer);
  for (int i = 0; i < 3; ++i) gameui::choice(renderer, levelBoxes[i], levelName(i), draft.level == i, computer);
  gameui::choice(renderer, sideBoxes[0], tr(STR_CHECKERS_DARK_FIRST), draft.human == checkers::Dark, computer);
  gameui::choice(renderer, sideBoxes[1], tr(STR_CHECKERS_LIGHT), draft.human == checkers::Light, computer);
  if (game->started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
  gameui::button(renderer, startBox, tr(STR_GAME_START), true);
}

void CheckersActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_CHECKERS));
  if (panel == Panel::Board && game->started()) newHold.draw(renderer, tr(STR_GAME_NEW_SHORT));

  const checkers::Side bottom = flipped() ? checkers::Dark : checkers::Light;
  drawStrip(topStripY, otherSide(bottom));
  drawBoard();
  drawStrip(bottomStripY, bottom);
  gameui::fittedText(renderer, {8, statusY, renderer.getScreenWidth() - 16, 34}, statusText());

  if (game->started() && game->state() != checkers::Game::State::Playing) {
    gameui::button(renderer, newGameBox, tr(STR_GAME_NEW), true);
  } else {
    gameui::iconButton(renderer, tools[Undo], appart::kIconUndo, tr(STR_SUDOKU_UNDO), false);
    gameui::iconButton(renderer, tools[Hint], appart::kIconHint, tr(STR_SUDOKU_HINT), false);
  }

  if (panel == Panel::NewGame) drawNewGamePanel();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
