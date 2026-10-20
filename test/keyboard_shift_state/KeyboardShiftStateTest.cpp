#include "KeyboardShiftState.h"

#include <gtest/gtest.h>

TEST(KeyboardShiftState, OneTapIsShiftForOneLetter) {
  KeyboardShiftState s;
  s.tap(10000, false);
  EXPECT_TRUE(s.shifted());
  EXPECT_FALSE(s.capsLock());
  s.afterCharacter();
  EXPECT_FALSE(s.shifted());
}

TEST(KeyboardShiftState, SecondSlowTapTurnsShiftOff) {
  KeyboardShiftState s;
  s.tap(10000, false);
  s.tap(10000 + KeyboardShiftState::DOUBLE_TAP_MS + 1, false);
  EXPECT_FALSE(s.shifted());
  EXPECT_FALSE(s.capsLock());
}

TEST(KeyboardShiftState, DoubleTapIsCapsLockUntilTappedAgain) {
  KeyboardShiftState s;
  s.tap(10000, false);
  s.tap(10200, false);
  EXPECT_TRUE(s.shifted());
  EXPECT_TRUE(s.capsLock());
  for (int i = 0; i < 5; ++i) {
    s.afterCharacter();
    EXPECT_TRUE(s.shifted());
  }
  s.tap(20000, false);
  EXPECT_FALSE(s.shifted());
  EXPECT_FALSE(s.capsLock());
}

TEST(KeyboardShiftState, HoldIsCapsLock) {
  KeyboardShiftState s;
  s.tap(10000, true);
  EXPECT_TRUE(s.capsLock());
  s.afterCharacter();
  EXPECT_TRUE(s.shifted());
  // A quick tap after Caps Lock turns it off, it does not start a new double tap.
  s.tap(10100, false);
  EXPECT_FALSE(s.capsLock());
  EXPECT_FALSE(s.shifted());
}

TEST(KeyboardShiftState, TapAfterCapsOffStartsAgain) {
  KeyboardShiftState s;
  s.tap(10000, false);
  s.tap(10100, false);  // Caps Lock
  s.tap(10200, false);  // off
  s.tap(10300, false);  // Shift again, not Caps Lock: Shift was off before this tap
  EXPECT_TRUE(s.shifted());
  EXPECT_FALSE(s.capsLock());
}

TEST(KeyboardShiftState, LayoutChangeTurnsEverythingOff) {
  KeyboardShiftState s;
  s.tap(10000, true);
  s.off();
  EXPECT_FALSE(s.shifted());
  EXPECT_FALSE(s.capsLock());
}
