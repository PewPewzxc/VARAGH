#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "GermanStemmer.h"

namespace {

// The dictionary index folds ASCII case, so a candidate matches a headword
// when they are equal after ASCII lower-casing.
std::string asciiLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; });
  return s;
}

bool proposes(const std::string& word, const std::string& headword) {
  const auto v = GermanStemmer::variants(word);
  return std::any_of(v.begin(), v.end(), [&](const std::string& c) { return asciiLower(c) == asciiLower(headword); });
}

}  // namespace

TEST(GermanStemmer, CaseOfUmlautInitial) {
  EXPECT_TRUE(proposes("Über", "über"));
  EXPECT_TRUE(proposes("ärger", "Ärger"));
}

TEST(GermanStemmer, NounPlurals) {
  EXPECT_TRUE(proposes("Hunde", "Hund"));
  EXPECT_TRUE(proposes("Frauen", "Frau"));
  EXPECT_TRUE(proposes("Blumen", "Blume"));
  EXPECT_TRUE(proposes("Kindern", "Kind"));
  EXPECT_TRUE(proposes("Autos", "Auto"));
  EXPECT_TRUE(proposes("Hauses", "Haus"));
  EXPECT_TRUE(proposes("Lehrerinnen", "Lehrerin"));
}

TEST(GermanStemmer, UmlautPlurals) {
  EXPECT_TRUE(proposes("Häuser", "Haus"));
  EXPECT_TRUE(proposes("Bäume", "Baum"));
  EXPECT_TRUE(proposes("Mütter", "Mutter"));
  EXPECT_TRUE(proposes("Äpfel", "Apfel"));
  EXPECT_TRUE(proposes("Bücher", "Buch"));
  EXPECT_TRUE(proposes("Gärten", "Garten"));
}

TEST(GermanStemmer, Adjectives) {
  EXPECT_TRUE(proposes("schönen", "schön"));
  EXPECT_TRUE(proposes("kleinste", "klein"));
  EXPECT_TRUE(proposes("schönsten", "schön"));
  EXPECT_TRUE(proposes("größeren", "groß"));
}

TEST(GermanStemmer, Verbs) {
  EXPECT_TRUE(proposes("mache", "machen"));
  EXPECT_TRUE(proposes("machst", "machen"));
  EXPECT_TRUE(proposes("macht", "machen"));
  EXPECT_TRUE(proposes("machte", "machen"));
  EXPECT_TRUE(proposes("arbeitet", "arbeiten"));
  EXPECT_TRUE(proposes("arbeiteten", "arbeiten"));
  EXPECT_TRUE(proposes("wandert", "wandern"));
}

TEST(GermanStemmer, Participles) {
  EXPECT_TRUE(proposes("gemacht", "machen"));
  EXPECT_TRUE(proposes("gearbeitet", "arbeiten"));
  EXPECT_TRUE(proposes("gefahren", "fahren"));
  EXPECT_TRUE(proposes("gemachte", "machen"));
  EXPECT_TRUE(proposes("aufgemacht", "aufmachen"));
  EXPECT_TRUE(proposes("anzurufen", "anrufen"));
}

TEST(GermanStemmer, NeverReturnsInputOrDuplicates) {
  for (const char* word : {"Häuser", "gearbeitet", "Lehrerinnen", "aufgemacht", "ist", "a", ""}) {
    auto v = GermanStemmer::variants(word);
    EXPECT_LE(v.size(), GermanStemmer::MAX_VARIANTS);
    EXPECT_EQ(std::count(v.begin(), v.end(), std::string(word)), 0) << word;
    std::sort(v.begin(), v.end());
    EXPECT_EQ(std::adjacent_find(v.begin(), v.end()), v.end()) << word;
  }
}

TEST(GermanStemmer, DetectsGermanSourceDictionaries) {
  EXPECT_TRUE(GermanStemmer::isGermanSource("de-en", "", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("de", "", ""));
  EXPECT_FALSE(GermanStemmer::isGermanSource("en-de", "German", ""));
  EXPECT_FALSE(GermanStemmer::isGermanSource("dev", "", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "WikDict de-en", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "German-English Wiktionary", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "Deutsch-Englisch", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "", "/dictionaries/de-en/dict"));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "German - English (FreeDict / Ding 1.9)", ""));
  EXPECT_TRUE(GermanStemmer::isGermanSource("", "", "/dictionaries/German-English/deu-eng-freedict"));
  EXPECT_FALSE(GermanStemmer::isGermanSource("", "", "/dictionaries/eng-deu-freedict"));
  EXPECT_FALSE(GermanStemmer::isGermanSource("", "English-German", ""));
  EXPECT_FALSE(GermanStemmer::isGermanSource("", "English Wiktionary", "/dictionaries/en/dict"));
}
