#include "LanguageRegistry.h"

#include <algorithm>
#include <array>

#include "HyphenationCommon.h"
#include "generated/hyph-de.trie.h"
#include "generated/hyph-en.trie.h"
#include "generated/hyph-it.trie.h"
#include "generated/hyph-pt.trie.h"

// VARAGH: with CROSSINK_HYPHENATION_SD=1 the six larger, rarely used tables (~115 KB of flash) are
// files on the card: /.crosspoint/hyphenation/hyph-<tag>.bin = u32 LE root offset + trie bytes
// (scripts/export_hyphenation_bins.py). One is resident at a time; without its file a language is
// simply not hyphenated, like any language that has no table.
#ifndef CROSSINK_HYPHENATION_SD
#define CROSSINK_HYPHENATION_SD 0
#endif

#if CROSSINK_HYPHENATION_SD
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#else
#include "generated/hyph-es.trie.h"
#include "generated/hyph-fr.trie.h"
#include "generated/hyph-pl.trie.h"
#include "generated/hyph-ru.trie.h"
#include "generated/hyph-sv.trie.h"
#include "generated/hyph-uk.trie.h"
#endif

namespace {

#if CROSSINK_HYPHENATION_SD
SerializedHyphenationPatterns cardPatterns{0, nullptr, 0};
HeapByteBuffer cardData;
const LanguageHyphenator* cardOwner = nullptr;
constexpr size_t MAX_CARD_TABLE_BYTES = 64 * 1024;  // largest today: Russian, 33 KB
const SerializedHyphenationPatterns& fr_patterns = cardPatterns;
const SerializedHyphenationPatterns& ru_patterns = cardPatterns;
const SerializedHyphenationPatterns& es_patterns = cardPatterns;
const SerializedHyphenationPatterns& sv_patterns = cardPatterns;
const SerializedHyphenationPatterns& uk_patterns = cardPatterns;
const SerializedHyphenationPatterns& pl_patterns = cardPatterns;

bool loadCardTable(const char* tag) {
  cardOwner = nullptr;
  cardPatterns = {0, nullptr, 0};
  cardData.reset();
  char path[48];
  std::snprintf(path, sizeof(path), "/.crosspoint/hyphenation/hyph-%s.bin", tag);
  HalFile file;
  if (!Storage.openFileForRead("HYPH", path, file)) return false;
  const size_t size = file.fileSize();
  uint8_t header[4];
  bool ok = size > sizeof(header) && size <= MAX_CARD_TABLE_BYTES &&
            static_cast<size_t>(file.read(header, sizeof(header))) == sizeof(header);
  const size_t trieSize = size - sizeof(header);
  const size_t root = header[0] | (header[1] << 8) | (header[2] << 16) | (static_cast<size_t>(header[3]) << 24);
  if (ok) {
    cardData = makeDefaultByteBufferNoThrow(trieSize);
    ok = cardData && root < trieSize && static_cast<size_t>(file.read(cardData.get(), trieSize)) == trieSize;
  }
  file.close();
  if (!ok) {
    LOG_ERR("HYPH", "Unusable hyphenation table: %s", path);
    cardData.reset();
    return false;
  }
  cardPatterns = {root, cardData.get(), trieSize};
  return true;
}
#endif

// English hyphenation patterns (3/3 minimum prefix/suffix length)
LanguageHyphenator englishHyphenator(en_patterns, isLatinLetter, toLowerLatin, 3, 3);
LanguageHyphenator frenchHyphenator(fr_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator germanHyphenator(de_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator russianHyphenator(ru_patterns, isCyrillicLetter, toLowerCyrillic);
LanguageHyphenator spanishHyphenator(es_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator italianHyphenator(it_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator swedishHyphenator(sv_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator ukrainianHyphenator(uk_patterns, isCyrillicLetter, toLowerCyrillic);
LanguageHyphenator polishHyphenator(pl_patterns, isLatinLetter, toLowerLatin);
LanguageHyphenator portugueseHyphenator(pt_patterns, isLatinLetter, toLowerLatin);

using EntryArray = std::array<LanguageEntry, 10>;

const EntryArray& entries() {
  static const EntryArray kEntries = {{{"english", "en", &englishHyphenator},
                                       {"french", "fr", &frenchHyphenator},
                                       {"german", "de", &germanHyphenator},
                                       {"russian", "ru", &russianHyphenator},
                                       {"spanish", "es", &spanishHyphenator},
                                       {"italian", "it", &italianHyphenator},
                                       {"polish", "pl", &polishHyphenator},
                                       {"portuguese", "pt", &portugueseHyphenator},
                                       {"swedish", "sv", &swedishHyphenator},
                                       {"ukrainian", "uk", &ukrainianHyphenator}}};
  return kEntries;
}

}  // namespace

const LanguageHyphenator* getLanguageHyphenatorForPrimaryTag(const std::string& primaryTag) {
  const auto& allEntries = entries();
  const auto it = std::find_if(allEntries.begin(), allEntries.end(),
                               [&primaryTag](const LanguageEntry& entry) { return primaryTag == entry.primaryTag; });
  if (it == allEntries.end()) return nullptr;
#if CROSSINK_HYPHENATION_SD
  for (const char* tag : {"fr", "ru", "es", "sv", "uk", "pl"}) {
    if (primaryTag != tag) continue;
    if (cardOwner != it->hyphenator) {
      if (!loadCardTable(tag)) return nullptr;
      cardOwner = it->hyphenator;
    }
    break;
  }
#endif
  return it->hyphenator;
}

LanguageEntryView getLanguageEntries() {
  const auto& allEntries = entries();
  return LanguageEntryView{allEntries.data(), allEntries.size()};
}
