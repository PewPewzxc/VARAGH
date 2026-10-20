#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "util/OnlineDictionaryText.h"

// The online dictionary: asks Wiktionary (and Wikipedia when Wiktionary has no
// entry) for a word and keeps every answer on the card as a small StarDict
// dictionary, one per language, so a word looked up once is there offline.
//
// Wi-Fi follows the firmware's usual routine: the reader restarts into a
// network-only boot for the request and restarts back into the book. The
// request file carries the word across the first restart and tells the reader
// after the second one which saved answer to show.
namespace OnlineDictionary {

// What the dictionary switcher returns for its Online row in place of a dictionary path.
constexpr char kSwitchPath[] = "online:";

struct Request {
  std::string word;
  onlinedict::Lang lang = onlinedict::Lang::English;
  // The source asked first, and the one tried when it has no answer. After the
  // lookup, provider is the one that answered.
  onlinedict::Provider provider = onlinedict::Provider::Wiktionary;
  onlinedict::Provider second = onlinedict::Provider::None;
  // False while the word is still to be fetched, true once its answer is saved.
  bool answered = false;
};

bool saveRequest(const Request& request);
bool loadRequest(Request& request);
void clearRequest();

// Base path (no extension) of the saved answers of one source for a language,
// usable wherever the path of an installed dictionary is. Each source keeps
// its own answers, so switching sources never mixes them up.
std::string storePath(onlinedict::Lang lang, onlinedict::Provider provider);
// True for a path made by storePath(); provider then names its source.
bool isStorePath(const std::string& path, onlinedict::Lang& lang, onlinedict::Provider& provider);
// True when Wi-Fi is already connected for in-place lookups (Keep Wi-Fi on)
// and the dictionary in use is one on the card (not saved online answers).
bool readyInPlace(const std::string& bookCachePath);
bool hasSaved(onlinedict::Lang lang, onlinedict::Provider provider, const std::string& word);
// Adds or replaces an answer. html is an entry in the layout of the VARAGH dictionaries.
bool save(onlinedict::Lang lang, onlinedict::Provider provider, const std::string& word, const std::string& html);

// The reader's choice for a language (Settings > Reader > Dictionaries),
// kept in /.crosspoint/online-sources.txt. second may be Provider::None.
onlinedict::Provider preferredProvider(onlinedict::Lang lang);
onlinedict::Provider secondProvider(onlinedict::Lang lang);
void setSources(onlinedict::Lang lang, onlinedict::Provider first, onlinedict::Provider second);

enum class Outcome : uint8_t { Found, NotFound, NoConnection, Cancelled };

struct FetchOptions {
  // The longest wait for the site at any one step of a request. A site that
  // does not answer ends the lookup (NoConnection) instead of being asked the
  // remaining questions as well.
  uint32_t timeoutMs = 10000;
  // Polled while a request waits; true gives the lookup up (Cancelled).
  std::function<bool()> cancelled;
};

// Needs Wi-Fi to be connected. On Found, word is the spelling the entry was found under.
Outcome fetch(onlinedict::Lang lang, onlinedict::Provider provider, std::string& word, std::string& html,
              const FetchOptions& options = FetchOptions());
// Asks request.provider, then request.second when the first has no answer or
// does not respond; request.provider is then set to the source that answered.
Outcome fetchWithFallback(Request& request, std::string& word, std::string& html,
                          const FetchOptions& options = FetchOptions());

}  // namespace OnlineDictionary
