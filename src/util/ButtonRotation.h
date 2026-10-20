#pragma once

#include <cstdint>

// VARAGH: which physical buttons take each other's job when the screen is
// turned, so that the button nearer the start of the text (left or top, as
// the reader now holds the device) always goes back. Always on in the reader;
// it replaced the two "Orientation aware" switches, which swapped the same
// buttons for both landscape directions and so were wrong in one of them.
//
// Portrait layout: the front row Back, Confirm, Left, Right along the bottom
// edge; the side pair Up above Down on the right edge. Orientation values are
// CrossPointSettings::ORIENTATION (0 portrait, 1 turned right, 2 upside down,
// 3 turned left).
namespace ButtonRotation {

enum class Front : uint8_t {
  Same,           // row keeps its jobs
  SwapLeftRight,  // only Left and Right trade jobs
  MirrorRow,      // whole row reversed: Back<->Right, Confirm<->Left
};

constexpr Front front(const uint8_t orientation) {
  switch (orientation) {
    case 1:  // turned right: row runs down the left edge, Left on top
      return Front::Same;
    case 2:  // upside down: row along the top, right to left
      return Front::MirrorRow;
    case 3:  // turned left: row runs down the right edge, Right on top
      return Front::SwapLeftRight;
    default:
      return Front::Same;
  }
}

// Up and Down trade jobs: turned right the pair lies along the bottom with Up
// on the right; upside down it is on the left edge with Up below.
constexpr bool swapSide(const uint8_t orientation) { return orientation == 1 || orientation == 2; }

}  // namespace ButtonRotation
