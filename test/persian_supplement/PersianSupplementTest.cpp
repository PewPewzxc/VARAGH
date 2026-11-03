#include <EpdFontFamily.h>
#include <builtinFonts/lexenddeca_14_bold.h>
#include <builtinFonts/lexenddeca_14_regular.h>
#include <builtinFonts/vazirmatn_persian_14_bold.h>
#include <builtinFonts/vazirmatn_persian_14_regular.h>
#include <gtest/gtest.h>

namespace {
const EpdFont latin(&lexenddeca_14_regular);
const EpdFont persian(&vazirmatn_persian_14_regular);
const EpdFontFamily plain(&latin, nullptr, nullptr, nullptr);
const EpdFontFamily withPersian = plain.withSupplement(&persian, &persian);
const EpdFont latinBold(&lexenddeca_14_bold);
const EpdFont persianBold(&vazirmatn_persian_14_bold);
const EpdFontFamily withBoldPersian =
    EpdFontFamily(&latin, &latinBold, nullptr, nullptr).withSupplement(&persian, &persianBold);
}  // namespace

// The built-in Persian supplement holds the letters only in their shaped forms, but
// text is shaped before drawing, so dictionary and flashcard text must keep them.
TEST(PersianSupplement, ShapedFormsCountAsDrawingTheBaseLetters) {
  for (const uint32_t cp : {0x0627u, 0x062Eu, 0x0631u, 0x062Fu, 0x067Eu, 0x06AFu, 0x06CCu}) {
    EXPECT_FALSE(withPersian.hasCodepoint(cp)) << std::hex << cp;
    EXPECT_TRUE(withPersian.drawsCodepoint(cp)) << std::hex << cp;
  }
  EXPECT_TRUE(withPersian.drawsCodepoint(0x06F1));  // Persian digit, held as it is
  EXPECT_TRUE(withPersian.drawsCodepoint('A'));
}

TEST(PersianSupplement, AFontWithoutPersianStillLacksIt) {
  EXPECT_FALSE(plain.drawsCodepoint(0x0627));
  EXPECT_FALSE(plain.drawsCodepoint(0x06F1));
  EXPECT_FALSE(withPersian.drawsCodepoint(0x0750));  // outside the shaped range rule
}

// Focus Reading bolds the start of Persian words: bold text takes the bold supplement.
TEST(PersianSupplement, BoldTextUsesTheBoldPersianFont) {
  for (const uint32_t cp : {0xFE8Eu, 0xFEA7u, 0xFB58u}) {  // alef final, khah initial, peh initial
    EXPECT_EQ(withBoldPersian.getGlyph(cp, EpdFontFamily::BOLD), persianBold.getGlyph(cp)) << std::hex << cp;
    EXPECT_EQ(withBoldPersian.getGlyph(cp, EpdFontFamily::REGULAR), persian.getGlyph(cp)) << std::hex << cp;
  }
  EXPECT_TRUE(withBoldPersian.drawsCodepoint(0x062E, EpdFontFamily::BOLD));
}
