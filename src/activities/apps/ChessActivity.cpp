#include "ChessActivity.h"

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
constexpr int kPieceInset = 1;  // the piece bitmaps are 56 pixels
constexpr int kStripHeight = 30;
constexpr int kToolHeight = 100;
constexpr int kToolGap = 8;
constexpr int kSmallStep = 20;
constexpr uint8_t kFileMagic[4] = {'V', 'C', 'A', '1'};

// The artwork lists the kinds as king, queen, rook, bishop, knight, pawn.
int artIndex(const chess::Piece piece) { return 6 - piece; }

const char* levelName(const int level) {
  return level == 0 ? tr(STR_SUDOKU_EASY) : (level == 1 ? tr(STR_SUDOKU_MEDIUM) : tr(STR_SUDOKU_HARD));
}

const char* colourName(const chess::Colour colour) {
  return colour == chess::White ? tr(STR_CHESS_WHITE) : tr(STR_CHESS_BLACK);
}

}  // namespace

ChessActivity::ChessActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Chess", renderer, mappedInput), game(std::make_unique<chess::Game>()), job(std::make_unique<Job>()) {}

ChessActivity::~ChessActivity() { thinker.cancel(); }

void ChessActivity::statusLine(char* out, const size_t size) {
  out[0] = '\0';
  uint8_t bytes[kFileBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return;
  // A game object is several kilobytes: not for the stack of the main task.
  const auto saved = std::make_unique<chess::Game>();
  if (!saved->load(bytes + 8, chess::Game::kSaveBytes) || !saved->started()) return;
  if (saved->state() == chess::Game::State::Playing) {
    snprintf(out, size, tr(STR_CHESS_MOVE_N), saved->plies() / 2 + 1);
  } else {
    snprintf(out, size, "%s", tr(STR_GAME_OVER));
  }
}

bool ChessActivity::load() {
  uint8_t bytes[kFileBytes];
  if (!gameui::readSave(kSavePath, bytes, sizeof(bytes)) || memcmp(bytes, kFileMagic, 4) != 0) return false;
  settings.opponent = bytes[4] == TwoPlayers ? TwoPlayers : Computer;
  settings.level = bytes[5] <= 2 ? bytes[5] : 1;
  settings.human = bytes[6] == chess::Black ? chess::Black : chess::White;
  settings.flipped = bytes[7] ? 1 : 0;
  return game->load(bytes + 8, chess::Game::kSaveBytes);
}

void ChessActivity::save() {
  if (!dirty) return;
  uint8_t bytes[kFileBytes];
  memcpy(bytes, kFileMagic, 4);
  bytes[4] = settings.opponent;
  bytes[5] = settings.level;
  bytes[6] = settings.human;
  bytes[7] = settings.flipped;
  game->save(bytes + 8);
  if (gameui::writeSave(kSavePath, bytes, sizeof(bytes))) {
    dirty = false;
  } else {
    LOG_ERR("CHESS", "Could not save the game");
  }
}

void ChessActivity::onEnter() {
  Activity::onEnter();
  if (!load()) LOG_DBG("CHESS", "No saved game");
  draft = settings;
  panel = game->started() ? Panel::Board : Panel::NewGame;
  selected = -1;
  targetCount = 0;
  hintFrom = hintTo = -1;
  requestUpdate();
  if (panel == Panel::Board && computerToMove()) startThinking(false);
}

void ChessActivity::onExit() {
  thinker.cancel();
  thinking = false;
  save();
  Activity::onExit();
}

void ChessActivity::layout() {
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

  // The New game panel lies over the middle of the board.
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
  for (int i = 0; i < 2; ++i) colourBoxes[i] = {innerX + i * (half + gap), y, half, rowHeight};
  y += rowHeight + gap;
  cancelBox = {innerX, y, half, rowHeight};
  startBox = {innerX + half + gap, y, half, rowHeight};

  constexpr int choice = 84;
  const int choicesX = board.x + (size - (4 * choice + 3 * 10)) / 2;
  for (int i = 0; i < 4; ++i) {
    promotionBoxes[i] = {choicesX + i * (choice + 10), board.y + (size - choice) / 2, choice, choice};
  }
}

bool ChessActivity::humanToMove() const {
  return settings.opponent == TwoPlayers || game->sideToMove() == settings.human;
}

bool ChessActivity::computerToMove() const {
  return settings.opponent == Computer && game->state() == chess::Game::State::Playing &&
         game->sideToMove() != settings.human;
}

void ChessActivity::startGame() {
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

void ChessActivity::select(const int square) {
  selected = square;
  targetCount = game->targets(square, targets);
}

void ChessActivity::tapSquare(const int square) {
  if (game->state() != chess::Game::State::Playing || !humanToMove()) return;
  hintFrom = hintTo = -1;
  if (selected >= 0) {
    for (int i = 0; i < targetCount; ++i) {
      if (targets[i] != square) continue;
      if (game->isPromotion(selected, square)) {
        promotionFrom = selected;
        promotionTo = square;
        panel = Panel::Promotion;
        requestUpdate();
        return;
      }
      game->play(selected, square);
      selected = -1;
      targetCount = 0;
      afterMove();
      return;
    }
  }
  const uint8_t piece = game->at(square);
  if (piece && chess::colourOf(piece) == game->sideToMove() && square != selected) {
    select(square);
  } else {
    selected = -1;
    targetCount = 0;
  }
  requestUpdate();
}

void ChessActivity::afterMove() {
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

void ChessActivity::think(void* self) {
  auto* activity = static_cast<ChessActivity*>(self);
  Job& work = *activity->job;
  work.result = chess::search(work.root, work.options, work.hashes, work.hashCount, &GameThinker::stopCallback,
                              &activity->thinker);
}

void ChessActivity::startThinking(const bool forHint) {
  if (thinking || game->state() != chess::Game::State::Playing) return;
  job->root = game->position();
  job->hashCount = game->hashCount();
  memcpy(job->hashes, game->hashes(), sizeof(uint32_t) * static_cast<size_t>(job->hashCount));
  job->result = chess::SearchResult{};

  // The first moves get some variety, or every game would open the same way.
  const bool opening = game->plies() < 10;
  const int level = forHint ? 1 : settings.level;
  uint32_t timeLimitMs = 2500;
  chess::SearchOptions& options = job->options;
  options = chess::SearchOptions{};
  if (level == 0) {
    options.maxDepth = 2;
    options.margin = 60;
    timeLimitMs = 1500;
  } else if (level == 1) {
    options.maxDepth = 4;
    options.margin = opening ? 15 : 0;
  } else {
    options.maxDepth = 8;
    options.margin = opening ? 10 : 0;
    timeLimitMs = 5000;
  }
  if (forHint) options.margin = 0;
  options.seed = esp_random() | 1U;

  hinting = forHint;
  thinking = thinker.start(&ChessActivity::think, this, timeLimitMs);
  requestUpdate();
}

void ChessActivity::finishThinking() {
  thinking = false;
  const chess::SearchResult& result = job->result;
  LOG_DBG("CHESS", "Search depth %d, %lu positions, score %d", result.depth, static_cast<unsigned long>(result.nodes),
          result.score);
  if (result.found && game->state() == chess::Game::State::Playing) {
    if (hinting) {
      hintFrom = result.best.fromSquare();
      hintTo = result.best.toSquare();
    } else if (game->play(result.best)) {
      dirty = true;
      save();
    }
  }
  hinting = false;
  requestUpdate();
}

void ChessActivity::undoMove() {
  if (!game->undo()) return;
  // Against the computer, go back to the player's own turn.
  if (settings.opponent == Computer && game->plies() > 0 && game->sideToMove() != settings.human) game->undo();
  selected = -1;
  targetCount = 0;
  hintFrom = hintTo = -1;
  dirty = true;
  save();
  requestUpdate();
  if (computerToMove()) startThinking(false);
}

gameui::Box ChessActivity::squareBox(const int square) const {
  const int rank = square >> 3;
  const int file = square & 7;
  const int column = settings.flipped ? 7 - file : file;
  const int row = settings.flipped ? rank : 7 - rank;
  return {board.x + column * kSquare, board.y + row * kSquare, kSquare, kSquare};
}

int ChessActivity::squareAt(const int x, const int y) const {
  if (!board.contains(x, y)) return -1;
  const int column = (x - board.x) / kSquare;
  const int row = (y - board.y) / kSquare;
  const int file = settings.flipped ? 7 - column : column;
  const int rank = settings.flipped ? row : 7 - row;
  return rank * 8 + file;
}

void ChessActivity::loop() {
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
    selected = -1;
    targetCount = 0;
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
      if (draft.opponent == Computer && colourBoxes[i].contains(tx, ty)) draft.human = static_cast<uint8_t>(i);
    }
    for (int i = 0; i < 3; ++i) {
      if (draft.opponent == Computer && levelBoxes[i].contains(tx, ty)) draft.level = static_cast<uint8_t>(i);
    }
    if (startBox.contains(tx, ty)) {
      settings = draft;
      settings.flipped = (settings.opponent == Computer && settings.human == chess::Black) ? 1 : 0;
      startGame();
      return;
    }
    if (game->started() && cancelBox.contains(tx, ty)) panel = Panel::Board;
    requestUpdate();
    return;
  }

  if (panel == Panel::Promotion) {
    constexpr chess::Piece kChoices[4] = {chess::Queen, chess::Rook, chess::Bishop, chess::Knight};
    panel = Panel::Board;
    for (int i = 0; i < 4; ++i) {
      if (!promotionBoxes[i].contains(tx, ty)) continue;
      game->play(promotionFrom, promotionTo, kChoices[i]);
      selected = -1;
      targetCount = 0;
      afterMove();
      return;
    }
    // A tap elsewhere takes the pawn move back to being chosen.
    requestUpdate();
    return;
  }

  if (newHold.box.contains(tx, ty)) return;  // New needs a hold; a tap does nothing
  const int square = squareAt(tx, ty);
  if (square >= 0) {
    tapSquare(square);
    return;
  }
  if (game->state() != chess::Game::State::Playing) {
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
  } else if (tools[Flip].contains(tx, ty)) {
    settings.flipped ^= 1;
    dirty = true;
    requestUpdate();
  }
}

const char* ChessActivity::statusText() const {
  static char line[72];
  using State = chess::Game::State;
  const bool white = game->sideToMove() == chess::White;
  switch (game->state()) {
    case State::NotStarted:
      return "";
    case State::Playing: {
      const char* turn = settings.opponent == TwoPlayers
                             ? (white ? tr(STR_CHESS_WHITE_TO_MOVE) : tr(STR_CHESS_BLACK_TO_MOVE))
                             : ((thinking && !hinting) || !humanToMove() ? tr(STR_GAME_THINKING)
                                                                         : tr(STR_GAME_YOUR_MOVE));
      if (!game->check()) return turn;
      snprintf(line, sizeof(line), "%s - %s", tr(STR_CHESS_CHECK), turn);
      return line;
    }
    case State::Checkmate:
      // The side to move has been mated.
      if (settings.opponent == TwoPlayers) {
        snprintf(line, sizeof(line), tr(STR_CHESS_MATE_BY), colourName(white ? chess::Black : chess::White));
        return line;
      }
      return game->sideToMove() == settings.human ? tr(STR_CHESS_MATE_LOST) : tr(STR_CHESS_MATE_WON);
    case State::Stalemate:
      return tr(STR_CHESS_STALEMATE);
    case State::DrawRepetition:
      return tr(STR_GAME_DRAW_REPEATED);
    case State::DrawFiftyMoves:
      return tr(STR_CHESS_DRAW_FIFTY);
    case State::DrawMaterial:
      return tr(STR_CHESS_DRAW_MATERIAL);
    case State::DrawTooLong:
      break;
  }
  return tr(STR_GAME_DRAW);
}

void ChessActivity::drawBoard() const {
  for (int square = 0; square < 64; ++square) {
    // a1 is a dark square.
    if ((((square >> 3) + (square & 7)) & 1) == 0) gameui::hatch(renderer, squareBox(square));
  }
  renderer.drawRect(board.x - 2, board.y - 2, board.w + 4, board.h + 4, 2, true);
  if (!game->started()) return;

  if (game->plies() > 0) {
    gameui::brackets(renderer, squareBox(game->lastMove().fromSquare()));
    gameui::brackets(renderer, squareBox(game->lastMove().toSquare()));
  }
  for (int square = 0; square < 64; ++square) {
    const uint8_t content = game->at(square);
    if (!content) continue;
    const gameui::Box box = squareBox(square);
    const int kind = artIndex(chess::pieceOf(content));
    const appart::Bitmap& ink =
        chess::colourOf(content) == chess::White ? *appart::kPieceWhite[kind] : *appart::kPieceBlack[kind];
    gameui::stampOver(renderer, ink, *appart::kPieceUnder[kind], box.x + kPieceInset, box.y + kPieceInset);
  }

  if (selected >= 0) {
    const gameui::Box box = squareBox(selected);
    renderer.drawRect(box.x, box.y, box.w, box.h, 4, true);
  }
  for (int i = 0; i < targetCount; ++i) {
    const gameui::Box box = squareBox(targets[i]);
    const int cx = box.x + box.w / 2;
    const int cy = box.y + box.h / 2;
    if (game->at(targets[i])) {
      // A piece that can be taken: a ring around it.
      gameui::ring(renderer, cx, cy, 28, 2, false);
      gameui::ring(renderer, cx, cy, 26, 3, true);
    } else {
      gameui::disc(renderer, cx, cy, 10, false);
      gameui::disc(renderer, cx, cy, 7, true);
    }
  }
  for (const int square : {hintFrom, hintTo}) {
    if (square < 0) continue;
    const gameui::Box box = squareBox(square);
    renderer.drawRect(box.x + 5, box.y + 5, box.w - 10, box.h - 10, 3, true);
  }
}

void ChessActivity::drawStrip(const int y, const chess::Colour side) const {
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

  // What this side has taken, most valuable first.
  constexpr chess::Piece kOrder[5] = {chess::Queen, chess::Rook, chess::Bishop, chess::Knight, chess::Pawn};
  const chess::Colour victim = chess::other(side);
  int count = 0;
  for (const chess::Piece piece : kOrder) count += game->lost(victim, piece);
  int x = board.x + board.w - count * kSmallStep - 6;
  for (const chess::Piece piece : kOrder) {
    const int kind = artIndex(piece);
    const appart::Bitmap& art =
        victim == chess::White ? *appart::kSmallPieceWhite[kind] : *appart::kSmallPieceBlack[kind];
    for (int i = game->lost(victim, piece); i > 0; --i) {
      gameui::stampOver(renderer, art, *appart::kSmallPieceUnder[kind], x, y + (kStripHeight - art.height) / 2);
      x += kSmallStep;
    }
  }
}

void ChessActivity::drawNewGamePanel() const {
  gameui::panelFrame(renderer, panelBox);
  gameui::centredText(renderer, UI_12_FONT_ID, {panelBox.x, panelBox.y + 14, panelBox.w, 36}, tr(STR_GAME_NEW), true,
                      EpdFontFamily::BOLD);
  const bool computer = draft.opponent == Computer;
  gameui::choice(renderer, opponentBoxes[0], tr(STR_GAME_COMPUTER), computer);
  gameui::choice(renderer, opponentBoxes[1], tr(STR_GAME_TWO_PLAYERS), !computer);
  for (int i = 0; i < 3; ++i) gameui::choice(renderer, levelBoxes[i], levelName(i), draft.level == i, computer);
  gameui::choice(renderer, colourBoxes[0], tr(STR_CHESS_WHITE), draft.human == chess::White, computer);
  gameui::choice(renderer, colourBoxes[1], tr(STR_CHESS_BLACK), draft.human == chess::Black, computer);
  if (game->started()) gameui::button(renderer, cancelBox, tr(STR_CANCEL), false);
  gameui::button(renderer, startBox, tr(STR_GAME_START), true);
}

void ChessActivity::drawPromotionPanel() const {
  constexpr chess::Piece kChoices[4] = {chess::Queen, chess::Rook, chess::Bishop, chess::Knight};
  const gameui::Box& first = promotionBoxes[0];
  const gameui::Box& last = promotionBoxes[3];
  const gameui::Box frame{first.x - 14, first.y - 14, last.x + last.w - first.x + 28, first.h + 28};
  gameui::panelFrame(renderer, frame);
  const bool white = game->sideToMove() == chess::White;
  for (int i = 0; i < 4; ++i) {
    const gameui::Box& box = promotionBoxes[i];
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 8, true);
    const int kind = artIndex(kChoices[i]);
    const appart::Bitmap& art = white ? *appart::kPieceWhite[kind] : *appart::kPieceBlack[kind];
    gameui::stamp(renderer, art, box.x + (box.w - art.width) / 2, box.y + (box.h - art.height) / 2);
  }
}

void ChessActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_GAME_CHESS));
  if (panel == Panel::Board && game->started()) newHold.draw(renderer, tr(STR_GAME_NEW_SHORT));

  drawStrip(topStripY, settings.flipped ? chess::White : chess::Black);
  drawBoard();
  drawStrip(bottomStripY, settings.flipped ? chess::Black : chess::White);
  gameui::fittedText(renderer, {8, statusY, renderer.getScreenWidth() - 16, 34}, statusText());

  if (game->started() && game->state() != chess::Game::State::Playing) {
    gameui::button(renderer, newGameBox, tr(STR_GAME_NEW), true);
  } else {
    gameui::iconButton(renderer, tools[Undo], appart::kIconUndo, tr(STR_SUDOKU_UNDO), false);
    gameui::iconButton(renderer, tools[Hint], appart::kIconHint, tr(STR_SUDOKU_HINT), false);
    gameui::iconButton(renderer, tools[Flip], appart::kIconFlip, tr(STR_GAME_FLIP), false);
  }

  if (panel == Panel::NewGame) drawNewGamePanel();
  if (panel == Panel::Promotion) drawPromotionPanel();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
