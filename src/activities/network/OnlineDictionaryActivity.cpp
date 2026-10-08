#include "OnlineDictionaryActivity.h"

#include <Arduino.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <memory>

#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "WifiSelectionActivity.h"
#include "PendingOverlayResume.h"
#include "activities/apps/GameThinker.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/OnlineWifi.h"
#include "network/WifiPowerSaveGuard.h"
#include "network/WifiUtils.h"
#include "util/Dictionary.h"

namespace {

constexpr int kButtonWidth = 260;
constexpr int kButtonHeight = 56;

Rect leaveButtonRect(const GfxRenderer& renderer) {
  return Rect{(renderer.getScreenWidth() - kButtonWidth) / 2, renderer.getScreenHeight() * 2 / 3, kButtonWidth,
              kButtonHeight};
}

void wifiOff() {
  WiFi.disconnect(false);
  delay(100);
  WiFi.mode(WIFI_OFF);
  delay(100);
}

// --- the lookup with Wi-Fi kept on: no restart, the answer is fetched with the book open --------
struct InPlaceJob {
  onlinedict::Lang lang = onlinedict::Lang::English;
  std::string word;
  std::string html;
  OnlineDictionary::Outcome outcome = OnlineDictionary::Outcome::NoConnection;
};

void fetchInPlace(void* context) {
  auto* job = static_cast<InPlaceJob*>(context);
  // The radio at full power for the request, dozing again afterwards.
  const WifiPowerSaveGuard fullPower;
  job->outcome = OnlineDictionary::fetch(job->lang, job->word, job->html);
}

enum class InPlace : uint8_t { Found, NoEntry, Unavailable };

InPlace lookUpInPlace(const GfxRenderer& renderer, const OnlineDictionary::Request& request) {
  GUI.drawPopup(renderer, tr(STR_ONLINE_LOOKING_UP));
  if (!OnlineWifi::waitConnected(12000)) return InPlace::Unavailable;
  // The secure connection and the task below need this much of the heap.
  if (ESP.getFreeHeap() < 72 * 1024) {
    LOG_ERR("ODICT", "Too little memory for a lookup with the book open (free %u)", ESP.getFreeHeap());
    return InPlace::Unavailable;
  }

  // The secure connection needs more stack than the main task has to spare
  // with a book open, so the request runs on a task of its own.
  InPlaceJob job;
  job.lang = request.lang;
  job.word = request.word;
  GameThinker worker;
  const uint32_t started = millis();
  if (!worker.start(&fetchInPlace, &job, 0)) return InPlace::Unavailable;
  while (worker.running()) delay(10);
  worker.takeFinished();
  LOG_INF("ODICT", "In-place lookup took %lu ms (free heap %u)", static_cast<unsigned long>(millis() - started),
          ESP.getFreeHeap());

  if (job.outcome == OnlineDictionary::Outcome::NoConnection) return InPlace::Unavailable;
  if (job.outcome != OnlineDictionary::Outcome::Found) {
    GUI.drawPopup(renderer, tr(STR_ONLINE_NO_ENTRY));
    delay(1800);
    return InPlace::NoEntry;
  }
  if (!OnlineDictionary::save(request.lang, job.word, job.html)) return InPlace::Unavailable;
  // The entry may have been found under another spelling; the reader searches for the one it asked for.
  if (job.word != request.word) OnlineDictionary::save(request.lang, request.word, job.html);
  return InPlace::Found;
}

}  // namespace

OnlineDictionaryActivity::OnlineDictionaryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   const Mode mode)
    : Activity("OnlineDictionary", renderer, mappedInput), mode(mode) {}

std::string OnlineDictionaryActivity::begin(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                            const std::string& word, const std::string& bookCachePath) {
  OnlineDictionary::Request request;
  request.word = onlinedict::normalizeWord(word);
  if (request.word.empty()) return {};
  // The dictionary chosen for this book tells the language of its words.
  const std::string dictionaryPath = Dictionary::readConfiguredDictPath(bookCachePath.c_str());
  const DictInfo info = dictionaryPath.empty() ? DictInfo{} : Dictionary::readInfo(dictionaryPath.c_str());
  request.lang = onlinedict::languageFor(request.word, info.valid ? info.lang : nullptr);
  if (OnlineDictionary::hasSaved(request.lang, request.word)) return OnlineDictionary::storePath(request.lang);

  if (OnlineWifi::enabled()) {
    switch (lookUpInPlace(renderer, request)) {
      case InPlace::Found:
        return OnlineDictionary::storePath(request.lang);
      case InPlace::NoEntry:
        return {};
      case InPlace::Unavailable:
        // No Wi-Fi in reach of the book: the restart route below can ask for a network.
        break;
    }
  }

  if (!OnlineDictionary::saveRequest(request)) {
    LOG_ERR("ODICT", "Could not write the lookup request");
    return {};
  }
  PendingOverlayResume resume;
  resume.origin = PendingOverlayOrigin::Reader;
  resume.overlay = PendingOverlayType::OnlineDefinition;
  resume.bookPath = APP_STATE.openEpubPath;
  APP_STATE.setPendingOverlayResume(std::move(resume));
  activityManager.replaceActivity(std::make_unique<OnlineDictionaryActivity>(renderer, mappedInput, Mode::HandOff));
  return {};
}

