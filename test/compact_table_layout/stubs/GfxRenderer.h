#pragma once

#include <cstdint>

#include "EpdFontFamily.h"

class GfxRenderer {
 public:
  // VARAGH 1.3.1: CSS font sizes, drop caps (host stub: no sized variants or glyph metrics).
  bool hasFont(int) const { return false; }
  int resolveFontVariant(int fontId, float) const { return fontId; }
  bool getCodepointMetrics(int, uint32_t, EpdFontFamily::Style, int32_t&, int&) const { return false; }
  int drawScaledCodepoint(int, uint32_t, EpdFontFamily::Style, int, int, int, bool = true) const { return 0; }
  int codepointWidth = 1;

  int getTextAdvanceX(int, const char* text, EpdFontFamily::Style) const {
    int width = 0;
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != '\0') {
      const unsigned char first = *cursor++;
      if ((first & 0x80) == 0) {
        width += 1;
      } else if ((first & 0xE0) == 0xC0) {
        cursor += 1;
        width += codepointWidth;
      } else if ((first & 0xF0) == 0xE0) {
        cursor += 2;
        width += codepointWidth;
      } else {
        cursor += 3;
        width += codepointWidth;
      }
    }
    return width;
  }

  int getSpaceWidth(int, EpdFontFamily::Style) const { return 1; }
};
