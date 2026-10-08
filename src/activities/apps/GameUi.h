#pragma once

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "AppArt.h"
#include "GameGlyphs.h"
#include "MappedInputManager.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "fontIds.h"

// Drawing, touch and storage helpers shared by the built-in apps. The panel is
// one bit deep, so "grey" here is always a pixel pattern.
namespace gameui {

constexpr char kSaveDir[] = "/.crosspoint/games";

// The spacing every app screen shares, so they line up with one another:
// - kHeaderGap between the header and the first thing below it, and between
//   the battery row and a button in the header's corner;
// - kSectionGap between the stacked parts of a screen (board, keys, buttons);
// - kBottomMargin between the lowest control and the foot of the screen.
constexpr int kHeaderGap = 8;
constexpr int kSectionGap = 12;
constexpr int kBottomMargin = 20;
// A button in the header is as tall as the smallest comfortable touch target.
constexpr int kCornerHeight = 44;
// The back arrow and the title sit this far below their usual place, which
// centres them (and the corner button) in the room under the battery row.
constexpr int kTitleOffset = 16;

struct Box {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool contains(const int px, const int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  // The same box grown by `margin` on every side, for forgiving touch targets.
  Box grown(const int margin) const { return Box{x - margin, y - margin, w + 2 * margin, h + 2 * margin}; }
};

// Greys out what is already drawn in the box by clearing every other pixel.
inline void fade(const GfxRenderer& renderer, const Box& box) {
  for (int y = box.y; y < box.y + box.h; ++y) {
    for (int x = box.x + ((box.x + y) & 1); x < box.x + box.w; x += 2) renderer.drawPixel(x, y, false);
  }
}

// A light tint of sparse dots that leaves text on top fully legible.
inline void tint(const GfxRenderer& renderer, const Box& box) {
  for (int y = box.y; y < box.y + box.h; ++y) {
    if (y & 1) continue;
    for (int x = box.x; x < box.x + box.w; ++x) {
      if ((x & 3) == ((y & 2) ? 2 : 0)) renderer.drawPixel(x, y, true);
    }
  }
}

// Top of a text line that centres capital letters and digits of the font in
// the box. One reference glyph keeps every label on the same baseline.
inline int centredTextY(const GfxRenderer& renderer, const int fontId, const Box& box) {
  const auto bounds = renderer.getTextVerticalBounds(fontId, "H");
  return box.y + (box.h - (bounds.bottom - bounds.top)) / 2 - bounds.top;
}

inline void centredText(const GfxRenderer& renderer, const int fontId, const Box& box, const char* text,
                        const bool black = true, const EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  const int x = box.x + (box.w - renderer.getTextWidth(fontId, text, style)) / 2;
  renderer.drawText(fontId, x, centredTextY(renderer, fontId, box), text, black, style);
}

// A bold line in the largest of the UI fonts that fits the box; translations
// can be half as long again as the English they stand for.
inline void fittedText(const GfxRenderer& renderer, const Box& box, const char* text, const bool black = true) {
  int fontId = UI_12_FONT_ID;
  if (renderer.getTextWidth(fontId, text, EpdFontFamily::BOLD) > box.w) fontId = UI_10_FONT_ID;
  if (renderer.getTextWidth(fontId, text, EpdFontFamily::BOLD) > box.w) fontId = SMALL_FONT_ID;
  centredText(renderer, fontId, box, text, black, EpdFontFamily::BOLD);
}

// Black text with a white rim, for labels that sit on a pixel pattern.
inline void rimmedText(const GfxRenderer& renderer, const int fontId, const Box& box, const char* text,
                       const EpdFontFamily::Style style = EpdFontFamily::BOLD) {
  const int x = box.x + (box.w - renderer.getTextWidth(fontId, text, style)) / 2;
  const int y = centredTextY(renderer, fontId, box);
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      if (dx != 0 || dy != 0) renderer.drawText(fontId, x + dx, y + dy, text, false, style);
    }
  }
  renderer.drawText(fontId, x, y, text, true, style);
}

// A one-bit bitmap (rows padded to whole bytes, 1 = ink) painted in black or white.
inline void bitsAt(const GfxRenderer& renderer, const uint8_t* data, const int width, const int rows, const int x,
                   const int y, const bool black) {
  const int stride = (width + 7) / 8;
  for (int row = 0; row < rows; ++row) {
    const uint8_t* line = data + row * stride;
    for (int column = 0; column < width; ++column) {
      if (line[column / 8] & (0x80 >> (column % 8))) renderer.drawPixel(x + column, y + row, black);
    }
  }
}

