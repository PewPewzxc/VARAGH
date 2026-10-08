#pragma once

#include <string>

#include "activities/Activity.h"
#include "network/OnlineDictionary.h"

// The Wi-Fi half of an online dictionary lookup.
//
// Started twice for one lookup. First as a hand-off from the reader, where it
// only lets the reader close before restarting the device into a network-only
// boot (the same routine KOReader sync uses). Then from that boot: it joins
// Wi-Fi, fetches the entry, saves it to the card and restarts into the book,
// where the reader shows the saved answer in the usual dictionary window.
class OnlineDictionaryActivity final : public Activity {
 public:
  enum class Mode : uint8_t { HandOff, Fetch };

  OnlineDictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode);

  // Entry point for the reader's lookup screens. When the word has been fetched
  // before, returns the base path of the saved answers, to be searched like any
  // dictionary. With "keep Wi-Fi on" the word is fetched right here and the
  // same path is returned (or an empty string when there is no entry).
  // Otherwise the Wi-Fi lookup starts (the reader closes and the device
  // restarts) and an empty string is returned.
  static std::string begin(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& word,
                           const std::string& bookCachePath);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State : uint8_t { Starting, Connecting, Fetching, NotFound, NoConnection, Leaving };

  void onWifiResult(bool connected);
  void fetch();
  // Wi-Fi off, then back into the book (or Home when no book is open).
  void leave();

  const Mode mode;
  State state = State::Starting;
  bool fetchStarted = false;
  OnlineDictionary::Request request;
};
