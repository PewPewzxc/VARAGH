#include "OnlineWifi.h"

#include <Arduino.h>
#include <HalPowerManager.h>
#include <Logging.h>
#include <WiFi.h>

#include <string>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"
#include "network/WifiUtils.h"

namespace {

// Joining Wi-Fi takes roughly this much of the internal heap; below it the
// reader is better left alone and the lookup takes the restart route.
constexpr uint32_t kFreeHeapNeeded = 96 * 1024;
constexpr uint32_t kRetryEveryMs = 45000;
// Opening a book is the reader's busiest moment for both the processor and the
// heap; Wi-Fi waits until the first page has been shown.
constexpr uint32_t kJoinAfterOpenMs = 4000;

bool readerOpen = false;
bool holding = false;  // Wi-Fi was switched on by this module
bool attempted = false;
uint32_t openedMs = 0;
uint32_t lastAttemptMs = 0;

bool join() {
  attempted = true;
  lastAttemptMs = millis();
  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) {
    LOG_DBG("OWIFI", "No saved network to join");
    return false;
  }
  if (!holding && ESP.getFreeHeap() < kFreeHeapNeeded) {
    LOG_ERR("OWIFI", "Too little memory for Wi-Fi next to the book (free %u)", ESP.getFreeHeap());
    return false;
  }
  const auto credential = WIFI_STORE.findCredential(ssid);
  WiFi.persistent(false);  // credentials stay in WifiCredentialStore
  if (!WiFi.mode(WIFI_STA)) {
    LOG_ERR("OWIFI", "Could not switch Wi-Fi on");
    return false;
  }
  holding = true;
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  const String hostname = "VARAGH-" + mac;
  WiFi.setHostname(hostname.c_str());
  // The radio dozes between the access point's beacons while nothing is sent.
  WiFi.setSleep(true);
  if (credential && !credential->password.empty()) {
    WiFi.begin(ssid.c_str(), credential->password.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }
  // With Wi-Fi only idling, the processor may still drop to its low clock.
  powerManager.setWifiIdleLowPowerAllowed(true);
  LOG_INF("OWIFI", "Joining %s in the background (free heap %u)", ssid.c_str(), ESP.getFreeHeap());
  return true;
}

void release() {
  if (!holding) return;
  holding = false;
  powerManager.setWifiIdleLowPowerAllowed(false);
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(50);
    WiFi.mode(WIFI_OFF);
    delay(50);
  }
  LOG_INF("OWIFI", "Wi-Fi off");
}

}  // namespace

namespace OnlineWifi {

bool enabled() {
  // Only where the low clock still carries Wi-Fi and PSRAM leaves the heap free.
  if constexpr (HalPowerManager::LOW_POWER_FREQ < 80) return false;
  return SETTINGS.onlineDictWifiAlwaysOn != 0;
}

void readerOpened() {
  readerOpen = true;
  attempted = false;
  openedMs = millis();
}

void readerClosed() {
  readerOpen = false;
  release();
}

void tick() {
  if (!enabled()) {
    release();
    return;
  }
  // Once a second is plenty; the reader's loop runs far more often.
  static uint32_t lastTickMs = 0;
  if (!readerOpen || millis() - lastTickMs < 1000) return;
  lastTickMs = millis();
  if (hasActiveStationWifiConnection()) return;
  if (!attempted) {
    if (millis() - openedMs >= kJoinAfterOpenMs) join();
    return;
  }
  // Not connected yet, or dropped (out of range, another feature switched the
  // radio off): try again now and then.
  if (millis() - lastAttemptMs >= kRetryEveryMs) join();
}

bool waitConnected(const uint32_t timeoutMs) {
  if (!enabled()) return false;
  if (hasActiveStationWifiConnection()) return true;
  if ((!holding || WiFi.getMode() == WIFI_MODE_NULL || millis() - lastAttemptMs > 8000) && !join()) return false;
  const uint32_t started = millis();
  while (millis() - started < timeoutMs) {
    if (hasActiveStationWifiConnection()) return true;
    delay(100);
  }
  LOG_ERR("OWIFI", "Wi-Fi did not connect in %lu ms", static_cast<unsigned long>(timeoutMs));
  return false;
}

}  // namespace OnlineWifi
