#pragma once

#include <cstdint>
#include <memory>
#include <string>

class Activity;

enum class FrontlightPanelAction : uint8_t {
  None = 0,
  SyncProgress,
  NearbyPositionSync,
  SendNearbyBook,
  RotateRight,  // VARAGH Control Center: turn the book 90 degrees right
  ToggleGuideDots,     // per-book settings: the reader toggles and saves them
  ToggleFocusReading,
};

enum class FrontlightBookSource : uint8_t { CurrentBook = 0, LastBook, DeviceOnly };

constexpr FrontlightBookSource chooseFrontlightBookSource(const bool activeEpub, const bool currentBookValid,
                                                          const bool lastBookValid) {
  if (activeEpub && currentBookValid) return FrontlightBookSource::CurrentBook;
  if (lastBookValid) return FrontlightBookSource::LastBook;
  return FrontlightBookSource::DeviceOnly;
}

constexpr bool hasFrontlightActiveReaderBook(const bool isReaderActivity, const bool currentBookValid) {
  return isReaderActivity && currentBookValid;
}

constexpr bool shouldShowStickyReaderDetails(const bool hasStickyReaderDetailsPanel, const bool hasFrontlight,
                                             const bool activeReaderBook) {
  return hasStickyReaderDetailsPanel && !hasFrontlight && activeReaderBook;
}

constexpr bool supportsFrontlightDrawer(const bool hasTouchHardware, const bool hasFrontlight,
                                        const bool hasReaderDetailsPanel = false) {
  return hasTouchHardware && (hasFrontlight || hasReaderDetailsPanel);
}

// VARAGH Control Center light slider: the left half covers 0-20 % in 1 %
// steps (where night reading happens), the right half 20-100 %.
// VARAGH: the whole visible slider takes touches, but a knob's centre only
// travels from half a knob in at one end to half a knob in at the other, so a
// finger on the knob's overhang still reaches exactly 0 or 1000.
constexpr int travelPermille(const int rectPermille, const int rectWidth, const int knobWidth) {
  const int travel = rectWidth - knobWidth;
  if (travel <= 0) return 0;
  const int x = rectPermille * (rectWidth - 1) / 1000 - knobWidth / 2;
  return x <= 0 ? 0 : x >= travel ? 1000 : x * 1000 / travel;
}

constexpr uint8_t lightPercentFromPermille(int permille) {
  if (permille < 0) permille = 0;
  if (permille > 1000) permille = 1000;
  return static_cast<uint8_t>(permille <= 500 ? (permille * 20 + 250) / 500 : 20 + ((permille - 500) * 80 + 250) / 500);
}

constexpr int lightPermilleFromPercent(const uint8_t percent) {
  return percent <= 20 ? percent * 500 / 20 : 500 + (percent > 100 ? 80 : percent - 20) * 500 / 80;
}

struct FrontlightPanelBookDetails {
  std::string title;
  std::string author;
  std::string chapter;
  uint32_t chapterPage = 0;
  uint32_t chapterPageCount = 0;
  bool chapterPageCountEstimated = false;
  int progressPercent = 0;
};

struct FrontlightDrawerState {
  int8_t selectedAction = 0;
  bool syncDialogOpen = false;
};

struct FrontlightPanelResult {
  FrontlightPanelAction action = FrontlightPanelAction::None;
  FrontlightDrawerState state{};
  bool activeEpub = false;
  bool ttfRenderingChanged = false;
  std::string bookPath;
};

struct FrontlightPanelContext {
  Activity* sourceActivity = nullptr;
  // An open reader of any supported format. This controls reader header chrome
  // and Home navigation; EPUB-only actions remain gated by activeEpub.
  bool activeReaderBook = false;
  bool activeEpub = false;
  bool showReaderDetails = false;
  bool showReadingStatsAction = true;
  std::string bookTitle;
  std::string bookPath;
  FrontlightPanelBookDetails bookDetails;
  std::unique_ptr<Activity> readingStatsActivity;
  FrontlightDrawerState drawerState{};
};
