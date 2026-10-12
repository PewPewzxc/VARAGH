#!/bin/bash
set -e
cd "$(dirname "$0")"

# One shared set of 10-point symbols for both UI scales: the power glyph, and
# (VARAGH) the stars of a book rating: full, empty and left half filled.
# Restrict the source face so none of the converter's default character ranges
# are included.
"${PYTHON:-python3}" fontconvert.py ui_symbols_10 10 \
  ../builtinFonts/source/NotoSymbols/NotoSansSymbols2-Regular.ttf \
  --no-default-intervals \
  --additional-intervals 0x23FB,0x23FB \
  --additional-intervals 0x2605,0x2606 \
  --additional-intervals 0x2BEA,0x2BEA \
  > ../builtinFonts/ui_symbols_10.h
