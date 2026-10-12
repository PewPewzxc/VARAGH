#!/usr/bin/env python3
"""
Build the VARAGH logo files from the master drawing, src/images/varagh-logo.png
(greyscale: white paper, black ink, mid grey where the turning page casts its
shadow):

  src/images/BootLogo192.h   boot and sleep screens: 192x192, one bit per pixel,
                             1 = white, stored turned a quarter left for the
                             portrait draw path. The shadow becomes a dot screen.
  web/assets/logo.png        web file manager: black on transparent, the shadow
                             as a light tint (the dark theme inverts it).

Usage:
    python scripts/generate_logo.py
Needs Pillow. Run scripts/build_web.py afterwards (the firmware build does).
"""

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
MASTER = ROOT / "src" / "images" / "varagh-logo.png"
BOOT_HEADER = ROOT / "src" / "images" / "BootLogo192.h"
WEB_LOGO = ROOT / "web" / "assets" / "logo.png"

BOOT_SIZE = 192
WEB_SIZE = 128
# Master levels: below INK_BELOW is ink, from there up to PAPER_FROM is shadow.
INK_BELOW = 88
PAPER_FROM = 216
# Share of a boot-logo pixel (of 255) that ink must cover. A little under half
# keeps the straight edges of the book, which fall close to half, from wobbling.
INK_COVER = 90


def boot_rows(master):
    """Rows of booleans (True = ink) for the boot logo."""
    ink = master.point(lambda v: 255 if v < INK_BELOW else 0).resize((BOOT_SIZE, BOOT_SIZE), Image.BOX)
    # Counted apart from ink, so the soft rim of a stroke is not taken for shadow.
    shade = master.point(lambda v: 255 if INK_BELOW <= v < PAPER_FROM else 0).resize((BOOT_SIZE, BOOT_SIZE), Image.BOX)
    ink_px, shade_px = ink.load(), shade.load()
    rows = []
    for y in range(BOOT_SIZE):
        row = []
        for x in range(BOOT_SIZE):
            if ink_px[x, y] >= INK_COVER:
                row.append(True)
            elif shade_px[x, y] >= 128:
                # Shadow: one dot in four on a slanted screen, like the drawing's halftone.
                row.append((x + 2 * y) % 4 == 0)
            else:
                row.append(False)
        rows.append(row)
    return rows


def write_boot_header(rows):
    upright = Image.new("1", (BOOT_SIZE, BOOT_SIZE), 1)
    px = upright.load()
    for y, row in enumerate(rows):
        for x, is_ink in enumerate(row):
            if is_ink:
                px[x, y] = 0
    data = upright.transpose(Image.ROTATE_90).tobytes()
    assert len(data) == BOOT_SIZE * BOOT_SIZE // 8

    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        "// Boot logo: the VARAGH open book with a V and a turning page, 192x192px,",
        "// 1 bit per pixel (1 = white). Stored rotated like Logo120 for the portrait",
        "// draw path. The shadow of the page is a dot screen. Generated from",
        "// src/images/varagh-logo.png by scripts/generate_logo.py; do not edit.",
        "static const uint8_t BootLogo192[] = {",
    ]
    for start in range(0, len(data), 19):
        chunk = ", ".join(f"0x{b:02x}" for b in data[start : start + 19])
        last = start + 19 >= len(data)
        lines.append(f"    {chunk}{'};' if last else ','}")
    lines.append(
        'static_assert(sizeof(BootLogo192) == 192 * 192 / 8, "BootLogo192 must be exactly 192x192 / 8 bytes");'
    )
    BOOT_HEADER.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return upright


def write_web_logo(master):
    small = master.resize((WEB_SIZE, WEB_SIZE), Image.LANCZOS)
    alpha = small.point(lambda v: 255 - v)
    logo = Image.merge("LA", (Image.new("L", small.size, 0), alpha))
    logo.save(WEB_LOGO, optimize=True)


def main():
    master = Image.open(MASTER).convert("L")
    write_boot_header(boot_rows(master))
    write_web_logo(master)
    print(f"wrote {BOOT_HEADER.relative_to(ROOT)} and {WEB_LOGO.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
