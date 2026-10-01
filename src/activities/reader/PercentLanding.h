#pragma once

#include <cstdint>
#include <functional>
#include <string>

// VARAGH: where a Go to % value lands, shown under the percent in both the
// reader drawer's Go to % pane and the full-screen selector (CrossPoint #3739).
struct PercentLanding {
  std::string chapter;     // chapter title; empty when the book has none
  uint32_t page = 0;       // 1-based page within the chapter; 0 = unknown
  uint32_t pageCount = 0;  // pages in that chapter; 0 = unknown
  bool estimated = false;  // true when the chapter is not laid out yet
};

// Called while the render lock is held (from render()); must not take it again.
using PercentLandingProvider = std::function<bool(float percent, PercentLanding& out)>;
