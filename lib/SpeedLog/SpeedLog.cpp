#include "SpeedLog.h"

#include <Arduino.h>
#include <FontFileMirror.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define SPEEDLOG_HAS_ESP32 1
#else
#define SPEEDLOG_HAS_ESP32 0
#endif

namespace SpeedLog {

namespace {

constexpr char LOG_PATH[] = "/.crosspoint/speed-log.csv";
constexpr char OLD_LOG_PATH[] = "/.crosspoint/speed-log.old.csv";
constexpr char HEADER[] =
    "t_ms,event,ms,a,b,battery,charging,light,cpu_mhz,heap_free,heap_min,psram_free,loop_stack,render_stack,"
    "font_hits,font_sd_reads,font_sd_ms\n";
constexpr size_t BUFFER_BYTES = 4096;
constexpr size_t FLUSH_AT_BYTES = 2048;
constexpr uint32_t FLUSH_INTERVAL_MS = 60000;
// Past this size the log moves to speed-log.old.csv (one generation kept).
constexpr size_t MAX_LOG_BYTES = 512U * 1024U;
// Inputs older than this are not treated as the cause of a render.
constexpr uint32_t INPUT_LATENCY_WINDOW_MS = 3000;
// Longest name recordNamed() keeps.
constexpr size_t MAX_NAME_CHARS = 24;

std::mutex mutex;
HeapByteBuffer buffer;
size_t used = 0;
uint32_t lastFlushMs = 0;
// Set while the card cannot be written (removed, full, or handed to the PC in
// USB Drive mode): retry once per FLUSH_INTERVAL_MS instead of every loop tick.
bool lastFlushFailed = false;
uint32_t droppedLines = 0;
Context context;
void* loopTaskHandle = nullptr;
void* renderTaskHandle = nullptr;
volatile uint32_t lastInputMs = 0;
volatile bool inputPending = false;

const char* eventName(const Event event) {
  switch (event) {
    case Event::Boot:
      return "boot";
    case Event::Open:
      return "open";
    case Event::Page:
      return "page";
    case Event::Index:
      return "index";
    case Event::Image:
      return "image";
    case Event::Battery:
      return "battery";
    case Event::Sleep:
      return "sleep";
  }
  return "?";
}

unsigned stackFree(void* task) {
#if SPEEDLOG_HAS_ESP32
  return task ? static_cast<unsigned>(uxTaskGetStackHighWaterMark(static_cast<TaskHandle_t>(task))) : 0;
#else
  (void)task;
  return 0;
#endif
}

// Formats one line (device state columns included) into the RAM buffer.
void appendLine(const char* name, const uint32_t now, const uint32_t durationMs, const int32_t a, const int32_t b) {
#if SPEEDLOG_HAS_ESP32
  const unsigned cpuMhz = getCpuFrequencyMhz();
  const unsigned heapFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const unsigned heapMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const unsigned psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
#else
  const unsigned cpuMhz = 0, heapFree = 0, heapMin = 0, psramFree = 0;
#endif
  const auto& font = FontFileMirror::globalStats();

  Context state;
  {
    std::lock_guard<std::mutex> lock(mutex);
    state = context;
  }

  char line[224];
  const int n = snprintf(line, sizeof(line), "%lu,%s,%lu,%ld,%ld,%d,%d,%d,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu\n",
                         static_cast<unsigned long>(now), name, static_cast<unsigned long>(durationMs),
                         static_cast<long>(a), static_cast<long>(b), state.batteryPercent, state.charging ? 1 : 0,
                         state.frontlightPercent, cpuMhz, heapFree, heapMin, psramFree, stackFree(loopTaskHandle),
                         stackFree(renderTaskHandle), static_cast<unsigned long>(font.hitReads),
                         static_cast<unsigned long>(font.sdFills), static_cast<unsigned long>(font.sdFillMs));
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(line)) return;

