# Fonts in VARAGH

## Which font to use

Select **NotoVazir** as the reader font:
**Settings → Reader → Font Options → Font Family → NotoVazir.**

NotoVazir has every letter VARAGH needs in one font: English and German
(Noto Serif), Persian and Arabic (Vazirmatn), IPA pronunciation letters and
symbols (Noto Serif, Noto Sans Math). With it selected, books, dictionary
definitions, highlights and flashcards all display correctly.

### Using another reader font

On the X4 Pro you can pick any other font too. When that font cannot draw a
Persian word or a pronunciation symbol, VARAGH draws that word in NotoVazir at
the same size (the rest of the text stays in your font). This needs NotoVazir,
or another installed font with Persian and IPA letters, in `/.fonts/`.

Books whose language is Persian or Arabic switch to NotoVazir for that book
automatically, because a whole book reads better in one font.

## Installing NotoVazir

Copy the `NotoVazir` folder from `varagh-sd-card-*.zip` (in each release) to
`/.fonts/NotoVazir/` on the SD card. It contains sizes 10, 12, 14, 16, 18 and 20.

## Building NotoVazir yourself

Source fonts (all SIL Open Font License 1.1, free to download):

- Noto Serif Medium, Medium Italic, Bold, Bold Italic: https://fonts.google.com/noto/specimen/Noto+Serif
- Noto Sans Math Regular: https://fonts.google.com/noto/specimen/Noto+Sans+Math
- Vazirmatn Medium and Bold: https://github.com/rastikerdar/vazirmatn

Put them in one folder (`$S` below) and run, from the repository root:

```bash
S=/path/to/source-fonts
AR="0x0600-0x06FF;0x0750-0x077F;0x08A0-0x08FF;0xFB50-0xFDFF;0xFE70-0xFEFF"
SY="0x2100-0x214F;0x2190-0x21FF;0x2200-0x22FF;0x2300-0x23FF;0x25A0-0x25FF;0x27E0-0x27FF"
python lib/EpdFont/scripts/fontconvert_sdcard.py \
  --regular $S/NotoSerif-Medium.ttf --bold $S/NotoSerif-Bold.ttf \
  --italic $S/NotoSerif-MediumItalic.ttf --bolditalic $S/NotoSerif-BoldItalic.ttf \
  --fallback-regular $S/Vazirmatn-Medium.ttf --fallback-regular-ranges "$AR" \
  --fallback-regular $S/NotoSansMath-Regular.ttf --fallback-regular-ranges "$SY" \
  --fallback-italic $S/Vazirmatn-Medium.ttf --fallback-italic-ranges "$AR" \
  --fallback-italic $S/NotoSansMath-Regular.ttf --fallback-italic-ranges "$SY" \
  --fallback-bold $S/Vazirmatn-Bold.ttf --fallback-bold-ranges "$AR" \
  --fallback-bold $S/NotoSansMath-Regular.ttf --fallback-bold-ranges "$SY" \
  --fallback-bolditalic $S/Vazirmatn-Bold.ttf --fallback-bolditalic-ranges "$AR" \
  --fallback-bolditalic $S/NotoSansMath-Regular.ttf --fallback-bolditalic-ranges "$SY" \
  --intervals "reading,arabic,symbols,(0x0250-0x02FF),(0x1D00-0x1DBF),(0x1DC0-0x1DFF),(0x20D0-0x20FF),(0xFE20-0xFE2F),(0x2100-0x214F),(0x27E0-0x27FF)" \
  --darken-aa --sizes 10,12,14,16,18,20 --name NotoVazir --output-dir out/NotoVazir
```

On Windows run it from Git Bash with `PYTHONUTF8=1` set. `--darken-aa` makes
the anti-aliased edges a little darker, which reads better on the X4 Pro panel.

When you share the result, include the OFL licence texts of Noto and Vazirmatn
(they are in the release zip). None of the three fonts has a Reserved Font Name,
so the merged font may keep the name NotoVazir.

## Built-in menu font

The menu font (Inter) is compiled into the firmware. VARAGH adds IPA letters
(U+0250–U+02FF) from Inter as small supplement fonts
(`lib/EpdFont/builtinFonts/inter_ipa_*.h`), generated with:

```bash
cd lib/EpdFont/scripts
python fontconvert.py inter_ipa_10_regular 10 ../builtinFonts/source/Inter/Inter-Regular.ttf \
  ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf --no-default-intervals \
  --additional-intervals 0x0250,0x02FF > ../builtinFonts/inter_ipa_10_regular.h
```

(repeat for 8 regular, 10 bold, 12 regular and 12 bold).
