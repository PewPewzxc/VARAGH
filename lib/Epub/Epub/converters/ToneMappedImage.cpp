#include "ToneMappedImage.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <cstring>
#include <utility>

#include "DirectPixelWriter.h"

bool ToneMappedImageWriter::enabled_ = true;

namespace {
constexpr int MAX_LEVEL = ToneMappedImageWriter::OUTPUT_LEVELS - 1;
constexpr int STEP = 255 / MAX_LEVEL;  // 85 for four levels

constexpr size_t align4(const size_t n) { return (n + 3) & ~static_cast<size_t>(3); }
}  // namespace

bool ToneMappedImageWriter::shouldUse(const RenderConfig& config, const int srcWidth, const int srcHeight,
                                      const int dstWidth, const int dstHeight) {
  return enabled_ && config.useDithering && psramHeapAvailable() && srcWidth > 0 && srcHeight > 0 && dstWidth > 0 &&
         dstHeight > 0 && dstWidth <= srcWidth && dstHeight <= srcHeight && srcWidth <= UINT16_MAX;
}

bool ToneMappedImageWriter::begin(GfxRenderer& renderer, const RenderConfig& config, PixelCache* cache,
                                  bool* caching, const int srcWidth, const int srcHeight, const int dstWidth,
                                  const int dstHeight) {
  release();
  if (srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0 || srcWidth > UINT16_MAX) return false;
  renderer_ = &renderer;
  config_ = &config;
  cache_ = cache;
  caching_ = caching;
  screenWidth_ = renderer.getScreenWidth();
  screenHeight_ = renderer.getScreenHeight();
  srcWidth_ = srcWidth;
  srcHeight_ = srcHeight;
  dstWidth_ = dstWidth;
  dstHeight_ = dstHeight;
  curSrcY_ = 0;
  accDstY_ = 0;
  rowsInAcc_ = 0;

  // About 17 bytes per output column (~14 KB for a full-width image): keep it
  // in internal RAM, which the per-pixel loops read far faster than PSRAM.
  const size_t w = static_cast<size_t>(dstWidth);
  const size_t colStartBytes = align4((w + 1) * sizeof(uint16_t));
  const size_t accBytes = w * sizeof(uint32_t);
  const size_t errBytes = (w + 2) * sizeof(int32_t);
  const size_t total = colStartBytes + accBytes + 2 * errBytes + 3 * w;
  work_ = makeInternalByteBufferNoThrow(total);
  if (!work_) work_ = makeDefaultByteBufferNoThrow(total);
  if (!work_) {
    LOG_ERR("IMG", "Tone mapper buffers unavailable (%u B); using ordered dither", static_cast<unsigned>(total));
    return false;
  }
  uint8_t* p = work_.get();
  colStart_ = reinterpret_cast<uint16_t*>(p);
  p += colStartBytes;
  accSum_ = reinterpret_cast<uint32_t*>(p);
  p += accBytes;
  errCur_ = reinterpret_cast<int32_t*>(p);
  p += errBytes;
  errNext_ = reinterpret_cast<int32_t*>(p);
  p += errBytes;
  gray8_ = p;
  p += w;
  lastGray8_ = p;
  p += w;
  outLevels_ = p;

  // Area averaging: source column sx belongs to output column
  // floor(sx * dstWidth / srcWidth). That map never decreases, so each output
  // column owns one contiguous run of source columns; store where it starts.
  int next = 0;
  for (int sx = 0; sx < srcWidth; ++sx) {
    int dx = static_cast<int>((static_cast<int64_t>(sx) * dstWidth) / srcWidth);
    if (dx >= dstWidth) dx = dstWidth - 1;
    while (next <= dx) colStart_[next++] = static_cast<uint16_t>(sx);
  }
  while (next <= dstWidth) colStart_[next++] = static_cast<uint16_t>(srcWidth);

  for (int v = 0; v < 256; ++v) {
    const int stretched = (v - BLACK_POINT) * 255 / (WHITE_POINT - BLACK_POINT);
    levels_[v] = static_cast<uint8_t>(stretched < 0 ? 0 : (stretched > 255 ? 255 : stretched));
    const int q = (v + STEP / 2) / STEP;
    quantLevel_[v] = static_cast<uint8_t>(q > MAX_LEVEL ? MAX_LEVEL : q);
  }
  memset(accSum_, 0, accBytes);
  memset(errCur_, 0, errBytes);
  memset(errNext_, 0, errBytes);
  memset(lastGray8_, 0, w);
  active_ = true;
  return true;
}

