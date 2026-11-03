"""VARAGH: small outlines the scalable built-in reader fonts borrow letters from.

vazirmatn_persian.ttf: Persian letters (EpdFontFamily::withSupplement).
noto_ipa.ttf: pronunciation (IPA) letters and combining marks (the family fallback).
Only those codepoints are kept: Latin comes from the reader font itself, and the
renderer shapes Arabic into presentation forms itself, so no GSUB/GPOS is needed.
Keep the ranges in step with PERSIAN_INTERVALS / IPA_INTERVALS in convert-builtin-fonts.sh.

  python subset-reader-supplements.py <Vazirmatn-Medium.ttf> <NotoSans-Regular.ttf>
"""
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

PERSIAN_RANGES = [
    (0x060C, 0x060C), (0x061B, 0x061B), (0x061F, 0x061F), (0x0621, 0x0621), (0x0640, 0x0640),
    (0x064B, 0x0655),  # harakat (kasra of ezafe, tanvin, shadda, hamza above/below)
    (0x0660, 0x066C), (0x0670, 0x0670), (0x06F0, 0x06F9),
    (0xFB56, 0xFB59), (0xFB7A, 0xFB7D), (0xFB8A, 0xFB8B), (0xFB8E, 0xFB95), (0xFBFC, 0xFBFF),
    (0xFE80, 0xFEFC),
]
# IPA letters + combining marks, the Greek letters IPA borrows (beta, theta, chi) and the
# phonetic-extension letters Wiktionary pronunciations use (ᵊ ᵋ ᵑ ᵝ ᵞ ᵻ ᵿ).
IPA_RANGES = [(0x0250, 0x02FF), (0x0300, 0x036F), (0x03B2, 0x03B2), (0x03B8, 0x03B8), (0x03C7, 0x03C7),
              (0x1D4A, 0x1D4B), (0x1D51, 0x1D51), (0x1D5D, 0x1D5E), (0x1D7B, 0x1D7B), (0x1D7F, 0x1D7F)]

def subset_to(source, ranges, name):
    font = TTFont(source)
    options = subset.Options()
    options.layout_features = []
    options.hinting = False  # the S3 builds autohint (FREEINK_FONT_ENABLE_AUTOHINT)
    options.name_IDs = [0, 1, 2, 4, 6]
    options.notdef_outline = False
    sub = subset.Subsetter(options)
    sub.populate(unicodes=[cp for lo, hi in ranges for cp in range(lo, hi + 1)])
    sub.subset(font)
    out = Path(__file__).resolve().parents[1] / "scalableFonts" / name
    font.save(out)
    print(f"{out} {out.stat().st_size} bytes")

if __name__ == "__main__":
    subset_to(sys.argv[1], PERSIAN_RANGES, "vazirmatn_persian.ttf")
    subset_to(sys.argv[2], IPA_RANGES, "noto_ipa.ttf")
