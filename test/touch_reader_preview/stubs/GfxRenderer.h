#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EpdFontFamily.h"

class GfxRenderer {
 public:
  // VARAGH 1.3.1: CSS font sizes, drop caps (host stub: no sized variants or glyph metrics).
  bool hasFont(int) const { return false; }
  int resolveFontVariant(int fontId, float) const { return fontId; }
  bool getCodepointMetrics(int, uint32_t, EpdFontFamily::Style, int32_t&, int&) const { return false; }
  int drawScaledCodepoint(int, uint32_t, EpdFontFamily::Style, int, int, int, bool = true) const { return 0; }
  struct DrawCall {
    std::string text;
    int x = 0;
    int y = 0;
    EpdFontFamily::Style style = EpdFontFamily::REGULAR;
  };

  mutable std::vector<DrawCall> drawCalls;

  bool isFontCacheScanning() const { return false; }

  int getLineHeight(int) const { return 10; }
  int getTextHeight(int) const { return 10; }
  int getSpaceWidth(int, EpdFontFamily::Style) const { return 1; }
  int getSpaceAdvance(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 1; }
  int getKerning(int, uint32_t, uint32_t, EpdFontFamily::Style) const { return 0; }

  int getTextAdvanceX(int fontId, const char* text, EpdFontFamily::Style) const {
    return static_cast<int>(std::string(text).size()) * (fontId == 2 ? 2 : 1);
  }

  int getTextAdvanceX(int fontId, const char* text, EpdFontFamily::Style style, uint32_t) const {
    return getTextAdvanceX(fontId, text, style);
  }

  void drawText(int, int x, int y, const char* text, bool, EpdFontFamily::Style style) const {
    drawCalls.push_back({text, x, y, style});
  }
};
