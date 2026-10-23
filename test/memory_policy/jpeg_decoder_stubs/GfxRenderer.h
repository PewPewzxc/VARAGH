#pragma once

#include <cstdint>
#include <vector>

class GfxRenderer {
 public:
  // VARAGH 1.3.1: CSS font sizes, drop caps (host stub: no sized variants or glyph metrics).
  bool hasFont(int) const { return false; }
  int resolveFontVariant(int fontId, float) const { return fontId; }
  bool getCodepointMetrics(int, uint32_t, int, int32_t&, int&) const { return false; }
  int drawScaledCodepoint(int, uint32_t, int, int, int, int, bool = true) const { return 0; }
  enum RenderMode { BW, GRAYSCALE_MSB, GRAYSCALE_LSB };
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };

  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  int getDisplayWidth() const { return width; }
  int getDisplayHeight() const { return height; }
  uint16_t getDisplayWidthBytes() const { return static_cast<uint16_t>((width + 7) / 8); }
  uint8_t* getWriteTarget() { return framebuffer.data(); }
  int getWriteOriginY() const { return 0; }
  int getWriteRows() const { return height; }
  RenderMode getRenderMode() const { return BW; }
  Orientation getOrientation() const { return LandscapeCounterClockwise; }

 private:
  static constexpr int width = 800;
  static constexpr int height = 480;
  std::vector<uint8_t> framebuffer = std::vector<uint8_t>((width + 7) / 8 * height, 0xff);
};
