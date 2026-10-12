#pragma once

#include <string>

#include "activities/Activity.h"
#include "activities/apps/GameUi.h"
#include "components/StarRating.h"

// Give a book its rating: five stars with halves. A tap on a star sets and
// saves the rating at once; its left half counts half the star. Tapping the
// value that is set, or Clear, takes the rating away. Back leaves.
class BookRatingActivity final : public Activity {
 public:
  BookRatingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string bookPath, std::string title,
                     std::string author);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void layout();
  void rate(uint8_t halfStars);

  std::string bookPath;
  std::string title;
  std::string author;
  uint8_t halfStars = 0;
  starrating::Row stars;
  gameui::Box clearBox;
  int titleY = 0;
  int labelY = 0;
  int valueY = 0;
  int hintY = 0;
};
