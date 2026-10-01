#pragma once

#include <HalStorage.h>
#include <Memory.h>

#include <cstddef>
#include <cstdint>
#include <memory>

// Keeps the bytes of an SD-card .cpfont in PSRAM once they have been read, so
// later glyph, advance and kerning lookups copy from memory instead of seeking
// on the card. PSRAM for the whole file is reserved when the font loads, but a
// 4 KB block is only fetched from the card the first time a read touches it:
// attaching costs no SD time, and a book's working set of glyphs settles after
// a few pages. Boards without PSRAM (and loads that would crowd PSRAM) never
// attach, and every read goes to the card exactly as before.
//
// Not thread-safe; callers serialize access exactly as they already serialize
// the SD reads it replaces (one open reader per font file).
class FontFileMirror {
 public:
  static constexpr uint32_t BLOCK_SIZE = 4096;
  // Larger fonts stay on the SD path; NotoVazir's biggest size is ~2.3 MB.
  static constexpr uint32_t MAX_FILE_BYTES = 4U * 1024U * 1024U;
  // All mirrors together may not exceed this, whatever is free.
  static constexpr uint32_t MAX_TOTAL_BYTES = 5U * 1024U * 1024U;
  // PSRAM that must still be free after a mirror is reserved, so image
  // decoding, page buffers and everything else keep their headroom.
  static constexpr uint32_t PSRAM_HEADROOM_BYTES = 1536U * 1024U;

  FontFileMirror() = default;
  ~FontFileMirror() { release(); }
  FontFileMirror(const FontFileMirror&) = delete;
  FontFileMirror& operator=(const FontFileMirror&) = delete;

  // Reserve memory for a file of fileSize bytes. Returns false and stays
  // detached when PSRAM is unavailable or the budget above would be exceeded.
  bool attach(uint32_t fileSize);
  void release();

  bool attached() const { return image_ != nullptr; }
  uint32_t size() const { return size_; }

  // True when every block overlapping [offset, offset + len) is mirrored.
  bool covers(uint32_t offset, uint32_t len) const;

  // Read the blocks overlapping [offset, offset + len) that are not mirrored
  // yet from `file` (already open) in one contiguous SD read.
  bool fill(HalFile& file, uint32_t offset, uint32_t len);

  // Copy mirrored bytes. The range must already be covered.
  void copy(uint32_t offset, void* dst, uint32_t len) const;

  struct Stats {
    uint32_t hitReads = 0;    // reads served entirely from memory
    uint32_t sdFills = 0;     // SD reads made to mirror missing blocks
    uint32_t sdBytes = 0;     // bytes read from the card into the mirror
    uint32_t sdFillMs = 0;    // time spent in those SD reads
  };
  const Stats& stats() const { return stats_; }
  void countHit() {
    stats_.hitReads++;
    globalStats_.hitReads++;
  }

  // Totals across every mirror since boot (for the speed log).
  static const Stats& globalStats() { return globalStats_; }

  // Total PSRAM held by every attached mirror.
  static uint32_t totalAttachedBytes() { return totalAttachedBytes_; }

  // Global switch (default on). When off, attach() declines and fonts read the
  // card exactly as before; tests of the per-page glyph buffers use this.
  static void setEnabled(bool enabled) { enabled_ = enabled; }
  static bool enabled() { return enabled_; }

 private:
  bool blockLoaded(uint32_t block) const { return (loaded_[block >> 5] >> (block & 31)) & 1U; }
  void markLoaded(uint32_t block) { loaded_[block >> 5] |= 1U << (block & 31); }

  HeapByteBuffer image_;
  std::unique_ptr<uint32_t[]> loaded_;  // one bit per BLOCK_SIZE block
  uint32_t size_ = 0;
  uint32_t blockCount_ = 0;
  Stats stats_;

  static uint32_t totalAttachedBytes_;
  static Stats globalStats_;
  static bool enabled_;
};

// Reads a .cpfont through its mirror when one is attached, otherwise straight
// from the card. It offers the subset of HalFile that SdCardFont uses
// (seekSet/read/close with the same return conventions), so the font parser
// is unchanged. With a mirror attached the SD file is opened only when a read
// misses the mirror.
class FontFileReader {
 public:
  FontFileReader(FontFileMirror& mirror, const char* path) : mirror_(mirror), path_(path) {}
  FontFileReader(const FontFileReader&) = delete;
  FontFileReader& operator=(const FontFileReader&) = delete;

  // Mirror attached: always succeeds; the card is touched on the first miss.
  // Otherwise opens the SD file, like Storage.openFileForRead().
  bool open();
  bool seekSet(size_t pos);
  // Returns the number of bytes read (short at end of file) or -1 on error.
  int read(void* buf, size_t count);
  bool close();

 private:
  bool ensureFileOpen();

  FontFileMirror& mirror_;
  const char* path_;
  HalFile file_;
  bool fileOpen_ = false;
  uint32_t pos_ = 0;
};
