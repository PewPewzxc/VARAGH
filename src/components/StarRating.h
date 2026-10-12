#pragma once

#include <GfxRenderer.h>

#include <cstdint>

#include "util/BookRatingText.h"

// A row of five large stars for giving a rating by touch: the left half of a
// star counts half of it, the right half all of it. (Small stars in a line of
// text are the star symbols of the menu font, see bookrating::stars.)
namespace starrating {

struct Row {
  int centreX = 0;  // of the middle star
  int centreY = 0;
  int radius = 28;
  int pitch = 76;  // from one star's centre to the next
};

namespace detail {

// A five-pointed star of radius 1000 around the origin, from its top point
// clockwise: outer and inner corners in turn.
constexpr int16_t kUnitX[10] = {0, 247, 951, 399, 588, 0, -588, -399, -951, -247};
constexpr int16_t kUnitY[10] = {-1000, -340, -309, 130, 809, 420, 809, 130, -309, -340};

inline void corners(const int cx, const int cy, const int radius, int* xs, int* ys) {
  for (int i = 0; i < 10; ++i) {
    xs[i] = cx + kUnitX[i] * radius / 1000;
    ys[i] = cy + kUnitY[i] * radius / 1000;
  }
}

}  // namespace detail

// fill: 0 an outline, 1 the left half filled, 2 all of it.
inline void drawStar(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const int fill) {
  int xs[10];
  int ys[10];
  detail::corners(cx, cy, radius, xs, ys);
  if (fill >= 2) {
    renderer.fillPolygon(xs, ys, 10, true);
  } else if (fill == 1) {
    // The corners on and left of the star's middle line.
    const int halfX[6] = {xs[0], xs[9], xs[8], xs[7], xs[6], xs[5]};
    const int halfY[6] = {ys[0], ys[9], ys[8], ys[7], ys[6], ys[5]};
    renderer.fillPolygon(halfX, halfY, 6, true);
  }
  const int line = radius >= 20 ? 2 : 1;
  for (int i = 0; i < 10; ++i) {
    const int next = (i + 1) % 10;
    renderer.drawLine(xs[i], ys[i], xs[next], ys[next], line, true);
  }
}

inline void draw(const GfxRenderer& renderer, const Row& row, const uint8_t halfStars) {
  for (int star = 0; star < bookrating::kStars; ++star) {
    const int fill = halfStars >= (star + 1) * 2 ? 2 : (halfStars == star * 2 + 1 ? 1 : 0);
    drawStar(renderer, row.centreX + (star - 2) * row.pitch, row.centreY, row.radius, fill);
  }
}

// True when the point is on the row; then which star and which half of it.
inline bool hit(const Row& row, const int x, const int y, int& starIndex, bool& rightHalf) {
  constexpr int kSlop = 18;
  if (y < row.centreY - row.radius - kSlop || y > row.centreY + row.radius + kSlop) return false;
  const int left = row.centreX - 2 * row.pitch - row.pitch / 2;
  if (x < left || x >= left + bookrating::kStars * row.pitch) return false;
  starIndex = (x - left) / row.pitch;
  rightHalf = (x - left) % row.pitch >= row.pitch / 2;
  return true;
}

}  // namespace starrating
