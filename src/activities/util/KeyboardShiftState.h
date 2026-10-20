#pragma once

#include <cstdint>

// VARAGH: Shift and Caps Lock on the on-screen keyboard's letter layers.
//
//   one tap            Shift for the next letter only
//   double tap / hold  Caps Lock: every letter upper case until Shift is tapped again
//   tap while on       off
//
// Pure state, so the rules are tested on the host.
class KeyboardShiftState {
 public:
  // Two taps within this time are a double tap.
  static constexpr uint32_t DOUBLE_TAP_MS = 450;

  bool shifted() const { return shifted_; }
  bool capsLock() const { return capsLock_; }

  void tap(const uint32_t nowMs, const bool longPress) {
    if (capsLock_) {
      off();
    } else if (longPress || (shifted_ && nowMs - lastTapMs_ < DOUBLE_TAP_MS)) {
      shifted_ = true;
      capsLock_ = true;
    } else {
      shifted_ = !shifted_;
    }
    lastTapMs_ = nowMs;
  }

  // A letter was typed: one-letter Shift lets go, Caps Lock stays.
  void afterCharacter() {
    if (!capsLock_) shifted_ = false;
  }

  // Layout, symbols or URL panel changed.
  void off() {
    shifted_ = false;
    capsLock_ = false;
  }

 private:
  bool shifted_ = false;
  bool capsLock_ = false;
  uint32_t lastTapMs_ = 0;
};
