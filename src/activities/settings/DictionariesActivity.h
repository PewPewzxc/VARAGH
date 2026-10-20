#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "activities/apps/GameUi.h"
#include "components/OptionPopup.h"
#include "util/OnlineDictionaryText.h"

// VARAGH 1.3.0: Settings > Reader > Dictionaries. One page for everything a
// lookup uses: a tab for each reading language (+ adds one, hold to remove),
// and for the chosen language its dictionary on the card and its first and
// second online source; below, Keep Wi-Fi on in books.
class DictionariesActivity final : public Activity {
 public:
  DictionariesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Row { Offline = 0, OnlineFirst, OnlineSecond, KeepWifi, kRowCount };

  struct CardDictionary {
    std::string name;
    std::string path;
    bool langKnown = false;
    bool sameLanguage = false;  // "en-en": explains words in their own language
    onlinedict::Lang lang = onlinedict::Lang::English;
  };

  std::vector<onlinedict::Lang> chosenLanguages() const;
  onlinedict::Lang current() const;
  bool hasCardDictionary(onlinedict::Lang lang) const;
  std::string offlineName(onlinedict::Lang lang) const;
  void layout();
  void openPicker(Row row);
  void openAddLanguage();
  void removeLanguage(int index);
  void toggleWifi();

  std::vector<CardDictionary> dictionaries;
  // Each language's chosen dictionary path, read once (render runs often).
  std::string offlinePaths[onlinedict::kLangCount];
  void chooseOffline(onlinedict::Lang lang, const std::string& path);
  int selected = 0;
  int focusRow = -1;  // the row Up/Down moved to; -1 = none (touch only)
  OptionPopup popup;
  std::vector<int> pickerValues;  // what each popup row stands for
  gameui::Box chipBoxes[onlinedict::kLangCount + 1];
  int chipCount = 0;
  gameui::Box rowBoxes[kRowCount];
  int hintY = 0;
  bool longPressHandled = false;
};