void OnlineDictionaryActivity::onEnter() {
  Activity::onEnter();

  if (mode == Mode::HandOff) {
    // The reader has closed and saved its place; continue in the network boot.
    silentRestartToNetwork(NetworkBootTarget::ONLINE_DICTIONARY);
    return;
  }

  if (!OnlineDictionary::loadRequest(request) || request.answered) {
    LOG_ERR("ODICT", "Network boot without a word to look up");
    OnlineDictionary::clearRequest();
    leave();
    return;
  }

  state = State::Connecting;
  requestUpdate();
  if (hasActiveStationWifiConnection()) {
    onWifiResult(true);
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput, true, false),
                         [this](const ActivityResult& result) { onWifiResult(!result.isCancelled); });
}

void OnlineDictionaryActivity::onWifiResult(const bool connected) {
  if (!connected) {
    OnlineDictionary::clearRequest();
    leave();
    return;
  }
  WiFi.setSleep(false);
  state = State::Fetching;
  requestUpdate();
}

void OnlineDictionaryActivity::fetch() {
  std::string html;
  std::string word = request.word;
  const OnlineDictionary::Outcome outcome = OnlineDictionary::fetch(request.lang, word, html);
  if (outcome == OnlineDictionary::Outcome::Found && OnlineDictionary::save(request.lang, word, html)) {
    // The reader picks the answer up from here once the book is open again.
    request.word = word;
    request.answered = true;
    if (OnlineDictionary::saveRequest(request)) {
      leave();
      return;
    }
  }
  OnlineDictionary::clearRequest();
  state = outcome == OnlineDictionary::Outcome::NoConnection ? State::NoConnection : State::NotFound;
  requestUpdate();
}

void OnlineDictionaryActivity::leave() {
  state = State::Leaving;
  wifiOff();
  if (APP_STATE.openEpubPath.empty()) {
    silentRestart();
  } else {
    silentRestartToReader(true);
  }
}

void OnlineDictionaryActivity::loop() {
  if (state == State::Fetching && !fetchStarted) {
    fetchStarted = true;
    // Put "Looking up" on the panel before the request holds this task.
    requestUpdateAndWait();
    fetch();
    return;
  }

  if (state != State::NotFound && state != State::NoConnection) return;
  const Rect button = leaveButtonRect(renderer);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasTapInRect(button.x, button.y, button.width, button.height)) {
    leave();
  }
}

void OnlineDictionaryActivity::render(RenderLock&&) {
  if (mode == Mode::HandOff || state == State::Starting || state == State::Leaving) return;

  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, width, metrics.headerHeight}, tr(STR_ONLINE_DICTIONARY));

  const char* message = "";
  switch (state) {
    case State::Connecting:
      message = tr(STR_ONLINE_CONNECTING);
      break;
    case State::Fetching:
      message = tr(STR_ONLINE_LOOKING_UP);
      break;
    case State::NotFound:
      message = tr(STR_ONLINE_NOT_FOUND);
      break;
    case State::NoConnection:
      message = tr(STR_ONLINE_NO_CONNECTION);
      break;
    case State::Starting:
    case State::Leaving:
      break;
  }

  const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  int y = renderer.getScreenHeight() / 3;
  renderer.drawCenteredText(UI_12_FONT_ID, y,
                            renderer.truncatedText(UI_12_FONT_ID, request.word.c_str(), width - 40).c_str(), true,
                            EpdFontFamily::BOLD);
  y += lineHeight * 2;
  for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, message, width - 60, 4)) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str());
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }

  if (state == State::NotFound || state == State::NoConnection) {
    const Rect button = leaveButtonRect(renderer);
    renderer.fillRoundedRect(button.x, button.y, button.width, button.height, 8, Color::Black);
    const char* label = tr(STR_ONLINE_BACK_TO_BOOK);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, label, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, button.x + (button.width - labelWidth) / 2,
                      button.y + (button.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2, label, false,
                      EpdFontFamily::BOLD);
  }

  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
