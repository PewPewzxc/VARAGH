#pragma once

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
  int getScreenWidth() const { return 800; }
  int getScreenHeight() const { return 480; }
  int getLineHeight(int) const { return 20; }
  int getTextWidth(int, const char* text, EpdFontFamily::Style = EpdFontFamily::REGULAR) const {
    return static_cast<int>(std::string(text).size()) * 8;
  }
  int getSpaceWidth(int, EpdFontFamily::Style = EpdFontFamily::REGULAR) const { return 8; }
  std::vector<std::string> wrappedText(int, const char* text, int, int maxLines, EpdFontFamily::Style) const {
    if (maxLines <= 0 || text == nullptr || text[0] == '\0') return {};
    return {text};
  }
  void displayBuffer() const {}
  void drawLine(int, int, int, int) const { ++drawnLines; }
  void fillPolygon(const int*, const int*, int, bool) const { ++drawnTriangles; }
  int lineCount() const { return drawnLines; }
  int triangleCount() const { return drawnTriangles; }

 private:
  mutable int drawnLines = 0;
  mutable int drawnTriangles = 0;
};
