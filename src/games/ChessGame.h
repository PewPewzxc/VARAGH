#pragma once

#include <cstddef>
#include <cstdint>

// Chess rules and a small computer opponent, free of any device code so the
// host tests can count positions (perft) and play games against it.
//
// Squares in the public interface run 0..63 with a1 = 0, b1 = 1, ... h8 = 63.
namespace chess {

enum Piece : uint8_t { NoPiece = 0, Pawn = 1, Knight = 2, Bishop = 3, Rook = 4, Queen = 5, King = 6 };
enum Colour : uint8_t { White = 0, Black = 1 };

// A square's content: 0 when empty, otherwise the piece in the low three bits
// and kBlackBit set for a black piece.
constexpr uint8_t kBlackBit = 8;
constexpr uint8_t kNoSquare = 0xFF;

constexpr Piece pieceOf(const uint8_t content) { return static_cast<Piece>(content & 7); }
constexpr Colour colourOf(const uint8_t content) { return (content & kBlackBit) ? Black : White; }
constexpr Colour other(const Colour colour) { return colour == White ? Black : White; }

// The board is kept in "0x88" form (16 columns a rank, half of them off the
// board), which turns the edge test into one AND.
constexpr uint8_t to88(const int square) { return static_cast<uint8_t>(square + (square & ~7)); }
constexpr int to64(const uint8_t square88) { return (square88 + (square88 & 7)) >> 1; }

struct Move {
  enum Flag : uint8_t { Capture = 1, EnPassant = 2, Castle = 4, DoublePush = 8 };

  uint8_t from = 0;  // 0x88 squares
  uint8_t to = 0;
  uint8_t promotion = NoPiece;
  uint8_t flags = 0;

  int fromSquare() const { return to64(from); }
  int toSquare() const { return to64(to); }
  bool sameAs(const Move& o) const { return from == o.from && to == o.to && promotion == o.promotion; }
};

struct Undo {
  uint8_t captured = 0;
  uint8_t castling = 0;
  uint8_t ep = kNoSquare;
  uint8_t halfmove = 0;
  uint32_t hash = 0;
};

struct Position {
  enum Castling : uint8_t { WhiteKing = 1, WhiteQueen = 2, BlackKing = 4, BlackQueen = 8 };

  uint8_t board[128] = {};
  uint8_t side = White;
  uint8_t castling = 0;
  uint8_t ep = kNoSquare;  // square a pawn may capture onto en passant
  uint8_t halfmove = 0;    // plies since a capture or a pawn move
  uint8_t king[2] = {0x04, 0x74};
  uint32_t hash = 0;

  void setStart();
  // Reads the first four fields of a FEN string (the move counters are optional).
  bool setFen(const char* fen);
  void recomputeHash();
};

constexpr int kMaxMoves = 256;  // no position has more than 218 legal moves

bool attacked(const Position& position, uint8_t square88, Colour by);
inline bool inCheck(const Position& position, const Colour side) {
  return attacked(position, position.king[side], other(side));
}
void makeMove(Position& position, const Move& move, Undo& undo);
void unmakeMove(Position& position, const Move& move, const Undo& undo);
// Moves that obey how the pieces move but may leave the own king in check.
int generatePseudo(const Position& position, Move* out, bool capturesOnly);
int generateLegal(Position& position, Move* out);
// Number of move sequences of the given length; the standard move-generator test.
uint64_t perft(Position& position, int depth);
// Centipawns from the point of view of the side to move.
int evaluate(const Position& position);

constexpr int kMateScore = 30000;

struct SearchOptions {
  int maxDepth = 4;
  uint32_t maxNodes = 0;  // 0 = no limit
  // Root moves scoring within this many centipawns of the best are all
  // candidates and one is picked at random: variety, and a weaker Easy level.
  int margin = 0;
  uint32_t seed = 1;
};

struct SearchResult {
  Move best;
  bool found = false;
  int score = 0;
  int depth = 0;  // last depth searched completely
  uint32_t nodes = 0;
};

// Polled every thousand positions or so; return true to end the search with
// the best move found so far. The device uses it for its time limit and to let
// other tasks run.
using StopFn = bool (*)(void* context);

// history: hashes of the positions already played in this game, oldest first,
// so the search can see a repetition coming. May be null.
SearchResult search(const Position& root, const SearchOptions& options, const uint32_t* history, int historyCount,
                    StopFn stop = nullptr, void* context = nullptr);

// A game from the starting position: the moves played, whose turn it is and
// how it ended. Undo replays the game from the start, so it never drifts.
class Game {
 public:
  enum class State : uint8_t {
    NotStarted,
    Playing,
    Checkmate,  // the side to move has lost
    Stalemate,
    DrawRepetition,
    DrawFiftyMoves,
    DrawMaterial,
    DrawTooLong,
  };

  static constexpr int kMaxPlies = 400;
  static constexpr size_t kSaveBytes = 8 + kMaxPlies * 2;

  void start();
  State state() const { return state_; }
  bool started() const { return state_ != State::NotStarted; }
  const Position& position() const { return pos_; }
  uint8_t at(const int square) const { return pos_.board[to88(square)]; }
  Colour sideToMove() const { return static_cast<Colour>(pos_.side); }
  bool check() const { return inCheck(pos_, sideToMove()); }
  int plies() const { return plies_; }
  // Valid when plies() > 0.
  Move lastMove() const { return last_; }

  // Squares the piece on `from` may move to. Returns their number.
  int targets(int from, uint8_t* out) const;
  bool isPromotion(int from, int to) const;
  bool play(int from, int to, Piece promotion = Queen);
  bool play(const Move& move) { return play(move.fromSquare(), move.toSquare(), static_cast<Piece>(move.promotion)); }
  bool undo();

  // How many of this piece the given colour has lost.
  int lost(Colour colour, Piece piece) const;
  const uint32_t* hashes() const { return hashes_; }
  int hashCount() const { return plies_ + 1; }

  size_t save(uint8_t out[kSaveBytes]) const;
  bool load(const uint8_t* data, size_t size);

 private:
  bool apply(int from, int to, Piece promotion);
  void updateState();

  Position pos_;
  State state_ = State::NotStarted;
  uint16_t plies_ = 0;
  Move last_;
  uint16_t moves_[kMaxPlies] = {};
  uint32_t hashes_[kMaxPlies + 1] = {};
  // Move lists are a kilobyte each: kept here, off the small main-task stack.
  mutable Move scratch_[kMaxMoves];
};

}  // namespace chess
