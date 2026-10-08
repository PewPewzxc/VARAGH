#include "ChessGame.h"

#include <cstring>
#include <initializer_list>

namespace chess {

namespace {

constexpr int8_t kKnightSteps[8] = {-33, -31, -18, -14, 14, 18, 31, 33};
constexpr int8_t kKingSteps[8] = {-17, -16, -15, -1, 1, 15, 16, 17};
constexpr int8_t kBishopSteps[4] = {-17, -15, 15, 17};
constexpr int8_t kRookSteps[4] = {-16, -1, 1, 16};

constexpr uint32_t kSideKey = 0xA5F00D17U;

constexpr bool onBoard(const int square88) { return (square88 & 0x88) == 0; }
constexpr uint8_t content(const Colour colour, const Piece piece) {
  return static_cast<uint8_t>(piece | (colour == Black ? kBlackBit : 0));
}

uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352DU;
  x ^= x >> 15;
  x *= 0x846CA68BU;
  x ^= x >> 16;
  return x;
}

// Position hashing without a key table: every (piece, square) pair gets its
// key from a mixing function.
uint32_t pieceKey(const uint8_t piece, const uint8_t square) {
  return mix((static_cast<uint32_t>(piece) << 8 | square) + 0x9E3779B9U);
}
uint32_t stateKey(const uint8_t castling, const uint8_t ep) {
  return mix(0x51000000U + (static_cast<uint32_t>(castling) << 8) + ep);
}

// Castling rights that end when a piece leaves or lands on this square.
uint8_t rightsKept(const uint8_t square) {
  switch (square) {
    case 0x00:
      return static_cast<uint8_t>(~Position::WhiteQueen);
    case 0x07:
      return static_cast<uint8_t>(~Position::WhiteKing);
    case 0x04:
      return static_cast<uint8_t>(~(Position::WhiteKing | Position::WhiteQueen));
    case 0x70:
      return static_cast<uint8_t>(~Position::BlackQueen);
    case 0x77:
      return static_cast<uint8_t>(~Position::BlackKing);
    case 0x74:
      return static_cast<uint8_t>(~(Position::BlackKing | Position::BlackQueen));
    default:
      return 0xFF;
  }
}

void movePiece(Position& p, uint32_t& hash, const uint8_t from, const uint8_t to) {
  const uint8_t piece = p.board[from];
  hash ^= pieceKey(piece, from) ^ pieceKey(piece, to);
  p.board[to] = piece;
  p.board[from] = 0;
}

void add(Move*& out, const uint8_t from, const uint8_t to, const uint8_t flags, const uint8_t promotion = NoPiece) {
  out->from = from;
  out->to = to;
  out->flags = flags;
  out->promotion = promotion;
  ++out;
}

void addPawnMove(Move*& out, const uint8_t from, const uint8_t to, const uint8_t flags, const bool promotes,
                 const bool queenOnly) {
  if (!promotes) {
    add(out, from, to, flags);
    return;
  }
  add(out, from, to, flags, Queen);
  if (queenOnly) return;
  add(out, from, to, flags, Knight);
  add(out, from, to, flags, Rook);
  add(out, from, to, flags, Bishop);
}

// Piece-square tables (Tomasz Michniewski's "simplified evaluation function"),
// written from White's side with the eighth rank first.
constexpr int8_t kPawnTable[64] = {
    0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50, 50, 50,  50, 50, 10, 10, 20, 30, 30, 20,
    10, 10, 5,  5,   10,  25, 25, 10, 5,  5,  0,   0,  0,  20,  20, 0,  0,  0,  5,  -5, -10, 0,
    0,  -10, -5, 5,  5,   10, 10, -20, -20, 10, 10, 5,  0,  0,   0,  0,  0,  0,  0,  0};
constexpr int8_t kKnightTable[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40, -30, 0,   10,  15,  15, 10,
    0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,   10, 15,
    15,  10,  5,   -30, -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};
constexpr int8_t kBishopTable[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,   0,   0,   -10, -10, 0,   5,   10,  10, 5,
    0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,   -10, -10, 10,  10, 10,
    10,  10,  10,  -10, -10, 5,   0,   0,   0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};
constexpr int8_t kRookTable[64] = {
    0,  0, 0, 0, 0, 0, 0, 0,  5,  10, 10, 10, 10, 10, 10, 5,  -5, 0, 0, 0, 0, 0,
    0,  -5, -5, 0, 0, 0, 0, 0, 0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5, -5, 0, 0, 0,
    0,  0, 0, -5, -5, 0, 0, 0, 0,  0,  0,  -5, 0,  0,  0,  5,  5,  0,  0,  0};
constexpr int8_t kQueenTable[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10, -10, 0,   5,   5,  5,  5,
    0,   -10, -5,  0,  5,  5,   5,   5,   0,   -5,  0,   0,  5,  5,   5,   5,   0,   -5,  -10, 5,  5,  5,
    5,   5,   0,   -10, -10, 0,  5,   0,   0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20};
constexpr int8_t kKingTable[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
    -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20,
    -20, -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};
constexpr int8_t kKingEndTable[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,   -10, -20, -30, -30, -10, 20,  30,  30, 20,
    -10, -30, -30, -10, 30,  40,  40,  30,  -10, -30, -30, -10, 30,  40,  40,  30,  -10, -30, -30, -10, 20, 30,
    30,  20,  -10, -30, -30, -30, 0,   0,   0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

constexpr int16_t kValue[7] = {0, 100, 320, 330, 500, 900, 0};

const int8_t* tableFor(const Piece piece, const bool endgame) {
  switch (piece) {
    case Pawn:
      return kPawnTable;
    case Knight:
      return kKnightTable;
    case Bishop:
      return kBishopTable;
    case Rook:
      return kRookTable;
    case Queen:
      return kQueenTable;
    default:
      return endgame ? kKingEndTable : kKingTable;
  }
}

}  // namespace

void Position::recomputeHash() {
  uint32_t value = stateKey(castling, ep);
  for (int square = 0; square < 128; ++square) {
    if (onBoard(square) && board[square]) value ^= pieceKey(board[square], static_cast<uint8_t>(square));
  }
  if (side == Black) value ^= kSideKey;
  hash = value;
}

void Position::setStart() { setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"); }

bool Position::setFen(const char* fen) {
  memset(board, 0, sizeof(board));
  castling = 0;
  ep = kNoSquare;
  halfmove = 0;
  bool kings[2] = {false, false};
  int rank = 7;
  int file = 0;
  const char* c = fen;
  for (; *c && *c != ' '; ++c) {
    if (*c == '/') {
      --rank;
      file = 0;
      continue;
    }
    if (*c >= '1' && *c <= '8') {
      file += *c - '0';
      continue;
    }
    const bool black = *c >= 'a';
    Piece piece = NoPiece;
    switch (black ? *c - 32 : *c) {
      case 'P':
        piece = Pawn;
        break;
      case 'N':
        piece = Knight;
        break;
      case 'B':
        piece = Bishop;
        break;
      case 'R':
        piece = Rook;
        break;
      case 'Q':
        piece = Queen;
        break;
      case 'K':
        piece = King;
        break;
      default:
        return false;
    }
    if (rank < 0 || file > 7) return false;
    const auto square = static_cast<uint8_t>(rank * 16 + file);
    board[square] = content(black ? Black : White, piece);
    if (piece == King) {
      king[black ? Black : White] = square;
      kings[black ? Black : White] = true;
    }
    ++file;
  }
  if (!kings[White] || !kings[Black] || *c != ' ') return false;
  ++c;
  side = *c == 'b' ? Black : White;
  if (*c) ++c;
  if (*c == ' ') ++c;
  for (; *c && *c != ' '; ++c) {
    if (*c == 'K') castling |= WhiteKing;
    if (*c == 'Q') castling |= WhiteQueen;
    if (*c == 'k') castling |= BlackKing;
    if (*c == 'q') castling |= BlackQueen;
  }
  if (*c == ' ') ++c;
  if (c[0] >= 'a' && c[0] <= 'h' && c[1] >= '1' && c[1] <= '8') {
    ep = static_cast<uint8_t>((c[1] - '1') * 16 + (c[0] - 'a'));
  }
  recomputeHash();
  return true;
}

bool attacked(const Position& p, const uint8_t square, const Colour by) {
  // A white pawn attacks the two squares diagonally above it.
  const uint8_t pawn = content(by, Pawn);
  const int pawnRow = by == White ? -16 : 16;
  for (const int side : {-1, 1}) {
    const int from = square + pawnRow + side;
    if (onBoard(from) && p.board[from] == pawn) return true;
  }
  const uint8_t knight = content(by, Knight);
  for (const int8_t step : kKnightSteps) {
    const int from = square + step;
    if (onBoard(from) && p.board[from] == knight) return true;
  }
  const uint8_t king = content(by, King);
  for (const int8_t step : kKingSteps) {
    const int from = square + step;
    if (onBoard(from) && p.board[from] == king) return true;
  }
  const uint8_t bishop = content(by, Bishop);
  const uint8_t rook = content(by, Rook);
  const uint8_t queen = content(by, Queen);
  for (const int8_t step : kBishopSteps) {
    for (int from = square + step; onBoard(from); from += step) {
      const uint8_t piece = p.board[from];
      if (!piece) continue;
      if (piece == bishop || piece == queen) return true;
      break;
    }
  }
  for (const int8_t step : kRookSteps) {
    for (int from = square + step; onBoard(from); from += step) {
      const uint8_t piece = p.board[from];
      if (!piece) continue;
      if (piece == rook || piece == queen) return true;
      break;
    }
  }
  return false;
}

void makeMove(Position& p, const Move& m, Undo& u) {
  const uint8_t piece = p.board[m.from];
  const auto us = static_cast<Colour>(p.side);
  u.captured = p.board[m.to];
  u.castling = p.castling;
  u.ep = p.ep;
  u.halfmove = p.halfmove;
  u.hash = p.hash;

  uint32_t hash = p.hash ^ stateKey(p.castling, p.ep);
  if (m.flags & Move::EnPassant) {
    const auto pawnSquare = static_cast<uint8_t>(m.to + (us == White ? -16 : 16));
    u.captured = p.board[pawnSquare];
    hash ^= pieceKey(u.captured, pawnSquare);
    p.board[pawnSquare] = 0;
  } else if (u.captured) {
    hash ^= pieceKey(u.captured, m.to);
  }
  movePiece(p, hash, m.from, m.to);
  if (m.promotion) {
    const uint8_t promoted = content(us, static_cast<Piece>(m.promotion));
    hash ^= pieceKey(piece, m.to) ^ pieceKey(promoted, m.to);
    p.board[m.to] = promoted;
  }
  if (pieceOf(piece) == King) {
    p.king[us] = m.to;
    if (m.flags & Move::Castle) {
      if (m.to > m.from) {
        movePiece(p, hash, static_cast<uint8_t>(m.from + 3), static_cast<uint8_t>(m.from + 1));
      } else {
        movePiece(p, hash, static_cast<uint8_t>(m.from - 4), static_cast<uint8_t>(m.from - 1));
      }
    }
  }
  p.castling &= rightsKept(m.from) & rightsKept(m.to);
  p.ep = (m.flags & Move::DoublePush) ? static_cast<uint8_t>((m.from + m.to) / 2) : kNoSquare;
  p.halfmove = (pieceOf(piece) == Pawn || u.captured) ? 0 : static_cast<uint8_t>(p.halfmove + 1);
  p.side = other(us);
  p.hash = hash ^ stateKey(p.castling, p.ep) ^ kSideKey;
}

void unmakeMove(Position& p, const Move& m, const Undo& u) {
  const Colour us = other(static_cast<Colour>(p.side));
  uint8_t piece = p.board[m.to];
  if (m.promotion) piece = content(us, Pawn);
  p.board[m.from] = piece;
  if (m.flags & Move::EnPassant) {
    p.board[m.to] = 0;
    p.board[m.to + (us == White ? -16 : 16)] = u.captured;
  } else {
    p.board[m.to] = u.captured;
  }
  if (pieceOf(piece) == King) {
    p.king[us] = m.from;
    if (m.flags & Move::Castle) {
      if (m.to > m.from) {
        p.board[m.from + 3] = p.board[m.from + 1];
        p.board[m.from + 1] = 0;
      } else {
        p.board[m.from - 4] = p.board[m.from - 1];
        p.board[m.from - 1] = 0;
      }
    }
  }
  p.side = us;
  p.castling = u.castling;
  p.ep = u.ep;
  p.halfmove = u.halfmove;
  p.hash = u.hash;
}

int generatePseudo(const Position& p, Move* out, const bool capturesOnly) {
  Move* const first = out;
  const auto us = static_cast<Colour>(p.side);
  const Colour them = other(us);
  const int forward = us == White ? 16 : -16;
  const int startRank = us == White ? 1 : 6;
  const int lastRank = us == White ? 7 : 0;

  for (int square = 0; square < 128; ++square) {
    if (!onBoard(square)) {
      square += 7;  // skip the off-board half of the rank
      continue;
    }
    const uint8_t piece = p.board[square];
    if (!piece || colourOf(piece) != us) continue;
    const auto from = static_cast<uint8_t>(square);

    switch (pieceOf(piece)) {
      case Pawn: {
        const int ahead = square + forward;
        const bool promotes = (ahead >> 4) == lastRank;
        if (onBoard(ahead) && !p.board[ahead]) {
          if (!capturesOnly || promotes) {
            addPawnMove(out, from, static_cast<uint8_t>(ahead), 0, promotes, capturesOnly);
          }
          const int two = ahead + forward;
          if (!capturesOnly && (square >> 4) == startRank && !p.board[two]) {
            add(out, from, static_cast<uint8_t>(two), Move::DoublePush);
          }
        }
        for (const int side : {-1, 1}) {
          const int to = ahead + side;
          if (!onBoard(to)) continue;
          const uint8_t target = p.board[to];
          if (target && colourOf(target) == them) {
            addPawnMove(out, from, static_cast<uint8_t>(to), Move::Capture, promotes, capturesOnly);
          } else if (!target && to == p.ep) {
            add(out, from, static_cast<uint8_t>(to), Move::Capture | Move::EnPassant);
          }
        }
        break;
      }
      case Knight:
      case King: {
        const int8_t* steps = pieceOf(piece) == Knight ? kKnightSteps : kKingSteps;
        for (int i = 0; i < 8; ++i) {
          const int to = square + steps[i];
          if (!onBoard(to)) continue;
          const uint8_t target = p.board[to];
          if (!target) {
            if (!capturesOnly) add(out, from, static_cast<uint8_t>(to), 0);
          } else if (colourOf(target) == them) {
            add(out, from, static_cast<uint8_t>(to), Move::Capture);
          }
        }
        break;
      }
      default: {
        const Piece kind = pieceOf(piece);
        for (int group = 0; group < 2; ++group) {
          if (group == 0 && kind == Rook) continue;
          if (group == 1 && kind == Bishop) continue;
          const int8_t* steps = group == 0 ? kBishopSteps : kRookSteps;
          for (int i = 0; i < 4; ++i) {
            for (int to = square + steps[i]; onBoard(to); to += steps[i]) {
              const uint8_t target = p.board[to];
              if (!target) {
                if (!capturesOnly) add(out, from, static_cast<uint8_t>(to), 0);
                continue;
              }
              if (colourOf(target) == them) add(out, from, static_cast<uint8_t>(to), Move::Capture);
              break;
            }
          }
        }
        break;
      }
    }
  }

  if (!capturesOnly) {
    // The king may not castle out of, through or into check.
    const uint8_t home = us == White ? 0x04 : 0x74;
    const uint8_t kingSide = us == White ? Position::WhiteKing : Position::BlackKing;
    const uint8_t queenSide = us == White ? Position::WhiteQueen : Position::BlackQueen;
    if (p.king[us] == home && (p.castling & (kingSide | queenSide)) && !attacked(p, home, them)) {
      if ((p.castling & kingSide) && !p.board[home + 1] && !p.board[home + 2] &&
          p.board[home + 3] == content(us, Rook) && !attacked(p, static_cast<uint8_t>(home + 1), them) &&
          !attacked(p, static_cast<uint8_t>(home + 2), them)) {
        add(out, home, static_cast<uint8_t>(home + 2), Move::Castle);
      }
      if ((p.castling & queenSide) && !p.board[home - 1] && !p.board[home - 2] && !p.board[home - 3] &&
          p.board[home - 4] == content(us, Rook) && !attacked(p, static_cast<uint8_t>(home - 1), them) &&
          !attacked(p, static_cast<uint8_t>(home - 2), them)) {
        add(out, home, static_cast<uint8_t>(home - 2), Move::Castle);
      }
    }
  }
  return static_cast<int>(out - first);
}

int generateLegal(Position& p, Move* out) {
  const int count = generatePseudo(p, out, false);
  const auto us = static_cast<Colour>(p.side);
  int legal = 0;
  for (int i = 0; i < count; ++i) {
    const Move move = out[i];
    Undo undo;
    makeMove(p, move, undo);
    if (!inCheck(p, us)) out[legal++] = move;
    unmakeMove(p, move, undo);
  }
  return legal;
}

uint64_t perft(Position& p, const int depth) {
  if (depth == 0) return 1;
  Move moves[kMaxMoves];
  const int count = generatePseudo(p, moves, false);
  const auto us = static_cast<Colour>(p.side);
  uint64_t total = 0;
  for (int i = 0; i < count; ++i) {
    Undo undo;
    makeMove(p, moves[i], undo);
    if (!inCheck(p, us)) total += perft(p, depth - 1);
    unmakeMove(p, moves[i], undo);
  }
  return total;
}

int evaluate(const Position& p) {
  int material[2] = {0, 0};
  for (int square = 0; square < 128; ++square) {
    if (!onBoard(square)) {
      square += 7;
      continue;
    }
    const uint8_t piece = p.board[square];
    if (piece && pieceOf(piece) != Pawn) material[colourOf(piece)] += kValue[pieceOf(piece)];
  }
  // With the heavy pieces gone the king should walk to the centre.
  const bool endgame = material[White] + material[Black] <= 2600;

  int score = 0;
  for (int square = 0; square < 128; ++square) {
    if (!onBoard(square)) {
      square += 7;
      continue;
    }
    const uint8_t piece = p.board[square];
    if (!piece) continue;
    const Piece kind = pieceOf(piece);
    const int file = square & 7;
    const int rank = square >> 4;
    const bool white = colourOf(piece) == White;
    const int value = kValue[kind] + tableFor(kind, endgame)[(white ? 7 - rank : rank) * 8 + file];
    score += white ? value : -value;
  }
  return p.side == White ? score : -score;
}

// --- search -------------------------------------------------------------------------------------
namespace {

constexpr int kMaxPly = 48;
constexpr int kPoolSize = 3072;
constexpr int kHistoryKept = 100;
constexpr int kInfinity = 32000;

struct Searcher {
  Position pos;
  StopFn stop = nullptr;
  void* context = nullptr;
  uint32_t nodes = 0;
  uint32_t maxNodes = 0;
  bool stopped = false;
  Move killers[kMaxPly][2];
  // Hashes of the game so far, then of the line being searched.
  uint32_t seen[kHistoryKept + kMaxPly + 1];
  int rootIndex = 0;
  // Every position on the line being searched takes its move list from this
  // one pool, which keeps a stack frame of the search to a few dozen bytes.
  Move pool[kPoolSize];
  int16_t poolScores[kPoolSize];
  int poolTop = 0;
  Move rootMoves[kMaxMoves];
  int rootScores[kMaxMoves];
  // How often a quiet move of this piece to this square refuted a line; such
  // moves are tried early everywhere else.
  uint16_t history[16][128];

  void poll() {
    if (maxNodes && nodes >= maxNodes) stopped = true;
    if ((nodes & 1023) == 0 && stop && stop(context)) stopped = true;
  }

  bool repeated(const int ply) const {
    const int here = rootIndex + ply;
    const int oldest = here - pos.halfmove;
    for (int i = here - 2; i >= 0 && i >= oldest; i -= 2) {
      if (seen[i] == pos.hash) return true;
    }
    return false;
  }

  // Captures first, the most valuable victim by the least valuable attacker.
  int16_t orderScore(const Move& m, const int ply) const {
    if (m.flags & Move::Capture) {
      const int victim = (m.flags & Move::EnPassant) ? Pawn : pieceOf(pos.board[m.to]);
      return static_cast<int16_t>(10000 + victim * 16 - pieceOf(pos.board[m.from]));
    }
    if (m.promotion == Queen) return 9000;
    if (ply < kMaxPly && (m.sameAs(killers[ply][0]) || m.sameAs(killers[ply][1]))) return 8000;
    const uint16_t seenGood = history[pos.board[m.from] & 15][m.to];
    return static_cast<int16_t>(seenGood < 7000 ? seenGood : 7000);
  }

  bool hasPieces(const Colour side) const {
    for (int square = 0; square < 128; ++square) {
      if (!onBoard(square)) {
        square += 7;
        continue;
      }
      const uint8_t piece = pos.board[square];
      if (piece && colourOf(piece) == side && pieceOf(piece) != Pawn && pieceOf(piece) != King) return true;
    }
    return false;
  }

  void pickNext(Move* moves, int16_t* scores, const int count, const int index) const {
    int best = index;
    for (int i = index + 1; i < count; ++i) {
      if (scores[i] > scores[best]) best = i;
    }
    if (best != index) {
      const Move move = moves[index];
      moves[index] = moves[best];
      moves[best] = move;
      const int16_t score = scores[index];
      scores[index] = scores[best];
      scores[best] = score;
    }
  }

  int quiesce(int alpha, const int beta, const int ply) {
    ++nodes;
    poll();
    if (stopped) return 0;
    const int stand = evaluate(pos);
    if (ply >= kMaxPly - 1) return stand;
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    if (poolTop + kMaxMoves > kPoolSize) return alpha;

    Move* const moves = pool + poolTop;
    int16_t* const scores = poolScores + poolTop;
    const int count = generatePseudo(pos, moves, true);
    for (int i = 0; i < count; ++i) scores[i] = orderScore(moves[i], kMaxPly);
    const int base = poolTop;
    poolTop += count;
    const auto us = static_cast<Colour>(pos.side);
    for (int i = 0; i < count; ++i) {
      pickNext(moves, scores, count, i);
      // A capture that cannot lift the score to alpha even with a margin is not worth a look.
      const int victim = (moves[i].flags & Move::EnPassant) ? Pawn : pieceOf(pos.board[moves[i].to]);
      if (stand + kValue[victim] + (moves[i].promotion ? 800 : 0) + 200 <= alpha) continue;
      Undo undo;
      makeMove(pos, moves[i], undo);
      if (inCheck(pos, us)) {
        unmakeMove(pos, moves[i], undo);
        continue;
      }
      const int score = -quiesce(-beta, -alpha, ply + 1);
      unmakeMove(pos, moves[i], undo);
      if (stopped || score >= beta) {
        poolTop = base;
        return stopped ? 0 : beta;
      }
      if (score > alpha) alpha = score;
    }
    poolTop = base;
    return alpha;
  }

  int negamax(int depth, int alpha, const int beta, const int ply, const bool mayPass = true) {
    if (pos.halfmove >= 100 || repeated(ply)) return 0;
    const auto us = static_cast<Colour>(pos.side);
    const bool checked = inCheck(pos, us);
    if (checked) ++depth;  // never stop looking while in check
    if (depth <= 0) return quiesce(alpha, beta, ply);
    ++nodes;
    poll();
    if (stopped) return 0;
    if (ply >= kMaxPly - 1 || poolTop + kMaxMoves > kPoolSize) return evaluate(pos);

    // Null move: if the position is still too good after giving the opponent
    // a free move, the line needs no further look. Not with only pawns left,
    // where having to move can be the whole problem.
    if (mayPass && !checked && depth >= 3 && beta < kMateScore - 1000 && hasPieces(us)) {
      const uint8_t ep = pos.ep;
      const uint32_t hash = pos.hash;
      pos.hash ^= stateKey(pos.castling, pos.ep) ^ stateKey(pos.castling, kNoSquare) ^ kSideKey;
      pos.ep = kNoSquare;
      pos.side = other(us);
      seen[rootIndex + ply + 1] = pos.hash;
      const int score = -negamax(depth - 3, -beta, -beta + 1, ply + 1, false);
      pos.side = us;
      pos.ep = ep;
      pos.hash = hash;
      if (stopped) return 0;
      if (score >= beta) return beta;
    }

    Move* const moves = pool + poolTop;
    int16_t* const scores = poolScores + poolTop;
    const int count = generatePseudo(pos, moves, false);
    for (int i = 0; i < count; ++i) scores[i] = orderScore(moves[i], ply);
    const int base = poolTop;
    poolTop += count;
    int legal = 0;
    for (int i = 0; i < count; ++i) {
      pickNext(moves, scores, count, i);
      Undo undo;
      makeMove(pos, moves[i], undo);
      if (inCheck(pos, us)) {
        unmakeMove(pos, moves[i], undo);
        continue;
      }
      ++legal;
      seen[rootIndex + ply + 1] = pos.hash;
      const int score = -negamax(depth - 1, -beta, -alpha, ply + 1);
      unmakeMove(pos, moves[i], undo);
      if (stopped) {
        poolTop = base;
        return 0;
      }
      if (score >= beta) {
        if (!(moves[i].flags & Move::Capture)) {
          if (!moves[i].sameAs(killers[ply][0])) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = moves[i];
          }
          uint16_t& seenGood = history[pos.board[moves[i].from] & 15][moves[i].to];
          if (seenGood < 60000) seenGood = static_cast<uint16_t>(seenGood + depth * depth);
        }
        poolTop = base;
        return beta;
      }
      if (score > alpha) alpha = score;
    }
    poolTop = base;
    if (legal == 0) return checked ? -kMateScore + ply : 0;
    return alpha;
  }
};

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

}  // namespace

SearchResult search(const Position& root, const SearchOptions& options, const uint32_t* history,
                    const int historyCount, const StopFn stop, void* context) {
  SearchResult result;
  // The move pool makes this too large for a task stack.
  auto* searcher = new Searcher();
  Searcher& s = *searcher;
  s.pos = root;
  s.stop = stop;
  s.context = context;
  s.maxNodes = options.maxNodes;
  memset(s.killers, 0, sizeof(s.killers));
  memset(s.history, 0, sizeof(s.history));
  const int kept = history ? (historyCount < kHistoryKept ? historyCount : kHistoryKept) : 0;
  for (int i = 0; i < kept; ++i) s.seen[i] = history[historyCount - kept + i];
  // The last history entry is the root itself.
  s.rootIndex = kept > 0 ? kept - 1 : 0;
  s.seen[s.rootIndex] = s.pos.hash;

  Move* const moves = s.rootMoves;
  int* const scores = s.rootScores;
  const int count = generateLegal(s.pos, moves);
  if (count == 0) {
    delete searcher;
    return result;
  }
  result.best = moves[0];
  result.found = true;
  uint32_t random = options.seed ? options.seed : 1;
  const int margin = options.margin > 0 ? options.margin : 0;
  const int maxDepth = options.maxDepth < 1 ? 1 : (options.maxDepth > kMaxPly - 8 ? kMaxPly - 8 : options.maxDepth);

  for (int depth = 1; depth <= maxDepth; ++depth) {
    int best = -kInfinity;
    int bestIndex = 0;
    bool complete = true;
    for (int i = 0; i < count; ++i) {
      Undo undo;
      makeMove(s.pos, moves[i], undo);
      s.seen[s.rootIndex + 1] = s.pos.hash;
      // The window opens below the best score by the margin, so every move
      // that is a candidate gets its exact score.
      const int alpha = best == -kInfinity ? -kInfinity : best - margin - 1;
      const int score = -s.negamax(depth - 1, -kInfinity, -alpha, 1);
      unmakeMove(s.pos, moves[i], undo);
      if (s.stopped) {
        complete = false;
        break;
      }
      scores[i] = score;
      if (score > best) {
        best = score;
        bestIndex = i;
      }
    }
    if (!complete) break;

    int chosen = bestIndex;
    // A forced mate is always played, never traded for variety.
    if (margin > 0 && best < kMateScore - 1000) {
      int candidateCount = 0;
      for (int i = 0; i < count; ++i) {
        if (scores[i] >= best - margin) ++candidateCount;
      }
      int pick = static_cast<int>(nextRandom(random) % static_cast<uint32_t>(candidateCount));
      for (int i = 0; i < count; ++i) {
        if (scores[i] >= best - margin && pick-- == 0) chosen = i;
      }
    }
    result.best = moves[chosen];
    result.score = best;
    result.depth = depth;

    // Search the best move first on the next, deeper pass.
    const Move bestMove = moves[bestIndex];
    for (int i = bestIndex; i > 0; --i) moves[i] = moves[i - 1];
    moves[0] = bestMove;
    s.poolTop = 0;
    if (best > kMateScore - 1000 || best < -kMateScore + 1000) break;
  }
  result.nodes = s.nodes;
  delete searcher;
  return result;
}

// --- game ---------------------------------------------------------------------------------------
namespace {

constexpr uint8_t kSaveMagic[4] = {'V', 'C', 'H', '1'};

uint16_t pack(const int from, const int to, const Piece promotion) {
  return static_cast<uint16_t>(from | (to << 6) | (promotion << 12));
}

}  // namespace

void Game::start() {
  pos_.setStart();
  plies_ = 0;
  last_ = Move{};
  hashes_[0] = pos_.hash;
  state_ = State::Playing;
}

int Game::targets(const int from, uint8_t* out) const {
  if (state_ != State::Playing) return 0;
  Position copy = pos_;
  Move* const moves = scratch_;
  const int count = generateLegal(copy, moves);
  int found = 0;
  for (int i = 0; i < count; ++i) {
    if (moves[i].fromSquare() != from) continue;
    const auto to = static_cast<uint8_t>(moves[i].toSquare());
    bool listed = false;
    for (int j = 0; j < found; ++j) listed = listed || out[j] == to;
    if (!listed) out[found++] = to;
  }
  return found;
}

bool Game::isPromotion(const int from, const int to) const {
  const uint8_t piece = at(from);
  return pieceOf(piece) == Pawn && (to >> 3) == (colourOf(piece) == White ? 7 : 0);
}

bool Game::apply(const int from, const int to, const Piece promotion) {
  Move* const moves = scratch_;
  const int count = generateLegal(pos_, moves);
  for (int i = 0; i < count; ++i) {
    const Move& m = moves[i];
    if (m.fromSquare() != from || m.toSquare() != to) continue;
    if (m.promotion && m.promotion != promotion) continue;
    Undo undo;
    makeMove(pos_, m, undo);
    moves_[plies_] = pack(from, to, static_cast<Piece>(m.promotion));
    ++plies_;
    hashes_[plies_] = pos_.hash;
    last_ = m;
    return true;
  }
  return false;
}

bool Game::play(const int from, const int to, const Piece promotion) {
  if (state_ != State::Playing || plies_ >= kMaxPlies) return false;
  if (!apply(from, to, promotion)) return false;
  updateState();
  return true;
}

bool Game::undo() {
  if (!started() || plies_ == 0) return false;
  const int keep = plies_ - 1;
  pos_.setStart();
  plies_ = 0;
  last_ = Move{};
  hashes_[0] = pos_.hash;
  for (int i = 0; i < keep; ++i) {
    const uint16_t packed = moves_[i];
    if (!apply(packed & 63, (packed >> 6) & 63, static_cast<Piece>(packed >> 12))) break;
  }
  updateState();
  return true;
}

void Game::updateState() {
  if (generateLegal(pos_, scratch_) == 0) {
    state_ = check() ? State::Checkmate : State::Stalemate;
    return;
  }
  if (pos_.halfmove >= 100) {
    state_ = State::DrawFiftyMoves;
    return;
  }
  int same = 0;
  for (int i = plies_; i >= 0 && i >= plies_ - pos_.halfmove; i -= 2) {
    if (hashes_[i] == pos_.hash) ++same;
  }
  if (same >= 3) {
    state_ = State::DrawRepetition;
    return;
  }
  // King against king, or with a single knight or bishop, cannot be won.
  int minors = 0;
  bool other = false;
  for (int square = 0; square < 64; ++square) {
    const Piece piece = pieceOf(at(square));
    if (piece == Knight || piece == Bishop) ++minors;
    if (piece == Pawn || piece == Rook || piece == Queen) other = true;
  }
  if (!other && minors <= 1) {
    state_ = State::DrawMaterial;
    return;
  }
  state_ = plies_ >= kMaxPlies ? State::DrawTooLong : State::Playing;
}

int Game::lost(const Colour colour, const Piece piece) const {
  constexpr int kStart[7] = {0, 8, 2, 2, 2, 1, 1};
  int present = 0;
  for (int square = 0; square < 64; ++square) {
    const uint8_t value = at(square);
    if (value && pieceOf(value) == piece && colourOf(value) == colour) ++present;
  }
  return present >= kStart[piece] ? 0 : kStart[piece] - present;
}

size_t Game::save(uint8_t out[kSaveBytes]) const {
  memset(out, 0, kSaveBytes);
  memcpy(out, kSaveMagic, 4);
  out[4] = started() ? 1 : 0;
  out[5] = static_cast<uint8_t>(plies_ & 0xFF);
  out[6] = static_cast<uint8_t>(plies_ >> 8);
  for (int i = 0; i < plies_; ++i) {
    out[8 + i * 2] = static_cast<uint8_t>(moves_[i] & 0xFF);
    out[9 + i * 2] = static_cast<uint8_t>(moves_[i] >> 8);
  }
  return kSaveBytes;
}

bool Game::load(const uint8_t* data, const size_t size) {
  state_ = State::NotStarted;
  plies_ = 0;
  if (size < kSaveBytes || memcmp(data, kSaveMagic, 4) != 0) return false;
  if (data[4] == 0) return true;
  const int count = data[5] | (data[6] << 8);
  if (count > kMaxPlies) return false;
  start();
  for (int i = 0; i < count; ++i) {
    const uint16_t packed = static_cast<uint16_t>(data[8 + i * 2] | (data[9 + i * 2] << 8));
    if (!apply(packed & 63, (packed >> 6) & 63, static_cast<Piece>(packed >> 12))) {
      state_ = State::NotStarted;
      plies_ = 0;
      return false;
    }
  }
  updateState();
  return true;
}

}  // namespace chess
