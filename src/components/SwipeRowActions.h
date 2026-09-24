#pragma once

#include <FreeInkUICore.h>

#include <cstdint>

#include "MappedInputManager.h"

class GfxRenderer;

// iOS-style swipe-to-delete for FreeInkUI list rows.
//
// Swiping a row to the left reveals a Delete button in the space the row
// vacates on the right (drawn in one refresh; e-ink cannot animate the slide).
// Tapping Delete reports the row; tapping anywhere else, swiping right or
// pressing a button hides it again. Rows are found by hit-testing the list's
// own published touch rects, so row height and scrolling need no mirroring.
//
// Usage in an activity:
//   loop():   if (const auto r = swipe.handleInput(app, mappedInput, ACTION_ROW, index); r != None) {
//               if (r == SwipeRowActions::Result::Delete) deleteItem(index);
//               requestUpdate(); return;
//             }            // before app.route()
//   render(): app.render(); swipe.draw(renderer);   // before displayBuffer()
class SwipeRowActions {
 public:
  enum class Result : uint8_t { None, Consumed, Delete };

  template <typename App>
  Result handleInput(const App& app, const MappedInputManager& input, const freeink::ui::ActionId rowAction,
                     int& outIndex) {
    if (active()) return handleRevealedInput(input, outIndex);
    MappedInputManager::SwipeDir dir = MappedInputManager::SwipeDir::None;
    int sx = 0, sy = 0, ex = 0, ey = 0;
    if (!input.wasSwipeWithPoints(dir, sx, sy, ex, ey) || dir != MappedInputManager::SwipeDir::Left) {
      return Result::None;
    }
    const auto* row = app.interactionAt(static_cast<int16_t>(sx), static_cast<int16_t>(sy));
    if (!row || row->action != rowAction || row->value < 0) return Result::None;
    reveal(row->value, row->rect);
    return Result::Consumed;
  }

  // Draw the revealed Delete button over the right end of its row.
  void draw(GfxRenderer& renderer) const;

  // Hide without deleting (e.g. after the list changed underneath).
  void clear() { index_ = -1; }
  bool active() const { return index_ >= 0; }
  int index() const { return index_; }

 private:
  void reveal(int16_t index, const freeink::ui::Rect& rowRect);
  Result handleRevealedInput(const MappedInputManager& input, int& outIndex);

  int16_t index_ = -1;
  freeink::ui::Rect button_{};
};
