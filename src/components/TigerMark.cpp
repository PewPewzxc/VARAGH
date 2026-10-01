#include "TigerMark.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "AppVersion.h"

#include <cstdint>

namespace TigerMark {

namespace {

constexpr int STRIDE = (WIDTH + 7) / 8;

// 1 = ink, MSB = leftmost pixel, in logical (upright) orientation.
constexpr uint8_t BITMAP[HEIGHT * STRIDE] = {
    0x38, 0x00, 0xE0,  // ..###...........###..
    0x7C, 0x01, 0xF0,  // .#####.........#####.
    0x6D, 0xFD, 0xB0,  // .##.##.#######.##.##.
    0x66, 0x73, 0x30,  // .##..##..###..##..##.
    0x34, 0xF9, 0x60,  // ..##.#..#####..#.##..
    0x11, 0x24, 0x40,  // ...#...#..#..#...#...
    0x20, 0x00, 0x20,  // ..#...............#..
    0x23, 0x06, 0x20,  // ..#...##.....##...#..
    0x43, 0x06, 0x10,  // .#....##.....##....#.
    0xF0, 0x00, 0x78,  // ####.............####
    0x40, 0x70, 0x10,  // .#.......###.......#.
    0xF0, 0x20, 0x78,  // ####......#......####
    0x40, 0x00, 0x10,  // .#.................#.
    0x38, 0x20, 0xE0,  // ..###.....#.....###..
    0x20, 0xD8, 0x20,  // ..#.....##.##.....#..
    0x18, 0x00, 0xC0,  // ...##...........##...
    0x07, 0xFF, 0x00,  // .....###########.....
};

}  // namespace

std::string versionLabel() {
  std::string version = CROSSINK_VERSION;
  const size_t suffix = version.find_first_of("-+");
  if (suffix != std::string::npos) version.resize(suffix);
  return version + " " + tr(STR_TIGER_CODENAME);
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

}  // namespace TigerMark
