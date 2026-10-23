#pragma once

#include <EpdFontFamily.h>

#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace BidiUtils {
enum class BidiBaseDir : signed char { AUTO = -1, LTR = 0, RTL = 1 };
}

class GfxRenderer {
 public:
  // VARAGH 1.3.1: CSS font sizes, drop caps (host stub: no sized variants or glyph metrics).
  bool hasFont(int) const { return false; }
  int resolveFontVariant(int fontId, float) const { return fontId; }
  bool getCodepointMetrics(int, uint32_t, EpdFontFamily::Style, int32_t&, int&) const { return false; }
  int drawScaledCodepoint(int, uint32_t, EpdFontFamily::Style, int, int, int, bool = true) const { return 0; }
  int textAdvancePerChar = 0;
  int getFontAscenderSize(int) const { return 12; }
  int getLineHeight(int) const { return 16; }
  int getTextWidth(int, const char*, EpdFontFamily::Style = EpdFontFamily::REGULAR) const { return 0; }
  int getTextAdvanceX(int, const char* text, EpdFontFamily::Style, uint32_t = 0) const {
    return static_cast<int>(std::strlen(text)) * textAdvancePerChar;
  }
  int getSpaceWidth(int, EpdFontFamily::Style) const { return 0; }
  int getKerning(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }
  int getSpaceAdvance(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }
  bool isSdCardFont(int) const { return false; }
  bool releaseSdCardFontForLowMemory(int, bool = false) { return false; }
  bool ensureSdCardFontReady(int, const uint32_t*, size_t, bool, bool, uint8_t) const { return true; }
  bool ensureSdCardFontReady(int, const std::deque<std::string>&, bool, uint8_t) const { return true; }
  bool ensureSdCardFontReady(int, const char*, uint8_t) const { return true; }
  std::vector<std::string> wrappedText(int, const char*, int, int,
                                       EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return {};
  }
};
