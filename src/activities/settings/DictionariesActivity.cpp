#include "DictionariesActivity.h"

#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/ConfirmationActivity.h"
#include "fontIds.h"
#include "network/OnlineDictionary.h"
#include "util/Dictionary.h"
#include "util/DictionaryRegistry.h"

namespace {

constexpr int kSide = 20;
constexpr int kChipHeight = 40;
constexpr int kChipGap = 10;
constexpr int kRowHeight = 58;
constexpr unsigned long kRemoveHoldMs = 800;

// Keep Wi-Fi on next to a book only where the memory allows it (as before).
constexpr bool kWifiRowShown = HalPowerManager::LOW_POWER_FREQ >= 80;

}  // namespace

DictionariesActivity::DictionariesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Dictionaries", renderer, mappedInput) {}

void DictionariesActivity::onEnter() {
  Activity::onEnter();
  // The registry is filled on demand: scan the card now (as the dictionary
  // picker does) and free it again in onExit().
  dictionaryRegistry.discover();
  dictionaries.clear();
  // The dictionaries on the card and the language of each (its .ifo lang=).
  for (const auto& entry : dictionaryRegistry.getEntries()) {
    CardDictionary card;
    card.name = entry.name;
    card.path = entry.basePath;
    const DictInfo info = Dictionary::readInfo(entry.basePath.c_str());
    card.langKnown = info.valid && onlinedict::langFromCode(info.lang, card.lang);
    card.sameLanguage = card.langKnown && info.lang[2] == '-' && info.lang[3] == info.lang[0] &&
                        info.lang[4] == info.lang[1];
    dictionaries.push_back(std::move(card));
  }
  for (uint8_t i = 0; i < onlinedict::kLangCount; ++i) {
    const auto lang = static_cast<onlinedict::Lang>(i);
    offlinePaths[i] = Dictionary::readLanguageDictPath(onlinedict::code(lang));
    if (!offlinePaths[i].empty()) continue;
    // Nothing chosen yet: take this language's dictionary from the card,
    // one in the same language ("de-de") before a translating one ("de-en").
    const CardDictionary* best = nullptr;
    for (const auto& card : dictionaries) {
      if (!card.langKnown || card.lang != lang) continue;
      if (!best || (card.sameLanguage && !best->sameLanguage)) best = &card;
    }
    if (best) chooseOffline(lang, best->path);
  }
  if (chosenLanguages().empty()) SETTINGS.readingLanguages = 1;  // English
  selected = 0;
  requestUpdate();
}

void DictionariesActivity::onExit() {
  dictionaryRegistry.clear();
  Activity::onExit();
}

void DictionariesActivity::chooseOffline(const onlinedict::Lang lang, const std::string& path) {
  offlinePaths[static_cast<uint8_t>(lang)] = path;
  Dictionary::saveLanguageDictPath(onlinedict::code(lang), path.c_str());
  // Books in a language without its own choice use the general dictionary.
  if (!path.empty() && Dictionary::readConfiguredDictPath().empty()) Dictionary::saveGlobalDictPath(path.c_str());
}

std::vector<onlinedict::Lang> DictionariesActivity::chosenLanguages() const {
  std::vector<onlinedict::Lang> out;
  for (uint8_t i = 0; i < onlinedict::kLangCount; ++i) {
    if (SETTINGS.readingLanguages & (1u << i)) out.push_back(static_cast<onlinedict::Lang>(i));
  }
  return out;
}

onlinedict::Lang DictionariesActivity::current() const {
  const auto chosen = chosenLanguages();
  if (chosen.empty()) return onlinedict::Lang::English;
  return chosen[selected < static_cast<int>(chosen.size()) ? selected : 0];
}

bool DictionariesActivity::hasCardDictionary(const onlinedict::Lang lang) const {
  for (const auto& card : dictionaries) {
    if (card.langKnown && card.lang == lang) return true;
  }
  return false;
}

std::string DictionariesActivity::offlineName(const onlinedict::Lang lang) const {
  const std::string& path = offlinePaths[static_cast<uint8_t>(lang)];
  for (const auto& card : dictionaries) {
    if (card.path == path) return card.name;
  }
  return tr(STR_DICT_NONE);
}