  std::lock_guard<std::mutex> lock(mutex);
  if (!buffer) {
    buffer = psramHeapAvailable() ? makePsramByteBufferNoThrow(BUFFER_BYTES) : HeapByteBuffer{};
    if (!buffer) buffer = makeDefaultByteBufferNoThrow(BUFFER_BYTES);
    if (!buffer) return;
  }
  if (used + static_cast<size_t>(n) > BUFFER_BYTES) {
    droppedLines++;
    return;
  }
  memcpy(buffer.get() + used, line, static_cast<size_t>(n));
  used += static_cast<size_t>(n);
}

// Buffered lines to the card; the caller holds `mutex`.
void flushLocked() {
  lastFlushMs = millis();
  if (used == 0 || !buffer) return;

  HalFile file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
  if (!file) {
    if (!lastFlushFailed) LOG_ERR("SPD", "Cannot open %s; retrying every minute", LOG_PATH);
    lastFlushFailed = true;
    return;
  }
  size_t size = file.size();
  if (size > MAX_LOG_BYTES) {
    file.close();
    Storage.remove(OLD_LOG_PATH);
    Storage.rename(LOG_PATH, OLD_LOG_PATH);
    file = Storage.open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND);
    if (!file) {
      LOG_ERR("SPD", "Cannot reopen %s after rotation", LOG_PATH);
      lastFlushFailed = true;
      return;
    }
    size = 0;
  }
  lastFlushFailed = false;
  if (size == 0) file.write(HEADER, sizeof(HEADER) - 1);
  if (droppedLines != 0) {
    char note[64];
    const int n = snprintf(note, sizeof(note), "%lu,dropped,%lu\n", static_cast<unsigned long>(millis()),
                           static_cast<unsigned long>(droppedLines));
    if (n > 0 && static_cast<size_t>(n) < sizeof(note)) file.write(note, static_cast<size_t>(n));
    droppedLines = 0;
  }
  if (file.write(buffer.get(), used) != used) {
    LOG_ERR("SPD", "Short write to %s", LOG_PATH);
  }
  file.close();
  used = 0;
}

}  // namespace

void setContext(const Context& value) {
  std::lock_guard<std::mutex> lock(mutex);
  context = value;
}

void setTasks(void* loopTask, void* renderTask) {
  loopTaskHandle = loopTask;
  renderTaskHandle = renderTask;
}

void noteInput() {
  lastInputMs = millis();
  inputPending = true;
}

void record(const Event event, const uint32_t durationMs, int32_t a, const int32_t b) {
  const uint32_t now = millis();
  if (event == Event::Page) {
    // Input-to-render delay, measured once per input.
    const uint32_t sinceInput = now - durationMs - lastInputMs;
    a = (inputPending && sinceInput < INPUT_LATENCY_WINDOW_MS) ? static_cast<int32_t>(sinceInput) : -1;
    inputPending = false;
  }
  appendLine(eventName(event), now, durationMs, a, b);
}

void recordNamed(const char* name, const uint32_t durationMs, const int32_t a, const int32_t b) {
  // Keep the column parseable: short, no separators.
  char safe[MAX_NAME_CHARS + 1];
  size_t n = 0;
  for (; name != nullptr && name[n] != '\0' && n < MAX_NAME_CHARS; ++n) {
    const char c = name[n];
    safe[n] = (c == ',' || c == '\n' || c == '\r') ? '_' : c;
  }
  safe[n] = '\0';
  appendLine(n != 0 ? safe : "?", millis(), durationMs, a, b);
}

bool flushDue() {
  std::lock_guard<std::mutex> lock(mutex);
  if (used == 0) return false;
  const bool intervalElapsed = millis() - lastFlushMs >= FLUSH_INTERVAL_MS;
  if (lastFlushFailed) return intervalElapsed;
  return used >= FLUSH_AT_BYTES || intervalElapsed;
}

void flush() {
  std::lock_guard<std::mutex> lock(mutex);
  flushLocked();
}

}  // namespace SpeedLog
