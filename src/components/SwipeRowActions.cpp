#include "SwipeRowActions.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>

#include "fontIds.h"

namespace {
// iOS sizes the Delete action to its label plus generous padding (about 74 pt
// on a 390 pt-wide phone, ~19%). On the X4 Pro's 480 px portrait width that is
// ~90 px, comfortably above a fingertip; never narrower than 80 px.
constexpr int kMinButtonWidth = 80;
constexpr int kButtonWidthPercent = 19;
}  // namespace

void SwipeRowActions::reveal(const int16_t index, const freeink::ui::Rect& rowRect) {
  index_ = index;
  const int width = std::max<int>(kMinButtonWidth, rowRect.width * kButtonWidthPercent / 100);
  button_.width = static_cast<int16_t>(std::min<int>(width, rowRect.width));
  button_.height = rowRect.height;
  button_.x = static_cast<int16_t>(rowRect.x + rowRect.width - button_.width);
  button_.y = rowRect.y;
}

SwipeRowActions::Result SwipeRowActions::handleRevealedInput(const MappedInputManager& input, int& outIndex) {
  int tx = 0;
  int ty = 0;
  if (input.wasScreenTapped(tx, ty)) {
    const bool onButton = button_.contains(static_cast<int16_t>(tx), static_cast<int16_t>(ty));
    if (onButton) outIndex = index_;
    index_ = -1;
    return onButton ? Result::Delete : Result::Consumed;
  }
  if (input.wasSwipe() != MappedInputManager::SwipeDir::None || input.wasAnyReleased()) {
    index_ = -1;
    return Result::Consumed;
  }
  return Result::None;
}

void SwipeRowActions::draw(GfxRenderer& renderer) const {
  if (!active()) return;
  // Black fill, white label: the e-ink stand-in for iOS's destructive red.
  renderer.fillRect(button_.x, button_.y, button_.width, button_.height, true);
  const char* label = tr(STR_DELETE);
  const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
  const int textX = button_.x + std::max(0, (button_.width - textWidth) / 2);
  const int textY = button_.y + std::max(0, (button_.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2);
  renderer.drawText(UI_10_FONT_ID, textX, textY, label, false);
}