void DictionariesActivity::layout() {
  const int width = renderer.getScreenWidth();
  int x = kSide;
  int y = gameui::contentTop(renderer, mappedInput) + 6;
  const auto chosen = chosenLanguages();
  chipCount = 0;
  // One chip per language, then "+"; chips wrap onto a second line.
  for (size_t i = 0; i <= chosen.size(); ++i) {
    const char* label = i < chosen.size() ? onlinedict::nativeName(chosen[i]) : "+";
    const int w = i < chosen.size() ? renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD) + 30
                                    : kChipHeight;
    if (x + w > width - kSide) {
      x = kSide;
      y += kChipHeight + kChipGap;
    }
    chipBoxes[chipCount++] = {x, y, w, kChipHeight};
    x += w + kChipGap;
  }
  y += kChipHeight + 16;
  for (int row = 0; row < kRowCount; ++row) {
    if (row == KeepWifi) y += 34;  // the "Online" section label
    rowBoxes[row] = {0, y, width, kRowHeight};
    y += kRowHeight;
    if (row == Offline && !hasCardDictionary(current())) y += 44;  // the note under it
  }
  hintY = y + 10;
}

void DictionariesActivity::openPicker(const Row row) {
  const onlinedict::Lang lang = current();
  std::vector<std::string> options;
  pickerValues.clear();
  int currentIndex = 0;
  if (row == Offline) {
    const std::string& chosenPath = offlinePaths[static_cast<uint8_t>(lang)];
    options.push_back(tr(STR_DICT_NONE));
    pickerValues.push_back(-1);
    // This language's dictionaries first, then the rest.
    for (int pass = 0; pass < 2; ++pass) {
      for (size_t i = 0; i < dictionaries.size(); ++i) {
        const bool matches = dictionaries[i].langKnown && dictionaries[i].lang == lang;
        if (matches != (pass == 0)) continue;
        if (dictionaries[i].path == chosenPath) currentIndex = static_cast<int>(options.size());
        options.push_back(dictionaries[i].name);
        pickerValues.push_back(static_cast<int>(i));
      }
    }
    popup.show(StrId::STR_DICT_OFFLINE, options, currentIndex, [this, lang](const int index) {
      const int card = pickerValues[index];
      chooseOffline(lang, card < 0 ? std::string() : dictionaries[card].path);
      requestUpdate();
    });
    requestUpdate();  // without it the popup is open but never drawn, and eats every tap
    return;
  }
  const onlinedict::Provider first = OnlineDictionary::preferredProvider(lang);
  const onlinedict::Provider second = OnlineDictionary::secondProvider(lang);
  const onlinedict::Provider now = row == OnlineFirst ? first : second;
  for (uint8_t p = 0; p < onlinedict::kProviderCount; ++p) {
    const auto provider = static_cast<onlinedict::Provider>(p);
    if (!onlinedict::providerServes(provider, lang)) continue;
    if (provider == now) currentIndex = static_cast<int>(options.size());
    options.push_back(onlinedict::providerName(provider));
    pickerValues.push_back(p);
  }
  if (row == OnlineSecond) {
    if (now == onlinedict::Provider::None) currentIndex = static_cast<int>(options.size());
    options.push_back(tr(STR_SRC_NONE));
    pickerValues.push_back(static_cast<int>(onlinedict::Provider::None));
  }
  popup.show(row == OnlineFirst ? StrId::STR_DICT_ONLINE_FIRST : StrId::STR_DICT_ONLINE_SECOND, options,
             currentIndex, [this, lang, row, first, second](const int index) {
               const auto picked = static_cast<onlinedict::Provider>(pickerValues[index]);
               if (row == OnlineFirst) {
                 // Picking the second source as first swaps them.
                 OnlineDictionary::setSources(lang, picked, picked == second ? first : second);
               } else {
                 OnlineDictionary::setSources(lang, first, picked);
               }
               requestUpdate();
             });
  requestUpdate();
}

