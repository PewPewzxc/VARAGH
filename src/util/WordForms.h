#pragma once

#include <string>
#include <vector>

// Candidate base forms of a word, for dictionary lookup only.
//
// Like GermanStemmer, this proposes plausible headwords and lets the
// dictionary index act as the filter: the first candidate that is found wins,
// so the order matters. The dictionary's own .syn list of word forms is asked
// before any of these guesses (see Dictionary::locateWithStemVariants).
namespace WordForms {

// Each candidate costs one index probe on the SD card.
constexpr size_t MAX_VARIANTS = 24;

// English: the smallest change comes first, so "nodes" tries "node" before
// "nod" and "hoping" tries "hope" before "hop". ASCII words only; the result
// is lower case and never contains the word itself.
std::vector<std::string> english(const std::string& word);

// Persian (Arabic-script words): plural, possessive, ezafe and comparative
// endings with or without the half-space (ZWNJ), the verb prefixes می/نمی/ب/ن,
// personal endings, and the infinitive of regular and common irregular verbs
// ("می‌روم" -> "رفتن"). Vowel marks and tatweel are dropped and Arabic kaf/yeh
// become the Persian letters first; that spelling itself is the first
// candidate when it differs from the word.
std::vector<std::string> persian(const std::string& word);

// True when the word holds Arabic-script letters.
bool isArabicScript(const std::string& word);

}  // namespace WordForms