inline void stamp(const GfxRenderer& renderer, const appart::Bitmap& bitmap, const int x, const int y,
                  const bool black = true) {
  bitsAt(renderer, bitmap.bits, bitmap.width, bitmap.height, x, y, black);
}

// A piece that stays readable on any square: its white underlay first, then its ink.
inline void stampOver(const GfxRenderer& renderer, const appart::Bitmap& ink, const appart::Bitmap& underlay,
                      const int x, const int y) {
  stamp(renderer, underlay, x, y, false);
  stamp(renderer, ink, x, y, true);
}

// The large game letters and digits (GameGlyphs.h), which the UI fonts are too
// small for. A glyph is centred in its box by the height of a capital.
inline void glyphAt(const GfxRenderer& renderer, const gameglyphs::Glyph& glyph, const uint8_t* bits, const int rows,
                    const int x, const int y, const bool black) {
  bitsAt(renderer, bits + glyph.offset, glyph.width, rows, x, y, black);
}

enum class Ink : uint8_t {
  Black,   // on a white box
  White,   // on a black box
  Rimmed,  // black with a white rim, on a pixel pattern
};

inline void centredGlyph(const GfxRenderer& renderer, const gameglyphs::Glyph& glyph, const uint8_t* bits,
                         const int rows, const int cap, const Box& box, const Ink ink) {
  const int x = box.x + (box.w - glyph.width) / 2;
  const int y = box.y + (box.h - cap) / 2;
  if (ink == Ink::Rimmed) {
    for (int dy = -2; dy <= 2; dy += 2) {
      for (int dx = -2; dx <= 2; dx += 2) {
        if (dx != 0 || dy != 0) glyphAt(renderer, glyph, bits, rows, x + dx, y + dy, false);
      }
    }
  }
  glyphAt(renderer, glyph, bits, rows, x, y, ink != Ink::White);
}

// letter: 'a'..'z' or 'A'..'Z'.
inline void bigLetter(const GfxRenderer& renderer, const char letter, const Box& box, const Ink ink) {
  const int index = (letter >= 'a' && letter <= 'z') ? letter - 'a' : letter - 'A';
  if (index < 0 || index >= 26) return;
  centredGlyph(renderer, gameglyphs::kLetterGlyphs[index], gameglyphs::kLetterBits, gameglyphs::kLetterRows,
               gameglyphs::kLetterCap, box, ink);
}

// digit: 1..9. Bold for printed clues, regular for the player's own digits.
inline void bigDigit(const GfxRenderer& renderer, const int digit, const bool bold, const Box& box, const Ink ink) {
  if (digit < 1 || digit > 9) return;
  if (bold) {
    centredGlyph(renderer, gameglyphs::kDigitBoldGlyphs[digit - 1], gameglyphs::kDigitBoldBits,
                 gameglyphs::kDigitBoldRows, gameglyphs::kDigitBoldCap, box, ink);
  } else {
    centredGlyph(renderer, gameglyphs::kDigitRegularGlyphs[digit - 1], gameglyphs::kDigitRegularBits,
                 gameglyphs::kDigitRegularRows, gameglyphs::kDigitRegularCap, box, ink);
  }
}

// A label wider than its button (a long translation) is drawn in the small font.
inline void button(const GfxRenderer& renderer, const Box& box, const char* label, const bool filled,
                   const int fontId = UI_10_FONT_ID) {
  if (filled) {
    renderer.fillRoundedRect(box.x, box.y, box.w, box.h, 8, Color::Black);
  } else {
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 8, true);
  }
  const bool fits = renderer.getTextWidth(fontId, label, EpdFontFamily::BOLD) <= box.w - 8;
  centredText(renderer, fits ? fontId : SMALL_FONT_ID, box, label, !filled, EpdFontFamily::BOLD);
}

// A button with a picture above its word.
inline void iconButton(const GfxRenderer& renderer, const Box& box, const appart::Bitmap& icon, const char* label,
                       const bool filled) {
  if (filled) {
    renderer.fillRoundedRect(box.x, box.y, box.w, box.h, 10, Color::Black);
  } else {
    renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 2, 10, true);
  }
  constexpr int labelHeight = 30;
  const int iconY = box.y + (box.h - icon.height - labelHeight - 4) / 2;
  stamp(renderer, icon, box.x + (box.w - icon.width) / 2, iconY, !filled);
  fittedText(renderer, {box.x + 3, iconY + icon.height + 4, box.w - 6, labelHeight}, label, !filled);
}