void DictionariesActivity::openAddLanguage() {
  std::vector<std::string> options;
  pickerValues.clear();
  for (uint8_t i = 0; i < onlinedict::kLangCount; ++i) {
    if (SETTINGS.readingLanguages & (1u << i)) continue;
    options.push_back(onlinedict::nativeName(static_cast<onlinedict::Lang>(i)));
    pickerValues.push_back(i);
  }
  if (options.empty()) return;
  popup.show(StrId::STR_DICT_ADD_LANGUAGE, options, 0, [this](const int index) {
    const int lang = pickerValues[index];
    SETTINGS.readingLanguages = static_cast<uint16_t>(SETTINGS.readingLanguages | (1u << lang));
    SETTINGS.saveToFile();
    const auto chosen = chosenLanguages();
    for (size_t i = 0; i < chosen.size(); ++i) {
      if (static_cast<int>(chosen[i]) == lang) selected = static_cast<int>(i);
    }
    requestUpdate();
  });
  requestUpdate();
}

void DictionariesActivity::removeLanguage(const int index) {
  const auto chosen = chosenLanguages();
  if (chosen.size() <= 1 || index >= static_cast<int>(chosen.size())) return;  // one language always stays
  SETTINGS.readingLanguages =
      static_cast<uint16_t>(SETTINGS.readingLanguages & ~(1u << static_cast<uint8_t>(chosen[index])));
  SETTINGS.saveToFile();
  selected = 0;
  requestUpdate();
}

void DictionariesActivity::toggleWifi() {
  if (SETTINGS.onlineDictWifiAlwaysOn != 0) {
    SETTINGS.onlineDictWifiAlwaysOn = 0;
    SETTINGS.saveToFile();
    requestUpdate();
    return;
  }
  // Same warning as before: Wi-Fi next to a book costs battery.
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_KEEP_WIFI_IN_BOOKS),
                                                                tr(STR_ONLINE_WIFI_WARNING)),
                         [this](const ActivityResult& result) {
                           if (!result.isCancelled) {
                             SETTINGS.onlineDictWifiAlwaysOn = 1;
                             SETTINGS.saveToFile();
                           }
                           requestUpdate();
                         });
}

void DictionariesActivity::loop() {
  if (popup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    setResult(ActivityResult{});
    finish();
    return;
  }
  const int languages = chipCount - 1;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left) && selected > 0) {
    --selected;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Right) && selected + 1 < languages) {
    ++selected;
    requestUpdate();
  }

  const int lastRow = kWifiRowShown ? KeepWifi : OnlineSecond;
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) && focusRow < lastRow) {
    ++focusRow;
    requestUpdate();
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) && focusRow > 0) {
    --focusRow;
    requestUpdate();
  }
  if (focusRow >= 0 && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (focusRow == KeepWifi) {
      toggleWifi();
    } else {
      openPicker(static_cast<Row>(focusRow));
    }
    return;
  }

  int x = 0;
  int y = 0;
  // Hold a language to remove it.
  if (mappedInput.isScreenTouchLongPress(x, y, kRemoveHoldMs)) {
    if (!longPressHandled) {
      for (int i = 0; i < languages; ++i) {
        if (!chipBoxes[i].contains(x, y)) continue;
        longPressHandled = true;
        mappedInput.suppressCurrentTouchContact();
        removeLanguage(i);
        return;
      }
    }
    return;
  }
  longPressHandled = false;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  for (int i = 0; i < chipCount; ++i) {
    if (!chipBoxes[i].contains(x, y)) continue;
    if (i == languages) {
      openAddLanguage();
    } else {
      selected = i;
      requestUpdate();
    }
    return;
  }
  for (int row = 0; row < kRowCount; ++row) {
    if (row == KeepWifi && !kWifiRowShown) continue;
    if (!rowBoxes[row].contains(x, y)) continue;
    if (row == KeepWifi) {
      toggleWifi();
    } else {
      openPicker(static_cast<Row>(row));
    }
    return;
  }
}

