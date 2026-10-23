#include "BlePageTurnerGlue.h"

#include <Arduino.h>
#include <BlePageTurner.h>
#include <Epub/Section.h>
#include <FontCacheManager.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <WiFi.h>

#include <atomic>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/Activity.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "activities/reader/ChapterBuildGate.h"

#if !defined(SIMULATOR)
#include <esp_heap_caps.h>
#endif

extern MappedInputManager mappedInputManager;
extern FontCacheManager fontCacheManager;

namespace blepager {
namespace {
std::atomic<bool> cpuHeld{false};
std::atomic<bool> releasedForBuild{false};
bleturner::Action pendingAction = bleturner::Action::None;  // main loop only
#if !defined(SIMULATOR)
TaskHandle_t mainTask = nullptr;
#endif
constexpr uint32_t BUILD_ROOM_WAIT_MS = 5000;
constexpr uint32_t BUILD_ROOM_STEP_MS = 20;

bleturner::Heap heap() {
#if defined(SIMULATOR)
  return {ESP.getFreeHeap(), ESP.getMaxAllocHeap()};
#else
  return {heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
          heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)};
#endif
}

// Runs on the radio's start task. Only TRIES the render lock: the render task can hold it through
// a chapter build that is waiting for this very radio.
bool releaseCaches() {
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return false;
  fontCacheManager.releaseSdFontCaches();
  return true;
}

// Main loop (inside tick): a press is taken when the reader's page is on screen and none waits.
bool deliver(const bleturner::Action action) {
  if (action == bleturner::Action::None || action == bleturner::Action::SaveQuote) return false;
  if (pendingAction != bleturner::Action::None || !activityManager.isForegroundReaderShown()) return false;
  pendingAction = action;
  return true;
}

bool yieldForRadio() { return true; }        // the reader keeps no parser alive between pages
bool fileTransferActive() { return false; }  // file transfer is a screen of its own: the radio is off there
void restartIntoBook() { silentRestartToReader(); }

void holdFullSpeed(const bool hold) {
  cpuHeld.store(hold);
  if (hold) powerManager.setPowerSaving(false);
}

void log(const bool error, const char* format, va_list args) {
#ifdef ENABLE_SERIAL_LOG
  char line[160];
  vsnprintf(line, sizeof(line), format, args);
  if (error) {
    LOG_ERR("BLE", "%s", line);
  } else {
    LOG_INF("BLE", "%s", line);
  }
#else
  (void)error;
  (void)format;
  (void)args;
#endif
}

const bleturner::Host host{heap,          releaseCaches, deliver, yieldForRadio, fileTransferActive, restartIntoBook,
                           holdFullSpeed, log,           nullptr};
}  // namespace

void begin() {
#if !defined(SIMULATOR)
  mainTask = xTaskGetCurrentTaskHandle();
#endif
  bleturner::begin(host, SETTINGS.ble);
  Section::setBuildGate(makeRoomForChapterBuild);
}

bool tick() {
  bleturner::Scene scene{};
  const bool reader = activityManager.isForegroundReaderShown();
  const bool home = activityManager.isForegroundHomeShown();
  scene.where = activityManager.isForegroundBleSettings() ? bleturner::Where::PairingScreen
                : reader                                 ? bleturner::Where::Reader
                : home                                   ? bleturner::Where::Home
                                                         : bleturner::Where::Elsewhere;
  scene.visit = activityManager.visit();
  scene.pageShown = reader || home;
  scene.storageBusy = activityManager.requiresExclusiveStorageLoop();
  scene.wifiOn = WiFi.getMode() != WIFI_MODE_NULL;
  return bleturner::tick(scene);
}

void applyRemoteInput() {
  const bleturner::Action action = pendingAction;
  pendingAction = bleturner::Action::None;
  switch (action) {
    case bleturner::Action::NextPage:
      mappedInputManager.injectRelease(MappedInputManager::Button::Right);
      bleturner::acknowledgeLinkNote();
      break;
    case bleturner::Action::PrevPage:
      mappedInputManager.injectRelease(MappedInputManager::Button::Left);
      bleturner::acknowledgeLinkNote();
      break;
    case bleturner::Action::Confirm:
      mappedInputManager.injectRelease(MappedInputManager::Button::Confirm);
      break;
    case bleturner::Action::Back:
      mappedInputManager.injectRelease(MappedInputManager::Button::Back);
      break;
    case bleturner::Action::NextChapter:
    case bleturner::Action::PrevChapter:
      activityManager.remoteChapter(action == bleturner::Action::NextChapter);
      bleturner::acknowledgeLinkNote();
      break;
    case bleturner::Action::ReaderMenu:
      activityManager.openReaderMenuFromShortcut();
      break;
    case bleturner::Action::SaveQuote:
    case bleturner::Action::None:
      break;
  }
}

bool beforeSleep(const unsigned timeoutMs) { return bleturner::beforeSleep(timeoutMs); }

bool holdsCpu() { return cpuHeld.load(); }

bool makeRoomForChapterBuild() {
  if (!bleturner::status().compiledIn) return true;
#if !defined(SIMULATOR)
  // A build started from the main loop: the stop finishes on this very loop, so stop it now.
  if (xTaskGetCurrentTaskHandle() == mainTask) return bleturner::stopNow(3000);
#endif
  // Render task, under its lock (CrossPoint's makeRoomForChapterBuild): the first ask stops the
  // radio; a later one asks again only once a start that was still settling left the radio up.
  bool asked = false;
  const auto roomMade = [&asked] {
    if (!releasedForBuild.load() && (!asked || !bleturner::status().starting)) {
      releasedForBuild = bleturner::beforeChapterBuild() != bleturner::BuildRelease::NotHeld;
    }
    asked = true;
    return chapterBuildMayRun(bleturner::status());
  };
  const auto nowMs = [] { return static_cast<uint32_t>(millis()); };
  const auto nap = [](const uint32_t ms) { delay(ms); };
  if (waitForBuildRoom(roomMade, nowMs, nap, BUILD_ROOM_WAIT_MS, BUILD_ROOM_STEP_MS)) return true;
  LOG_ERR("BLE", "Radio still holds the heap after %u ms; chapter build refused",
          static_cast<unsigned>(BUILD_ROOM_WAIT_MS));
  return false;
}

void afterPagePaint() {
  if (releasedForBuild.exchange(false)) bleturner::afterPaint();
}

bool holdsHeap() { return bleturner::holdsHeap(); }

}  // namespace blepager