// Diagonal hatching, the dark squares of a printed chess diagram.
inline void hatch(const GfxRenderer& renderer, const Box& box) {
  for (int y = box.y; y < box.y + box.h; ++y) {
    for (int x = box.x + ((4 - ((box.x + y) & 3)) & 3); x < box.x + box.w; x += 4) renderer.drawPixel(x, y, true);
  }
}

inline void disc(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const bool black) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
    renderer.fillRect(cx - half, cy + dy, 2 * half + 1, 1, black);
  }
}

inline void ring(const GfxRenderer& renderer, const int cx, const int cy, const int radius, const int thickness,
                 const bool black) {
  const int inner = radius - thickness;
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
    if (dy <= -inner || dy >= inner) {
      renderer.fillRect(cx - half, cy + dy, 2 * half + 1, 1, black);
      continue;
    }
    const int hole = static_cast<int>(sqrtf(static_cast<float>(inner * inner - dy * dy)));
    renderer.fillRect(cx - half, cy + dy, half - hole, 1, black);
    renderer.fillRect(cx + hole + 1, cy + dy, half - hole, 1, black);
  }
}

// Corner brackets on a board square: the mark of the move just played.
inline void brackets(const GfxRenderer& renderer, const Box& box) {
  constexpr int length = 12;
  constexpr int thick = 3;
  const int right = box.x + box.w;
  const int bottom = box.y + box.h;
  renderer.fillRect(box.x + 2, box.y + 2, length, thick, true);
  renderer.fillRect(box.x + 2, box.y + 2, thick, length, true);
  renderer.fillRect(right - 2 - length, box.y + 2, length, thick, true);
  renderer.fillRect(right - 2 - thick, box.y + 2, thick, length, true);
  renderer.fillRect(box.x + 2, bottom - 2 - thick, length, thick, true);
  renderer.fillRect(box.x + 2, bottom - 2 - length, thick, length, true);
  renderer.fillRect(right - 2 - length, bottom - 2 - thick, length, thick, true);
  renderer.fillRect(right - 2 - thick, bottom - 2 - length, thick, length, true);
}

// One of several options in a panel: filled when it is the one chosen, greyed
// when the row does not apply.
inline void choice(const GfxRenderer& renderer, const Box& box, const char* label, const bool chosen,
                   const bool enabled = true) {
  button(renderer, box, label, chosen && enabled);
  if (!enabled) fade(renderer, box);
}

// A white panel with a frame, laid over whatever is already drawn.
inline void panelFrame(const GfxRenderer& renderer, const Box& box) {
  renderer.fillRoundedRect(box.x - 3, box.y - 3, box.w + 6, box.h + 6, 14, Color::White);
  renderer.drawRoundedRect(box.x, box.y, box.w, box.h, 3, 12, true);
}

// --- the header of an app screen ---------------------------------------------------------------
// Where the back arrow and the title sit inside the touch header.
inline int headerTitleCentreY(const GfxRenderer& renderer, const MappedInputManager& input) {
  const Rect header = TouchHeaderBackButton::headerRect(renderer, input);
  const Rect icon = TouchHeaderBackButton::layout(header).iconRect;
  const int iconBottom = icon.y + (icon.height + TouchHeaderBackButton::ICON_SIZE) / 2;
  const int offset = std::clamp(kTitleOffset, 0, std::max(0, header.y + header.height - iconBottom));
  return icon.y + offset + icon.height / 2;
}

inline int headerBottom(const GfxRenderer& renderer, const MappedInputManager& input) {
  const Rect header = TouchHeaderBackButton::headerRect(renderer, input);
  return header.y + header.height;
}

// Where an app's own content starts and where its lowest control ends.
inline int contentTop(const GfxRenderer& renderer, const MappedInputManager& input) {
  return headerBottom(renderer, input) + kHeaderGap;
}

inline int contentBottom(const GfxRenderer& renderer) { return renderer.getScreenHeight() - kBottomMargin; }

// A button in the top right corner of the header, level with the title and in
// the title's letter size. Its right edge lines up with the battery above it,
// and it keeps kHeaderGap clear of the battery row.
inline Box headerCornerBox(const GfxRenderer& renderer, const MappedInputManager& input, const char* label) {
  const int width = renderer.getTextWidth(UI_12_FONT_ID, label, EpdFontFamily::BOLD) + 48;
  return Box{renderer.getScreenWidth() - StatusBarMetrics::sideInset - width,
             headerTitleCentreY(renderer, input) - kCornerHeight / 2, width, kCornerHeight};
}

inline void cornerButton(const GfxRenderer& renderer, const Box& box, const char* label) {
  button(renderer, box, label, false, UI_12_FONT_ID);
}

