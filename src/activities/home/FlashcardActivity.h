#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "highlights/HighlightStore.h"
#include "util/ButtonNavigator.h"

// Flashcard review for a Flashcards category (Leitner boxes, see
// HighlightFormat). Front: the word and the sentence it came from. Tap to
// flip to the saved meaning, then "Again" or "Know it". Swipe left/right (or
// the side buttons) to move between cards. Cards in lower boxes come first.
class FlashcardActivity final : public Activity {
 public:
  FlashcardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint16_t categoryId);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Rect4 {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  };

  void startSession();
  void showCard(size_t position);
  void answer(bool knewIt);
  void layout();
  void drawScreen(int font);
  int textFont() const;
  size_t knownCount() const;

  const uint16_t categoryId;
  std::string categoryName;
  std::vector<uint32_t> offsets;
  std::vector<uint8_t> levels;
  std::vector<uint16_t> order;
  size_t position = 0;
  bool finished = false;
  bool flipped = false;
  HighlightFormat::Entry card;

  Rect4 cardRect;
  Rect4 againRect;
  Rect4 knowRect;
  Rect4 restartRect;
  ButtonNavigator buttonNavigator;
};
