#include "CodenameMark.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "AppVersion.h"

#include <cstdint>

namespace CodenameMark {

namespace {

constexpr int STRIDE = (WIDTH + 7) / 8;

// 1 = ink, MSB = leftmost pixel, in logical (upright) orientation.
constexpr uint8_t BITMAP[HEIGHT * STRIDE] = {
    0x06, 0xDB, 0x00,  // .....##.##.##.##.....
    0x1F, 0xDF, 0xC0,  // ...#######.#######...
    0x3F, 0xFF, 0xE0,  // ..#################..
    0x78, 0xF8, 0xF0,  // .####...#####...####.
    0x76, 0x03, 0x70,  // .###.##.......##.###.
    0xF4, 0x01, 0x78,  // ####.#.........#.####
    0xE0, 0x00, 0x38,  // ###...............###
    0xE6, 0x03, 0x38,  // ###..##.......##..###
    0xF6, 0x03, 0x78,  // ####.##.......##.####
    0xE0, 0x00, 0x38,  // ###...............###
    0xF0, 0x70, 0x78,  // ####.....###.....####
    0x70, 0x20, 0x70,  // .###......#......###.
    0x78, 0x50, 0xF0,  // .####....#.#....####.
    0x3C, 0x21, 0xE0,  // ..####....#....####..
    0x1E, 0x03, 0xC0,  // ...####.......####...
    0x0F, 0xFF, 0x80,  // ....#############....
    0x02, 0xAA, 0x00,  // ......#.#.#.#.#......
};

}  // namespace

std::string versionLabel() {
  std::string version = CROSSINK_VERSION;
  const size_t suffix = version.find_first_of("-+");
  if (suffix != std::string::npos) version.resize(suffix);
  return version + " " + tr(STR_CODENAME);
}

void draw(const GfxRenderer& renderer, const int x, const int y, const bool black) {
  // Per-pixel drawing keeps the mark correct in every screen orientation; at
  // 21x17 it is a few hundred pixels, far below anything measurable.
  for (int row = 0; row < HEIGHT; ++row) {
    const uint8_t* bits = BITMAP + row * STRIDE;
    for (int col = 0; col < WIDTH; ++col) {
      if (bits[col >> 3] & (0x80 >> (col & 7))) {
        renderer.drawPixel(x + col, y + row, black);
      }
    }
  }
}

int labelWidth(const GfxRenderer& renderer, const int fontId, const char* label) {
  return renderer.getTextWidth(fontId, label) + LABEL_GAP + WIDTH;
}

void drawLabel(const GfxRenderer& renderer, const int fontId, const int x, const int y, const char* label,
               const bool black) {
  renderer.drawText(fontId, x, y, label, black);
  const int textHeight = renderer.getTextHeight(fontId);
  const int markY = y + (textHeight - HEIGHT) / 2;
  draw(renderer, x + renderer.getTextWidth(fontId, label) + LABEL_GAP, markY, black);
}

void drawLabelCentered(const GfxRenderer& renderer, const int fontId, const int pageWidth, const int y,
                       const char* label, const bool black) {
  drawLabel(renderer, fontId, (pageWidth - labelWidth(renderer, fontId, label)) / 2, y, label, black);
}

}  // namespace CodenameMark
