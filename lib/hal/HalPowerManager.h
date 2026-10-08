#pragma once

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <InputManager.h>
#include <Logging.h>
#include <Wire.h>
#include <freertos/semphr.h>

#include <cassert>

#include "HalGPIO.h"

// Opt-in battery telemetry for diagnosing a miscalibrated fuel gauge. Off in
// every shipping environment; the `debug` PlatformIO env sets it to 1. Defined
// here because this is the lowest layer that consumes it.
#ifndef CROSSINK_BATTERY_DIAG_LOG
#define CROSSINK_BATTERY_DIAG_LOG 0
#endif

class HalPowerManager;
extern HalPowerManager powerManager;  // Singleton

class HalPowerManager {
  int normalFreq = 0;  // MHz
  bool isLowPower = false;
  bool panelWaitLowered = false;  // CPU lowered by beginPanelWait(), not setPowerSaving()
  // VARAGH: set while Wi-Fi is only kept associated in the background (the
  // online dictionary's "keep Wi-Fi on"), not carrying a transfer.
  bool wifiIdleLowPowerAllowed = false;
  // Wi-Fi needs the 80 MHz bus clock, which the S3 boards keep at their low
  // CPU clock. The C3's 10 MHz low clock cannot carry it.
  bool wifiBlocksLowPower() const;

  mutable int _batteryCachedPercent = 0;  // Last read battery percentage * 10 (0-1000); callers divide by 10 (ADC/X4
                                          // path only — I2C/X3 path stores 0-100 directly)
  mutable unsigned long _batteryLastPollMs = 0;  // Timestamp of last battery read in milliseconds

  enum LockMode { None, NormalSpeed };
  LockMode currentLockMode = None;
  SemaphoreHandle_t modeMutex = nullptr;  // Protect access to currentLockMode

 public:
#if defined(BOARD_HAS_PSRAM)
  static constexpr int LOW_POWER_FREQ = 80;  // MHz
#else
  static constexpr int LOW_POWER_FREQ = 10;  // MHz
#endif
  // Drop to the low-power clock this long after the last input. Instant input
  // wake restores full speed before an input is handled, so the window only
  // needs to cover short bursts of taps (it was 3000 ms before input wake).
  static constexpr unsigned long IDLE_POWER_SAVING_MS = 1000;  // ms
  static constexpr unsigned long BATTERY_POLL_MS = 1500;       // ms

  void begin();
  // VARAGH: lets the idle clock drop while Wi-Fi is merely kept associated.
  // Every other Wi-Fi user leaves this off and keeps the full clock.
  void setWifiIdleLowPowerAllowed(bool allowed) { wifiIdleLowPowerAllowed = allowed; }

  // Control CPU frequency for power saving
  void setPowerSaving(bool enabled);

  // Panel-refresh waits. The display driver calls these around long BUSY
  // waits, when a render has handed the frame to the panel and only waits for
  // its waveform. The CPU drops to LOW_POWER_FREQ for that window even while a
  // render Lock is held, then returns to the speed it had; the refresh itself
  // takes exactly as long either way. S3 boards only (LOW_POWER_FREQ >= 80,
  // so the APB peripheral clock is unchanged); elsewhere these do nothing.
  void beginPanelWait();
  void endPanelWait();

  // Setup wake up GPIO and enter deep sleep
  // Should be called inside main loop() to handle the currentLockMode
  void startDeepSleep(HalGPIO& gpio) const;

  // Get battery percentage (range 0-100)
  uint16_t getBatteryPercentage() const;

#if CROSSINK_BATTERY_DIAG_LOG
  // Raw battery telemetry for the diagnostic log, kept behind the flag so
  // shipping builds carry neither the struct nor the extra I2C traffic.
  struct BatteryDiagnostics {
    uint16_t soc = 0;         // 0-100, as reported by the backend
    uint16_t millivolts = 0;  // cell voltage; gauge boards report it directly
    bool charging = false;
    // Each field can fail independently, so a valid zero stays distinguishable
    // from an unread one.
    bool socKnown = false;
    bool millivoltsKnown = false;
    bool chargingKnown = false;
  };

  // Samples the battery backend directly, bypassing getBatteryPercentage()'s
  // poll cache and smoothing so every row is a fresh read. Returns false when
  // the active board reports no battery telemetry at all.
  bool getBatteryDiagnostics(BatteryDiagnostics& out) const;
#endif

  // RAII helper class to manage power saving locks
  // Usage: create an instance of Lock in a scope to disable power saving, for example when running a task that needs
  // full performance. When the Lock instance is destroyed (goes out of scope), power saving will be re-enabled.
  class Lock {
    friend class HalPowerManager;
    bool valid = false;

   public:
    explicit Lock();
    ~Lock();

    // Non-copyable and non-movable
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
    Lock(Lock&&) = delete;
    Lock& operator=(Lock&&) = delete;
  };
};