void ToneMappedImageWriter::addSourceRow(const uint8_t* row) {
  if (!active_ || curSrcY_ >= srcHeight_) return;

  // Source rows belong to output row floor(y * dstHeight / srcHeight); once a
  // row starts a new output row, the rows before it are final.
  int dstY = static_cast<int>((static_cast<int64_t>(curSrcY_) * dstHeight_) / srcHeight_);
  if (dstY >= dstHeight_) dstY = dstHeight_ - 1;
  while (accDstY_ < dstY) {
    finalizeRow(accDstY_);
    ++accDstY_;
  }

  const uint8_t* const lut = levels_;
  const uint16_t* const start = colStart_;
  uint32_t* const acc = accSum_;
  for (int dx = 0; dx < dstWidth_; ++dx) {
    uint32_t sum = 0;
    const int end = start[dx + 1];
    for (int sx = start[dx]; sx < end; ++sx) sum += lut[row[sx]];
    acc[dx] += sum;
  }
  ++rowsInAcc_;
  ++curSrcY_;
}

void ToneMappedImageWriter::finish() {
  if (active_) {
    while (accDstY_ < dstHeight_) {
      finalizeRow(accDstY_);
      ++accDstY_;
    }
  }
  release();
}

void ToneMappedImageWriter::finalizeRow(const int dstY) {
  const int w = dstWidth_;

  // 1. Averaged row. An output column without samples (never the case when
  //    shrinking) repeats its left neighbour; a row without samples repeats
  //    the previous row.
  if (rowsInAcc_ > 0) {
    uint8_t left = lastGray8_[0];
    for (int dx = 0; dx < w; ++dx) {
      const uint32_t count = static_cast<uint32_t>(colStart_[dx + 1] - colStart_[dx]) * rowsInAcc_;
      if (count > 0) left = static_cast<uint8_t>(accSum_[dx] / count);
      gray8_[dx] = left;
    }
  } else {
    memcpy(gray8_, lastGray8_, w);
  }

  // 2. Floyd-Steinberg, left to right: 7/16 right, 3/16 down-left, 5/16 down,
  //    1/16 down-right. The error rows are offset by one so x-1 and x+1 stay
  //    in range at the edges.
  for (int dx = 0; dx < w; ++dx) {
    int v = gray8_[dx] + errCur_[dx + 1];
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    const int q = quantLevel_[v];
    outLevels_[dx] = static_cast<uint8_t>(q);
    const int err = v - q * STEP;
    errCur_[dx + 2] += err * 7 / 16;
    errNext_[dx] += err * 3 / 16;
    errNext_[dx + 1] += err * 5 / 16;
    errNext_[dx + 2] += err * 1 / 16;
  }

  writeRow(dstY, outLevels_, w);

  // 3. Roll the row state forward.
  memcpy(lastGray8_, gray8_, w);
  std::swap(errCur_, errNext_);
  memset(errNext_, 0, sizeof(int32_t) * (w + 2));
  memset(accSum_, 0, sizeof(uint32_t) * w);
  rowsInAcc_ = 0;
}

void ToneMappedImageWriter::release() {
  active_ = false;
  work_.reset();
  colStart_ = nullptr;
  accSum_ = nullptr;
  errCur_ = nullptr;
  errNext_ = nullptr;
  gray8_ = nullptr;
  lastGray8_ = nullptr;
  outLevels_ = nullptr;
}

void ToneMappedImageWriter::writeRow(const int dstY, const uint8_t* levels, const int dstWidth) {
  const int cfgX = config_->x;
  const int outY = config_->y + dstY;
  // Same clipping as the ordered-dither paths: rows and columns off screen are
  // neither drawn nor cached (their cache bytes stay zero).
  if (outY < 0 || outY >= screenHeight_) return;
  int dstXStart = 0;
  int dstXEnd = dstWidth;
  if (dstXStart < -cfgX) dstXStart = -cfgX;
  if (dstXEnd > screenWidth_ - cfgX) dstXEnd = screenWidth_ - cfgX;
  if (dstXStart >= dstXEnd) return;

  DirectPixelWriter pw;
  pw.init(*renderer_);
  pw.beginRow(outY);

  DirectCacheWriter cw;
  bool caching = caching_ && *caching_;
  if (caching) {
    if (!cache_->advanceTo(dstY)) {
      caching = false;
      *caching_ = false;
    } else {
      cw.init(cache_->buffer, cache_->bytesPerRow, cache_->bandRows, cache_->originX);
      cw.beginRow(outY, config_->y + cache_->bandStart);
    }
  }

  for (int dstX = dstXStart; dstX < dstXEnd; ++dstX) {
    const int outX = cfgX + dstX;
    const uint8_t level = levels[dstX];
    pw.writePixel(outX, level);
    if (caching) cw.writePixel(outX, level);
  }
}
