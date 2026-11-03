#include "ClockNetSync.h"

#include <Arduino.h>
#include <HalClock.h>
#include <Logging.h>

#include <string>

#include "CrossPointSettings.h"
#include "network/HttpDownloader.h"

namespace clocksync {

namespace {

constexpr unsigned long kResyncGapMs = 60UL * 60UL * 1000UL;
unsigned long lastSyncMs = 0;
bool syncedThisBoot = false;

bool fetchOffsetMinutes(int& minutes) {
  // Two free services that place the network's address in a time zone; the
  // second is only asked when the first fails.
  static constexpr const char* kUrls[] = {"http://ip-api.com/line/?fields=offset", "https://ipapi.co/utc_offset/"};
  for (const char* url : kUrls) {
    std::string body;
    if (HttpDownloader::fetchUrl(url, body) && parseUtcOffsetMinutes(body.c_str(), minutes)) return true;
    LOG_ERR("CLK", "Time zone lookup failed: %s", url);
  }
  return false;
}

}  // namespace

bool syncNow() {
  if (!halClock.syncFromNTP()) return false;
  SETTINGS.clockHasBeenSynced = 1;
  SETTINGS.clockDateHasBeenSynced = 1;
  lastSyncMs = millis();
  syncedThisBoot = true;

  int minutes = 0;
  if (SETTINGS.clockAutoZone && fetchOffsetMinutes(minutes)) {
    // Quarter-hour steps biased by 48 (see CrossPointSettings::clockUtcOffsetQ).
    const int q = (minutes >= 0 ? minutes + 7 : minutes - 7) / 15 + 48;
    SETTINGS.clockUtcOffsetQ = static_cast<uint8_t>(q < 0 ? 0 : q > 104 ? 104 : q);
    // The looked-up offset already includes summer time.
    SETTINGS.clockDstRule = 0;
    LOG_INF("CLK", "Time zone set to UTC%+d:%02d", minutes / 60, abs(minutes % 60));
  }
  SETTINGS.saveToFile();
  return true;
}

void onWifiConnected() {
  if (!halClock.isAvailable()) return;
  if (syncedThisBoot && millis() - lastSyncMs < kResyncGapMs) return;
  syncNow();
}

}  // namespace clocksync