// The usual header - back arrow, title, clock and battery - without the rule
// under it. `centred` replaces the title with a line in the middle; `rightEdge`
// is where that line has to stop (the left edge of a corner button).
inline void drawHeader(GfxRenderer& renderer, const MappedInputManager& input, const char* title,
                       const char* centred = nullptr, const int rightEdge = 0) {
  const Rect header = TouchHeaderBackButton::headerRect(renderer, input);
  if (input.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, header, centred ? "" : title, true, 0, nullptr, kTitleOffset);
  } else {
    GUI.drawHeader(renderer, header, centred ? "" : title);
  }
  renderer.fillRect(0, header.y + header.height - 4, renderer.getScreenWidth(), 5, false);
  if (centred) {
    const int left = TouchHeaderBackButton::layout(header).titleX;
    const int right = rightEdge > 0 ? rightEdge - 8 : renderer.getScreenWidth() - left;
    const int centreY = headerTitleCentreY(renderer, input);
    const bool fits = renderer.getTextWidth(UI_12_FONT_ID, centred, EpdFontFamily::BOLD) <= right - left;
    centredText(renderer, fits ? UI_12_FONT_ID : UI_10_FONT_ID, {left, centreY - 20, right - left, 40}, centred, true,
                EpdFontFamily::BOLD);
  }
}

// A button that acts only when it is held: it fills from the left while the
// finger rests on it, in a few steps because the panel repaints slowly.
struct HoldButton {
  static constexpr unsigned long kHoldMs = 1200;
  static constexpr uint8_t kSteps = 4;

  Box box;
  uint8_t step = 0;  // 0 = not held, kSteps = full

  // Call on every loop. Returns true once, when the hold is complete; sets
  // `repaint` when the fill has to be drawn again.
  bool update(MappedInputManager& input, bool& repaint) {
    int x = 0;
    int y = 0;
    unsigned long heldMs = 0;
    uint8_t now = 0;
    if (box.w > 0 && input.isScreenTouchTapCandidate(x, y, heldMs) && box.grown(8).contains(x, y)) {
      if (heldMs >= kHoldMs) {
        // The rest of this touch is not a tap on whatever comes next.
        input.suppressCurrentTouchContact();
        step = 0;
        repaint = true;
        return true;
      }
      now = heldMs >= 100 ? static_cast<uint8_t>(1 + (heldMs - 100) * (kSteps - 1) / (kHoldMs - 100)) : 0;
    }
    if (now != step) {
      step = now;
      repaint = true;
    }
    return false;
  }

  void draw(const GfxRenderer& renderer, const char* label) const {
    cornerButton(renderer, box, label);
    if (step > 0) renderer.invertRect(box.x + 4, box.y + 4, (box.w - 8) * step / kSteps, box.h - 8);
  }
};

// The clock digits (AppArt.h): '0'..'9' and ':'.
struct DigitSet {
  const appart::Glyph* glyphs;
  const uint8_t* bits;
  int rows;
};

inline int digitIndex(const char c) { return c >= '0' && c <= '9' ? c - '0' : (c == ':' ? 10 : -1); }

inline int digitTextWidth(const DigitSet& set, const char* text, const int gap) {
  int width = 0;
  for (const char* c = text; *c; ++c) {
    const int index = digitIndex(*c);
    if (index >= 0) width += set.glyphs[index].width + gap;
  }
  return width > 0 ? width - gap : 0;
}

// Draws the digits centred on centreX with their tops at y.
inline void digitText(const GfxRenderer& renderer, const DigitSet& set, const char* text, const int centreX,
                      const int y, const int gap, const bool black = true) {
  int x = centreX - digitTextWidth(set, text, gap) / 2;
  for (const char* c = text; *c; ++c) {
    const int index = digitIndex(*c);
    if (index < 0) continue;
    bitsAt(renderer, set.bits + set.glyphs[index].offset, set.glyphs[index].width, set.rows, x, y, black);
    x += set.glyphs[index].width + gap;
  }
}

inline bool readSave(const char* path, uint8_t* out, const size_t size) {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("GAME", path, file)) return false;
  const int got = file.read(out, size);
  file.close();
  return got == static_cast<int>(size);
}

inline bool writeSave(const char* path, const uint8_t* data, const size_t size) {
  Storage.mkdir(kSaveDir);
  HalFile file;
  if (!Storage.openFileForWrite("GAME", path, file)) return false;
  const bool ok = file.write(data, size) == size;
  file.close();
  return ok;
}

}  // namespace gameui
