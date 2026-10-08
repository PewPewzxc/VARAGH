#!/usr/bin/env python3
"""
Draw the artwork of the built-in apps and write it as
src/activities/apps/AppArt.h:

- the button icons of the board and number games (undo, erase, notes, fill,
  hint, flip), drawn here as simple line art;
- the chess pieces, from Noto Sans Symbols 2 (already in the repository, SIL
  Open Font License): one bitmap per piece and colour, plus one "underlay" per
  piece kind - its filled outline grown by two pixels - that is painted white
  first so a piece stays readable on a hatched square;
- the digits of the clock, from Lexend Deca (SIL Open Font License), in three
  sizes.

Everything is a one-bit bitmap, rows padded to whole bytes, most significant
bit first, 1 = ink.

Usage:
    python scripts/generate_app_art.py        (needs Pillow)
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO = Path(__file__).resolve().parent.parent
SOURCES = REPO / "lib" / "EpdFont" / "builtinFonts" / "source"
SYMBOLS = SOURCES / "NotoSymbols2" / "NotoSansSymbols2-Regular.ttf"
LEXEND_BOLD = SOURCES / "LexendDeca" / "LexendDeca-Bold.ttf"
OUTPUT = REPO / "src" / "activities" / "apps" / "AppArt.h"

ICON = 36  # the grid the icons are drawn on
ICON_OUT = 44  # their size on the screen
SCALE = 4  # icons are drawn four times larger and reduced, for even strokes
PIECE = 56
PIECE_FONT = 50
SMALL_PIECE = 26
SMALL_PIECE_FONT = 25
MENU_KNIGHT = 84
MENU_KNIGHT_FONT = 78
# White king, queen, rook, bishop, knight, pawn; the black set follows at +6.
WHITE_PIECES = "♔♕♖♗♘♙"
PIECE_NAMES = ["King", "Queen", "Rook", "Bishop", "Knight", "Pawn"]
CLOCK_SETS = [("ClockLarge", 106), ("ClockMedium", 74), ("ClockSmall", 38)]
CLOCK_CHARACTERS = "0123456789:"


# --- icons --------------------------------------------------------------------------------------
def rot(points, cx, cy, degrees):
    a = math.radians(degrees)
    ca, sa = math.cos(a), math.sin(a)
    return [(cx + x * ca - y * sa, cy + x * sa + y * ca) for x, y in points]


def scaled(points):
    return [(x * SCALE, y * SCALE) for x, y in points]


def line(d, points, width=3):
    d.line(scaled(points), fill=255, width=width * SCALE, joint="curve")
    radius = width * SCALE / 2
    for x, y in scaled(points):
        d.ellipse([x - radius, y - radius, x + radius, y + radius], fill=255)


def icon_undo(d, cx, cy):
    line(d, [(cx - 7, cy - 12), (cx - 14, cy - 5), (cx - 7, cy + 2)])
    line(d, [(cx - 14, cy - 5), (cx + 5, cy - 5)])
    d.arc(scaled([(cx - 5, cy - 5)]) + scaled([(cx + 15, cy + 13)]), start=-90, end=90, fill=255, width=3 * SCALE)
    line(d, [(cx + 5, cy + 13), (cx - 8, cy + 13)])


def icon_erase(d, cx, cy):
    body = rot([(-15, -8), (15, -8), (15, 8), (-15, 8)], cx, cy - 3, -35)
    line(d, body + [body[0]])
    d.polygon(scaled(rot([(-15, -8), (-3, -8), (-3, 8), (-15, 8)], cx, cy - 3, -35)), fill=255)
    line(d, [(cx - 16, cy + 14), (cx + 16, cy + 14)])


def icon_notes(d, cx, cy):
    body = rot([(-8, -5), (16, -5), (16, 5), (-8, 5)], cx + 2, cy - 2, -45)
    line(d, body + [body[0]])
    d.polygon(scaled(rot([(-8, -6), (-19, 0), (-8, 6)], cx + 2, cy - 2, -45)), fill=255)
    line(d, rot([(10, -5), (10, 5)], cx + 2, cy - 2, -45), 2)


def icon_fill(d, cx, cy):
    line(d, [(cx - 14, cy - 14), (cx + 14, cy - 14), (cx + 14, cy + 14), (cx - 14, cy + 14), (cx - 14, cy - 14)])
    for r in range(3):
        for c in range(3):
            x, y = (cx - 8 + c * 8) * SCALE, (cy - 8 + r * 8) * SCALE
            d.ellipse([x - 7, y - 7, x + 7, y + 7], fill=255)


def icon_hint(d, cx, cy):
    box = scaled([(cx - 10, cy - 15)]) + scaled([(cx + 10, cy + 5)])
    d.ellipse(box, outline=255, width=3 * SCALE)
    line(d, [(cx - 4, cy + 8), (cx + 4, cy + 8)])
    line(d, [(cx - 3, cy + 13), (cx + 3, cy + 13)])
    line(d, [(cx - 17, cy - 5), (cx - 15, cy - 5)], 2)
    line(d, [(cx + 15, cy - 5), (cx + 17, cy - 5)], 2)
    line(d, [(cx - 14, cy - 17), (cx - 12, cy - 15)], 2)
    line(d, [(cx + 14, cy - 17), (cx + 12, cy - 15)], 2)


def icon_flip(d, cx, cy):
    line(d, [(cx - 8, cy + 13), (cx - 8, cy - 13)])
    line(d, [(cx - 15, cy - 6), (cx - 8, cy - 13), (cx - 1, cy - 6)])
    line(d, [(cx + 8, cy - 13), (cx + 8, cy + 13)])
    line(d, [(cx + 1, cy + 6), (cx + 8, cy + 13), (cx + 15, cy + 6)])


ICONS = [("Undo", icon_undo), ("Erase", icon_erase), ("Notes", icon_notes), ("Fill", icon_fill),
         ("Hint", icon_hint), ("Flip", icon_flip)]


def render_icon(draw_icon):
    big = Image.new("L", (ICON * SCALE, ICON * SCALE), 0)
    draw_icon(ImageDraw.Draw(big), ICON // 2, ICON // 2)
    small = big.resize((ICON_OUT, ICON_OUT), Image.LANCZOS)
    pixels = small.load()
    return [[pixels[x, y] >= 110 for x in range(ICON_OUT)] for y in range(ICON_OUT)]


# --- chess pieces -------------------------------------------------------------------------------
def render_symbol(character, cell, size):
    font = ImageFont.truetype(str(SYMBOLS), size)
    image = Image.new("L", (cell, cell), 0)
    d = ImageDraw.Draw(image)
    d.fontmode = "1"
    left, top, right, bottom = d.textbbox((0, 0), character, font=font)
    d.text(((cell - (right - left)) // 2 - left, (cell - (bottom - top)) // 2 - top), character, font=font, fill=255)
    return image.point(lambda v: 255 if v > 127 else 0)


def underlay_of(inks, cell):
    """The union of the given glyphs with their holes filled, grown by two pixels."""
    union = Image.new("L", (cell, cell), 0)
    for ink in inks:
        padded = Image.new("L", (cell + 2, cell + 2), 0)
        padded.paste(ink, (1, 1))
        ImageDraw.floodfill(padded, (0, 0), 128)
        filled = padded.point(lambda v: 0 if v == 128 else 255).crop((1, 1, cell + 1, cell + 1))
        union = Image.composite(filled, union, filled)
    return union.filter(ImageFilter.MaxFilter(5))


def rows_of(image):
    pixels = image.load()
    return [[pixels[x, y] > 127 for x in range(image.size[0])] for y in range(image.size[1])]


# --- clock digits -------------------------------------------------------------------------------
def font_for_height(path, reference, height):
    size = height
    best = ImageFont.truetype(str(path), size)
    while True:
        font = ImageFont.truetype(str(path), size)
        top, bottom = font.getbbox(reference)[1], font.getbbox(reference)[3]
        if bottom - top > height:
            return best
        best = font
        size += 1


def render_digits(font):
    top, bottom = font.getbbox("8")[1], font.getbbox("8")[3]
    rows = bottom - top
    glyphs = []
    for character in CLOCK_CHARACTERS:
        left, _, right, _ = font.getbbox(character)
        width = right - left
        image = Image.new("L", (width, rows), 0)
        ImageDraw.Draw(image).text((-left, -top), character, font=font, fill=255)
        pixels = image.load()
        glyphs.append((width, [[pixels[x, y] >= 128 for x in range(width)] for y in range(rows)]))
    return rows, glyphs


# --- output -------------------------------------------------------------------------------------
def pack(rows):
    data = []
    for row in rows:
        for start in range(0, len(row), 8):
            byte = 0
            for bit in range(8):
                byte = (byte << 1) | (1 if start + bit < len(row) and row[start + bit] else 0)
            data.append(byte)
    return data


def byte_lines(data):
    return "\n".join("    " + ", ".join(f"0x{b:02X}" for b in data[i : i + 20]) + "," for i in range(0, len(data), 20))


def bitmap(parts, name, rows):
    data = pack(rows)
    parts.append(f"inline constexpr uint8_t k{name}Bits[] = {{\n{byte_lines(data)}\n}};")
    parts.append(f"inline constexpr Bitmap k{name} = {{{len(rows[0])}, {len(rows)}, k{name}Bits}};\n")
    return len(data)


def main() -> None:
    total = 0
    parts = [
        "#pragma once\n",
        "#include <cstdint>\n",
        "// Generated by scripts/generate_app_art.py. Do not edit.",
        "//",
        "// Chess pieces from Noto Sans Symbols 2, clock digits from Lexend Deca (both SIL Open",
        "// Font License); the button icons are drawn by the script.",
        "//",
        "// One-bit bitmaps: rows padded to whole bytes, most significant bit first, 1 = ink.\n",
        "namespace appart {\n",
        "struct Bitmap {\n  uint8_t width;\n  uint8_t height;\n  const uint8_t* bits;\n};\n",
        "struct Glyph {\n  uint16_t offset;  // into the set's bitmap bytes\n  uint8_t width;\n};\n",
        "// Button icons.",
    ]
    for name, draw_icon in ICONS:
        total += bitmap(parts, f"Icon{name}", render_icon(draw_icon))

    parts.append("// Chess pieces for a board square: [colour][kind], kinds in the order king, queen, rook,")
    parts.append("// bishop, knight, pawn. The underlay of a kind is painted white before either colour.")
    for cell, size, prefix in ((PIECE, PIECE_FONT, "Piece"), (SMALL_PIECE, SMALL_PIECE_FONT, "SmallPiece")):
        for index, name in enumerate(PIECE_NAMES):
            white = render_symbol(WHITE_PIECES[index], cell, size)
            black = render_symbol(chr(ord(WHITE_PIECES[index]) + 6), cell, size)
            total += bitmap(parts, f"{prefix}White{name}", rows_of(white))
            total += bitmap(parts, f"{prefix}Black{name}", rows_of(black))
            total += bitmap(parts, f"{prefix}Under{name}", rows_of(underlay_of([white, black], cell)))
        for colour in ("White", "Black", "Under"):
            parts.append(
                f"inline constexpr const Bitmap* k{prefix}{colour}[6] = {{"
                + ", ".join(f"&k{prefix}{colour}{name}" for name in PIECE_NAMES)
                + "};"
            )
        parts.append("")

    knight = render_symbol("♞", MENU_KNIGHT, MENU_KNIGHT_FONT)
    parts.append("// The knight of the Chess tile in the Apps menu.")
    total += bitmap(parts, "MenuKnight", rows_of(knight))
    total += bitmap(parts, "MenuKnightUnder", rows_of(underlay_of([knight], MENU_KNIGHT)))

    parts.append("// Clock digits: glyphs 0-9, then the colon. Every glyph of a set is k<Set>Rows high.")
    for name, height in CLOCK_SETS:
        font = font_for_height(LEXEND_BOLD, "8", height)
        rows, glyphs = render_digits(font)
        data = []
        table = []
        for width, glyph_rows in glyphs:
            table.append((len(data), width))
            data.extend(pack(glyph_rows))
        parts.append(f"constexpr int k{name}Rows = {rows};")
        parts.append(f"inline constexpr uint8_t k{name}Bits[] = {{\n{byte_lines(data)}\n}};")
        parts.append(
            f"inline constexpr Glyph k{name}Glyphs[{len(table)}] = {{\n    "
            + ", ".join(f"{{{offset}, {width}}}" for offset, width in table)
            + ",\n};\n"
        )
        total += len(data)
        print(f"{name}: {rows} rows, {len(data)} bytes, widest {max(w for _, w in table)} px")
    parts.append("}  // namespace appart")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(parts) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {OUTPUT.relative_to(REPO)}: {total} bytes of bitmaps")


if __name__ == "__main__":
    main()
