#include "ButtonRotation.h"

#include <gtest/gtest.h>

// Turns a model X4 Pro and checks, for every orientation, that the button
// doing "back" (Left, Up) sits before the one doing "forward" (Right, Down)
// as the reader now holds the device: further left on a row, higher in a column.
namespace {

enum Phys { Back, Confirm, Left, Right, Up, Down };

struct Point {
  int x;
  int y;
};

// Portrait, seen from the front, y down: the front row along the bottom, the
// side pair on the right edge.
Point portrait(const Phys b) {
  switch (b) {
    case Back: return {-3, 10};
    case Confirm: return {-1, 10};
    case Left: return {1, 10};
    case Right: return {3, 10};
    case Up: return {5, -1};
    case Down: return {5, 1};
  }
  return {};
}

// Where a button is after the device was turned right `quarters` times.
Point turned(const Phys b, const int quarters) {
  Point p = portrait(b);
  for (int i = 0; i < quarters; ++i) p = {-p.y, p.x};
  return p;
}

// Orientation setting -> clockwise quarter turns of the device.
int quartersFor(const uint8_t orientation) { return orientation == 3 ? 3 : orientation; }

// The physical button that does the job of logical `job`, as MappedInputManager maps it.
Phys physicalFor(const Phys job, const uint8_t orientation) {
  if (job == Up || job == Down) {
    if (!ButtonRotation::swapSide(orientation)) return job;
    return job == Up ? Down : Up;
  }
  switch (ButtonRotation::front(orientation)) {
    case ButtonRotation::Front::Same:
      return job;
    case ButtonRotation::Front::SwapLeftRight:
      return job == Left ? Right : job == Right ? Left : job;
    case ButtonRotation::Front::MirrorRow:
      switch (job) {
        case Back: return Right;
        case Confirm: return Left;
        case Left: return Confirm;
        case Right: return Back;
        default: return job;
      }
  }
  return job;
}

bool before(const Point a, const Point b) {
  // Pairs lie on a row (same y) or a column (same x).
  return a.y == b.y ? a.x < b.x : a.y < b.y;
}

}  // namespace

TEST(ButtonRotation, BackIsAlwaysBeforeForward) {
  for (uint8_t orientation = 0; orientation < 4; ++orientation) {
    const int q = quartersFor(orientation);
    SCOPED_TRACE(static_cast<int>(orientation));
    EXPECT_TRUE(before(turned(physicalFor(Left, orientation), q), turned(physicalFor(Right, orientation), q)));
    EXPECT_TRUE(before(turned(physicalFor(Up, orientation), q), turned(physicalFor(Down, orientation), q)));
  }
}

TEST(ButtonRotation, UpsideDownKeepsPortraitSlotOrder) {
  const int q = quartersFor(2);
  const Phys order[] = {Back, Confirm, Left, Right};
  for (int i = 0; i + 1 < 4; ++i) {
    EXPECT_TRUE(before(turned(physicalFor(order[i], 2), q), turned(physicalFor(order[i + 1], 2), q)));
  }
}

TEST(ButtonRotation, TurningRightSwapsTheSideButtonsAsTheUserExpects) {
  // "When I rotate the screen clockwise one time, the forward button should
  // be the back button": turned right, the side pair trades jobs.
  EXPECT_TRUE(ButtonRotation::swapSide(1));
  EXPECT_FALSE(ButtonRotation::swapSide(3));
  EXPECT_FALSE(ButtonRotation::swapSide(0));
}

TEST(ButtonRotation, FourTurnsComeBackToPortrait) {
  for (int b = Back; b <= Down; ++b) {
    const Point p = turned(static_cast<Phys>(b), 4);
    EXPECT_EQ(p.x, portrait(static_cast<Phys>(b)).x);
    EXPECT_EQ(p.y, portrait(static_cast<Phys>(b)).y);
  }
}
