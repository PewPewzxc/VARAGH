#pragma once

#include <cstdint>

#if __has_include(<esp_timer.h>) && !defined(SIMULATOR)
#include <esp_timer.h>
#define SPEED_PROFILE_ENABLED 1
#else
#define SPEED_PROFILE_ENABLED 0
#endif

// Where the time of a chapter build goes, for the speed log's "index.<part>"
// lines: each Part accumulates microseconds across every Scope that timed it,
// and Section records the difference between two snapshots. Scopes nest, so
// parts overlap: layout includes the font and hyphenation work it triggers,
// and parse includes layout. A Scope costs two esp_timer reads (~1 us), so
// scopes sit around whole chunks, paragraphs and words, never glyphs.
// On host builds (tests, simulator) the clock reads 0 and nothing is timed.
namespace SpeedProfile {

// Panel comes from the display driver (panelTimeUsSource below), not a Scope:
// the screen refreshes (from any task) during the span, so it overlaps the
// other parts.
enum Part : uint8_t { Unzip, Css, Parse, Layout, Hyphenate, Fonts, Images, Write, Panel, PartCount };

inline uint32_t totalsUs[PartCount] = {};
// Open scopes per part: a part timed again inside itself (a font lookup that
// calls another font lookup) is counted once, by the outermost scope.
inline uint8_t depth[PartCount] = {};

inline uint32_t nowUs() {
#if SPEED_PROFILE_ENABLED
  return static_cast<uint32_t>(esp_timer_get_time());
#else
  return 0;
#endif
}

inline const char* partName(const Part part) {
  switch (part) {
    case Unzip:
      return "index.unzip";
    case Css:
      return "index.css";
    case Parse:
      return "index.parse";
    case Layout:
      return "index.layout";
    case Hyphenate:
      return "index.hyphen";
    case Fonts:
      return "index.fonts";
    case Images:
      return "index.images";
    case Write:
      return "index.write";
    case Panel:
      return "index.panel";
    case PartCount:
      break;
  }
  return "index.?";
}

class Scope {
 public:
  explicit Scope(const Part part) : part_(part), outer_(depth[part]++ == 0), startUs_(outer_ ? nowUs() : 0) {}
  ~Scope() {
    if (outer_) totalsUs[part_] += nowUs() - startUs_;
    --depth[part_];
  }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;

 private:
  Part part_;
  bool outer_;
  uint32_t startUs_;
};

// The Panel part reads the display driver's running total through this hook,
// set at boot, so the display library does not depend on the speed log.
inline uint32_t (*panelTimeUsSource)() = nullptr;

inline uint32_t panelUs() { return panelTimeUsSource ? panelTimeUsSource() : 0; }

struct Snapshot {
  uint32_t us[PartCount] = {};
};

inline Snapshot snapshot() {
  Snapshot s;
  for (int i = 0; i < PartCount; ++i) s.us[i] = totalsUs[i];
  s.us[Panel] = panelUs();
  return s;
}

}  // namespace SpeedProfile