void DictionariesActivity::render(RenderLock&&) {
  layout();
  renderer.clearScreen();
  gameui::drawHeader(renderer, mappedInput, tr(STR_DICTIONARIES));
  const int width = renderer.getScreenWidth();
  const auto chosen = chosenLanguages();
  const onlinedict::Lang lang = current();

  for (int i = 0; i < chipCount; ++i) {
    const gameui::Box& chip = chipBoxes[i];
    const bool on = i == selected;
    const char* label = i < static_cast<int>(chosen.size()) ? onlinedict::nativeName(chosen[i]) : "+";
    renderer.fillRoundedRect(chip.x, chip.y, chip.w, chip.h, chip.h / 2, on ? Color::Black : Color::White);
    renderer.drawRoundedRect(chip.x, chip.y, chip.w, chip.h, 2, chip.h / 2, true);
    gameui::centredText(renderer, UI_10_FONT_ID, chip, label, !on, EpdFontFamily::BOLD);
  }

  const auto drawRow = [&](const gameui::Box& box, const char* label, const char* value) {
    const int textY = box.y + (box.h - renderer.getLineHeight(UI_12_FONT_ID)) / 2;
    renderer.drawText(UI_12_FONT_ID, kSide + 8, textY, label);
    const int labelW = renderer.getTextWidth(UI_12_FONT_ID, label);
    const int chevronX = width - kSide - 8;
    renderer.drawLine(chevronX - 6, box.y + box.h / 2 - 7, chevronX, box.y + box.h / 2, 2, true);
    renderer.drawLine(chevronX, box.y + box.h / 2, chevronX - 6, box.y + box.h / 2 + 7, 2, true);
    const std::string shown =
        renderer.truncatedText(UI_10_FONT_ID, value, chevronX - 18 - (kSide + 8 + labelW + 16));
    const int valueW = renderer.getTextWidth(UI_10_FONT_ID, shown.c_str());
    renderer.drawText(UI_10_FONT_ID, chevronX - 14 - valueW,
                      box.y + (box.h - renderer.getLineHeight(UI_10_FONT_ID)) / 2, shown.c_str());
  };

  const std::string offline = offlineName(lang);
  drawRow(rowBoxes[Offline], tr(STR_DICT_OFFLINE), offline.c_str());
  if (!hasCardDictionary(lang)) {
    char note[160];
    // No dictionary folder on the card yet: name the one the scan looks for.
    const std::string& root = dictionaryRegistry.root();
    snprintf(note, sizeof(note), tr(STR_DICT_NO_CARD_DICT), onlinedict::nativeName(lang),
             root.empty() ? "/dictionaries" : root.c_str());
    int noteY = rowBoxes[Offline].y + kRowHeight - 4;
    for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, note, width - 2 * (kSide + 8), 2)) {
      renderer.drawText(SMALL_FONT_ID, kSide + 8, noteY, line.c_str());
      noteY += renderer.getLineHeight(SMALL_FONT_ID) + 2;
    }
  }
  drawRow(rowBoxes[OnlineFirst], tr(STR_DICT_ONLINE_FIRST),
          onlinedict::providerName(OnlineDictionary::preferredProvider(lang)));
  const onlinedict::Provider second = OnlineDictionary::secondProvider(lang);
  drawRow(rowBoxes[OnlineSecond], tr(STR_DICT_ONLINE_SECOND),
          second == onlinedict::Provider::None ? tr(STR_SRC_NONE) : onlinedict::providerName(second));

  if (kWifiRowShown) {
    const gameui::Box& box = rowBoxes[KeepWifi];
    renderer.drawText(SMALL_FONT_ID, kSide + 8, box.y - 26, tr(STR_DICT_SECTION_ONLINE), true, EpdFontFamily::BOLD);
    renderer.drawText(UI_12_FONT_ID, kSide + 8, box.y + (box.h - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                      tr(STR_KEEP_WIFI_IN_BOOKS));
    // A switch like the other settings: knob right and filled when on.
    const bool on = SETTINGS.onlineDictWifiAlwaysOn != 0;
    const int sx = width - kSide - 8 - 40;
    const int sy = box.y + box.h / 2 - 10;
    renderer.drawRect(sx, sy, 40, 20, 2, true);
    renderer.fillRect(on ? sx + 22 : sx + 3, sy + 3, 15, 14, true);
  }

  int y = hintY;
  for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, tr(STR_DICT_LANG_HINT), width - 2 * (kSide + 8), 3)) {
    renderer.drawText(SMALL_FONT_ID, kSide + 8, y, line.c_str());
    y += renderer.getLineHeight(SMALL_FONT_ID) + 2;
  }

  if (focusRow >= 0) {
    const gameui::Box& box = rowBoxes[focusRow];
    renderer.fillRect(kSide - 8, box.y + 10, 4, box.h - 20, true);
  }

  if (popup.isActive()) popup.render(renderer);
  renderer.displayBuffer();
}
