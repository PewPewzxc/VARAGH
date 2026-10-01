#include "FontFileMirror.h"

#include <Logging.h>
#include <PoolBudget.h>

#include <algorithm>
#include <cstring>

uint32_t FontFileMirror::totalAttachedBytes_ = 0;
FontFileMirror::Stats FontFileMirror::globalStats_;
bool FontFileMirror::enabled_ = true;

namespace {
// SD reads land here first: the SDMMC driver reads a DMA-capable destination
// in one multi-sector transfer, but falls back to one command per 512-byte
// sector when it has to write straight into PSRAM.
constexpr uint32_t BOUNCE_BYTES = FontFileMirror::BLOCK_SIZE;
}  // namespace

bool FontFileMirror::attach(const uint32_t fileSize) {
  release();
  if (!enabled_ || fileSize == 0 || fileSize > MAX_FILE_BYTES || !psramHeapAvailable()) return false;
  if (totalAttachedBytes_ + fileSize > MAX_TOTAL_BYTES) {
    LOG_DBG("SDCF", "Font mirror skipped: %u B would exceed the %u B mirror budget", static_cast<unsigned>(fileSize),
            static_cast<unsigned>(MAX_TOTAL_BYTES));
    return false;
  }
  if (!MemoryBudget::admits(byteHeapSnapshot(MemoryPool::Psram), {fileSize, fileSize, PSRAM_HEADROOM_BYTES})) {
    LOG_DBG("SDCF", "Font mirror skipped: PSRAM too tight for %u B", static_cast<unsigned>(fileSize));
    return false;
  }

  const uint32_t blocks = (fileSize + BLOCK_SIZE - 1) / BLOCK_SIZE;
  std::unique_ptr<uint32_t[]> loaded(new (std::nothrow) uint32_t[(blocks + 31) / 32]());
  if (!loaded) return false;
  HeapByteBuffer image = makePsramByteBufferNoThrow(fileSize);
  if (!image) {
    LOG_DBG("SDCF", "Font mirror skipped: PSRAM allocation of %u B failed", static_cast<unsigned>(fileSize));
    return false;
  }

  image_ = std::move(image);
  loaded_ = std::move(loaded);
  size_ = fileSize;
  blockCount_ = blocks;
  stats_ = Stats{};
  totalAttachedBytes_ += fileSize;
  LOG_DBG("SDCF", "Font mirror attached: %u B in %u blocks (all mirrors %u B)", static_cast<unsigned>(fileSize),
          static_cast<unsigned>(blocks), static_cast<unsigned>(totalAttachedBytes_));
  return true;
}

void FontFileMirror::release() {
  if (image_) totalAttachedBytes_ -= size_;
  image_.reset();
  loaded_.reset();
  size_ = 0;
  blockCount_ = 0;
}

bool FontFileMirror::covers(const uint32_t offset, const uint32_t len) const {
  if (!image_ || offset >= size_ || len > size_ - offset) return false;
  if (len == 0) return true;
  const uint32_t last = (offset + len - 1) / BLOCK_SIZE;
  for (uint32_t block = offset / BLOCK_SIZE; block <= last; ++block) {
    if (!blockLoaded(block)) return false;
  }
  return true;
}

bool FontFileMirror::fill(HalFile& file, const uint32_t offset, const uint32_t len) {
  if (!image_ || len == 0 || offset >= size_ || len > size_ - offset) return false;

  // Narrow the request to the span between its first and last missing block;
  // already-mirrored blocks inside that span are simply read again.
  uint32_t first = offset / BLOCK_SIZE;
  uint32_t last = (offset + len - 1) / BLOCK_SIZE;
  while (first <= last && blockLoaded(first)) ++first;
  if (first > last) return true;
  while (last > first && blockLoaded(last)) --last;

  const uint32_t start = first * BLOCK_SIZE;
  const uint32_t end = std::min(size_, (last + 1) * BLOCK_SIZE);
  const unsigned long startedAt = millis();
  if (!file.seekSet(start)) {
    LOG_ERR("SDCF", "Font mirror: seek to %u failed", static_cast<unsigned>(start));
    return false;
  }

  HeapByteBuffer bounce = makeInternalByteBufferNoThrow(BOUNCE_BYTES);
  bool ok = true;
  for (uint32_t pos = start; pos < end;) {
    const uint32_t chunk = std::min(end - pos, bounce ? BOUNCE_BYTES : end - pos);
    uint8_t* const dst = image_.get() + pos;
    const int got = file.read(bounce ? bounce.get() : dst, chunk);
    if (got != static_cast<int>(chunk)) {
      ok = false;
      break;
    }
    if (bounce) memcpy(dst, bounce.get(), chunk);
    pos += chunk;
  }
  if (!ok) {
    // Nothing in this span is trusted until it has been read in full.
    for (uint32_t block = first; block <= last; ++block) loaded_[block >> 5] &= ~(1U << (block & 31));
    LOG_ERR("SDCF", "Font mirror: short SD read in %u..%u", static_cast<unsigned>(start), static_cast<unsigned>(end));
    return false;
  }

  for (uint32_t block = first; block <= last; ++block) markLoaded(block);
  const uint32_t elapsedMs = millis() - startedAt;
  for (Stats* s : {&stats_, &globalStats_}) {
    s->sdFills++;
    s->sdBytes += end - start;
    s->sdFillMs += elapsedMs;
  }
  return true;
}

void FontFileMirror::copy(const uint32_t offset, void* dst, const uint32_t len) const {
  memcpy(dst, image_.get() + offset, len);
}

bool FontFileReader::open() {
  pos_ = 0;
  if (mirror_.attached()) return true;
  return ensureFileOpen();
}

bool FontFileReader::ensureFileOpen() {
  if (!fileOpen_) fileOpen_ = Storage.openFileForRead("SDCF", path_, file_);
  return fileOpen_;
}

bool FontFileReader::seekSet(const size_t pos) {
  if (!mirror_.attached()) return fileOpen_ && file_.seekSet(pos);
  if (pos > mirror_.size()) return false;
  pos_ = static_cast<uint32_t>(pos);
  return true;
}

int FontFileReader::read(void* buf, const size_t count) {
  if (!mirror_.attached()) return fileOpen_ ? file_.read(buf, count) : -1;

  const uint32_t size = mirror_.size();
  if (count == 0 || pos_ >= size) return 0;
  const uint32_t n = static_cast<uint32_t>(std::min<size_t>(count, size - pos_));
  if (mirror_.covers(pos_, n)) {
    mirror_.countHit();
  } else if (!ensureFileOpen() || !mirror_.fill(file_, pos_, n)) {
    return -1;
  }
  mirror_.copy(pos_, buf, n);
  pos_ += n;
  return static_cast<int>(n);
}

bool FontFileReader::close() {
  if (!fileOpen_) return true;
  fileOpen_ = false;
  return file_.close();
}
