#pragma once

#include <cstddef>
#include <cstdint>

// Tiger speed log. A few numbers per page turn, chapter index, image decode
// and book open, plus a battery sample every few minutes, appended as CSV to
// /.crosspoint/speed-log.csv so real-device timings can be compared between
// builds. It records timings and device state only: no titles, paths or text.
//
// record() only formats one line into a small RAM buffer and may be called
// from any task. The main loop calls flush() while the reader is idle, so
// logging adds no SD traffic to a page turn.
namespace SpeedLog {

enum class Event : uint8_t { Boot, Open, Page, Index, Image, Battery, Sleep, Start };

// Device state written with every line. The main loop refreshes it (battery
// and frontlight reads use the shared I2C bus, so other tasks never query
// them); record() copies the latest snapshot.
struct Context {
  int batteryPercent = -1;
  int batteryMillivolts = -1;  // -1 when the board cannot read it
  bool charging = false;
  int frontlightPercent = -1;  // -1 when the board has no frontlight
  // Local date and time when the context was refreshed, to the minute, and
  // millis() at that moment. Lines written later add the time since then.
  // clockValid is false until the clock has been set (over Wi-Fi).
  bool clockValid = false;
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0;
  uint32_t clockAtMs = 0;
};
void setContext(const Context& context);

// Tasks whose remaining stack headroom is logged with every line.
void setTasks(void* loopTask, void* renderTask);

// Mark a user input, so the next Page line can report input-to-render delay
// and, in its note column, which kind of input it was ("in=touch").
enum class Input : uint8_t { Button, Touch, Tilt };
void noteInput(Input input = Input::Button);

// Column meanings per event (a, b):
//   Boot    ms = time from reset to the end of setup()
//   Open    ms = reader start to first page on screen
//   Page    ms = render time; a = input-to-render-start delay (-1 if none);
//           b = part of ms spent in panel calls (SPI transfer + refresh wait)
//   Index   ms = chapter build time; a = pages, b = spine index
//   Image   ms = decode time; a = width, b = height
//   Battery ms = 0 (periodic sample)
//   Sleep   ms = uptime in this session
//   Start   the first line of every session, written early in setup(): its
//           note column says the firmware version, reset reason and wake route
// `note` (optional) goes to the note column; for Page lines the input kind is
// put in front of it.
void record(Event event, uint32_t durationMs, int32_t a = -1, int32_t b = -1, const char* note = nullptr);

// Like record() with text in the note column (up to 96 characters; commas and
// line breaks become '_').
void recordNote(Event event, const char* note, uint32_t durationMs = 0, int32_t a = -1, int32_t b = -1);

// "2026-10-19 21:05" for the context's clock moved on to `nowMs`; empty when
// the clock is not set. Exposed for the host test.
void formatClock(const Context& context, uint32_t nowMs, char* out, size_t outSize);

// A named detail line ("open.opf", "index.layout", "home.first", ...): the
// event column carries `name` (up to 24 characters), the rest as record().
void recordNamed(const char* name, uint32_t durationMs, int32_t a = -1, int32_t b = -1);

// True when buffered lines should be written (buffer filling up, or a minute
// since the last write; after a failed write only the minute counts, so an
// unwritable card is retried once a minute). flush() appends them to the SD
// card; call it from the main loop while no render is running.
bool flushDue();
void flush();

}  // namespace SpeedLog
