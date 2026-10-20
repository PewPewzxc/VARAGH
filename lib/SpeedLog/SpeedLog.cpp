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
    "font_hits,font_sd_reads,font_sd_ms,battery_mv,clock,note\n";
constexpr size_t BUFFER_BYTES = 4096;
constexpr size_t FLUSH_AT_BYTES = 2048;
constexpr uint32_t FLUSH_INTERVAL_MS = 60000;
// Past this size the log moves to speed-log.old.csv (one generation kept).
constexpr size_t MAX_LOG_BYTES = 512U * 1024U;
// Inputs older than this are not treated as the cause of a render.
constexpr uint32_t INPUT_LATENCY_WINDOW_MS = 3000;
// Longest name recordNamed() keeps.
constexpr size_t MAX_NAME_CHARS = 24;
constexpr size_t MAX_NOTE_CHARS = 96;

std::mutex mutex;
HeapByteBuffer buffer;
size_t used = 0;
uint32_t lastFlushMs = 0;
// Set while the card cannot be written (removed, full, or handed to the PC in
// USB Drive mode): retry once per FLUSH_INTERVAL_MS instead of every loop tick.
bool lastFlushFailed = false;
// The first write of a session (after its Start line) checks the file's
// header: a log started by an older build has fewer columns, so it moves to
// speed-log.old.csv.
bool headerChecked = false;
uint32_t droppedLines = 0;
Context context;
void* loopTaskHandle = nullptr;
void* renderTaskHandle = nullptr;
volatile uint32_t lastInputMs = 0;
volatile bool inputPending = false;
volatile Input lastInput = Input::Button;

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
    case Event::Start:
      return "start";
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
void appendLine(const char* name, const uint32_t now, const uint32_t durationMs, const int32_t a, const int32_t b,
                const char* note = "") {
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

  char clock[24];
  formatClock(state, now, clock, sizeof(clock));

  char line[352];
  const int n = snprintf(line, sizeof(line), "%lu,%s,%lu,%ld,%ld,%d,%d,%d,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%d,%s,%s\n",
                         static_cast<unsigned long>(now), name, static_cast<unsigned long>(durationMs),
                         static_cast<long>(a), static_cast<long>(b), state.batteryPercent, state.charging ? 1 : 0,
                         state.frontlightPercent, cpuMhz, heapFree, heapMin, psramFree, stackFree(loopTaskHandle),
                         stackFree(renderTaskHandle), static_cast<unsigned long>(font.hitReads),
                         static_cast<unsigned long>(font.sdFills), static_cast<unsigned long>(font.sdFillMs),
                         state.batteryMillivolts, clock, note);
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

  if (!headerChecked && Storage.exists(LOG_PATH)) {
    char head[sizeof(HEADER)] = {};
    HalFile existing = Storage.open(LOG_PATH, O_RDONLY);
    const bool readable = static_cast<bool>(existing);
    const size_t got = readable ? existing.read(head, sizeof(HEADER) - 1) : 0;
    if (readable) existing.close();
    if (readable && (got != sizeof(HEADER) - 1 || memcmp(head, HEADER, sizeof(HEADER) - 1) != 0)) {
      Storage.remove(OLD_LOG_PATH);
      Storage.rename(LOG_PATH, OLD_LOG_PATH);
    }
  }
  headerChecked = true;

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

// Days since 1970-01-01 for a civil date, and back (H. Hinnant's algorithms).
int32_t daysFromCivil(int32_t y, const unsigned m, const unsigned d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

void civilFromDays(int32_t z, int32_t& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = static_cast<int32_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y += m <= 2;
}

}  // namespace

void formatClock(const Context& context, const uint32_t nowMs, char* out, const size_t outSize) {
  if (outSize == 0) return;
  out[0] = '\0';
  if (!context.clockValid || context.month < 1 || context.month > 12 || context.day < 1 || context.day > 31) return;
  const int64_t minutes = static_cast<int64_t>(daysFromCivil(context.year, context.month, context.day)) * 1440 +
                          context.hour * 60 + context.minute + (nowMs - context.clockAtMs) / 60000U;
  int32_t y;
  unsigned m, d;
  civilFromDays(static_cast<int32_t>(minutes / 1440), y, m, d);
  const unsigned minuteOfDay = static_cast<unsigned>(minutes % 1440);
  snprintf(out, outSize, "%04ld-%02u-%02u %02u:%02u", static_cast<long>(y), m, d, minuteOfDay / 60, minuteOfDay % 60);
}

void setContext(const Context& value) {
  std::lock_guard<std::mutex> lock(mutex);
  context = value;
}

void setTasks(void* loopTask, void* renderTask) {
  loopTaskHandle = loopTask;
  renderTaskHandle = renderTask;
}

void noteInput(const Input input) {
  lastInputMs = millis();
  lastInput = input;
  inputPending = true;
}

void record(const Event event, const uint32_t durationMs, int32_t a, const int32_t b, const char* note) {
  const uint32_t now = millis();
  char text[MAX_NOTE_CHARS + 1] = "";
  if (event == Event::Page) {
    // Input-to-render delay, measured once per input.
    const uint32_t sinceInput = now - durationMs - lastInputMs;
    const bool fromInput = inputPending && sinceInput < INPUT_LATENCY_WINDOW_MS;
    a = fromInput ? static_cast<int32_t>(sinceInput) : -1;
    inputPending = false;
    if (fromInput) {
      const Input input = lastInput;
      snprintf(text, sizeof(text), "in=%s%s", input == Input::Touch ? "touch" : input == Input::Tilt ? "tilt" : "button",
               note != nullptr && note[0] != '\0' ? " " : "");
    }
  }
  if (note != nullptr) {
    const size_t used = strlen(text);
    snprintf(text + used, sizeof(text) - used, "%s", note);
  }
  for (char* c = text; *c != '\0'; ++c) {
    if (*c == ',' || *c == '\n' || *c == '\r') *c = '_';
  }
  appendLine(eventName(event), now, durationMs, a, b, text);
}

void recordNote(const Event event, const char* note, const uint32_t durationMs, const int32_t a, const int32_t b) {
  char safe[MAX_NOTE_CHARS + 1];
  size_t n = 0;
  for (; note != nullptr && note[n] != '\0' && n < MAX_NOTE_CHARS; ++n) {
    const char c = note[n];
    safe[n] = (c == ',' || c == '\n' || c == '\r') ? '_' : c;
  }
  safe[n] = '\0';
  if (event == Event::Start) {
    // A new session: its first write checks the log's header again.
    std::lock_guard<std::mutex> lock(mutex);
    headerChecked = false;
  }
  appendLine(eventName(event), millis(), durationMs, a, b, safe);
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
