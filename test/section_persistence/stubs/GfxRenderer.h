#pragma once

#include <cstdint>

class FontCacheManager;

class GfxRenderer {
 public:
  // VARAGH 1.3.1: CSS font sizes, drop caps (host stub: no sized variants or glyph metrics).
  bool hasFont(int) const { return false; }
  int resolveFontVariant(int fontId, float) const { return fontId; }
  bool getCodepointMetrics(int, uint32_t, int, int32_t&, int&) const { return false; }
  int drawScaledCodepoint(int, uint32_t, int, int, int, int, bool = true) const { return 0; }
  FontCacheManager* getFontCacheManager() const { return nullptr; }
  bool isSdCardFont(int) const { return false; }
  bool releaseSdCardFontForLowMemory(int, bool = false) { return false; }
};
