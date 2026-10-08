#pragma once

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
  // False while the word is still to be fetched, true once its answer is saved.
  bool answered = false;
};

bool saveRequest(const Request& request);
bool loadRequest(Request& request);
void clearRequest();

// Base path (no extension) of the saved answers for a language, usable wherever
// the path of an installed dictionary is.
std::string storePath(onlinedict::Lang lang);
bool hasSaved(onlinedict::Lang lang, const std::string& word);
// Adds or replaces an answer. html is an entry in the layout of the VARAGH dictionaries.
bool save(onlinedict::Lang lang, const std::string& word, const std::string& html);

enum class Outcome : uint8_t { Found, NotFound, NoConnection };
// Needs Wi-Fi to be connected. On Found, word is the spelling the entry was found under.
Outcome fetch(onlinedict::Lang lang, std::string& word, std::string& html);

}  // namespace OnlineDictionary
