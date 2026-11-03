#pragma once

#include <cstdlib>

// Internet clock sync: time from NTP, and (Automatic Time Zone on) the UTC
// offset from the network's location, summer time included. Needs Wi-Fi.
namespace clocksync {

// Sets the clock now. False when the time could not be fetched.
bool syncNow();

// Called after every Wi-Fi connect; re-syncs at most once an hour per boot.
void onWifiConnected();

// "+0330" / "-0500" (ipapi.co) or seconds such as "12600" (ip-api.com) -> minutes.
inline bool parseUtcOffsetMinutes(const char* text, int& minutes) {
  if (!text) return false;
  while (*text == ' ' || *text == '\n' || *text == '\r') ++text;
  const bool hhmm = (text[0] == '+' || text[0] == '-') && text[1] && text[2] && text[3] && text[4] &&
                    (text[5] == '\0' || text[5] == '\n' || text[5] == '\r');
  char* end = nullptr;
  const long value = strtol(text, &end, 10);
  if (end == text) return false;
  if (*end != '\0' && *end != '\n' && *end != '\r') return false;
  const long total = hhmm ? (value / 100) * 60 + (value % 100) : value / 60;
  if (total < -12 * 60 || total > 14 * 60) return false;
  minutes = static_cast<int>(total);
  return true;
}

}  // namespace clocksync
