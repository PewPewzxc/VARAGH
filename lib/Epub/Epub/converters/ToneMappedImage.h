#pragma once

#include <Memory.h>

#include <cstdint>

#include "ImageToFramebufferDecoder.h"
#include "PixelCache.h"

class GfxRenderer;

// Higher-quality image path for PSRAM devices. Decoders feed 8-bit grayscale
// source rows top to bottom. A levels step first snaps near-white to white
// and near-black to black, so off-white scanned paper stays clean instead of
// being diffused into grey speckle. The image is then shrunk by area averaging
// (every source pixel counts, instead of nearest-neighbour skipping) and
// quantised to the panel's 4 levels with Floyd-Steinberg error diffusion
// (smooth gradients, instead of the Bayer crosshatch). Each finished row is
// written to the framebuffer and, when caching, to the streamed pixel cache,
// with the same screen clipping as the Bayer path.
//
// The averaging and diffusion produce exactly the output of the SDK's
// freeink::ImageToneMapper (with the levels table applied to its input); this
// copy is arranged for speed on the ESP32-S3: the levels table is folded into
// the column sums, each output column's run of source pixels is summed in a
// register, sample counts come from precomputed column widths (32-bit, so any
// shrink ratio works), and quantisation is a table lookup.
class ToneMappedImageWriter {
 public:
  ToneMappedImageWriter() = default;
  ~ToneMappedImageWriter() { release(); }
  ToneMappedImageWriter(const ToneMappedImageWriter&) = delete;
  ToneMappedImageWriter& operator=(const ToneMappedImageWriter&) = delete;

  // Tone-map when dithering is requested, the board has PSRAM (C3 keeps its
  // smaller Bayer working set), and the image is not enlarged: upscales keep
  // their bilinear/replicating paths, which suit enlargement better.
  static bool shouldUse(const RenderConfig& config, int srcWidth, int srcHeight, int dstWidth, int dstHeight);

  // `cache` and `caching` are the decoder's streamed pixel cache and its live
  // flag; the writer clears the flag if a cache write fails.
  bool begin(GfxRenderer& renderer, const RenderConfig& config, PixelCache* cache, bool* caching, int srcWidth,
             int srcHeight, int dstWidth, int dstHeight);

  // Feed exactly srcHeight rows of srcWidth samples (0 = black, 255 = white).
  void addSourceRow(const uint8_t* row);

  // Emit the remaining destination rows. Call once after the last row.
  void finish();

  bool active() const { return active_; }

  // Test switch (default on): off makes shouldUse() decline, so unit tests
  // can exercise the decoders' ordered-dither paths.
  static void setEnabled(bool enabled) { enabled_ = enabled; }
  static bool enabled() { return enabled_; }

  // Levels: inputs at or below the black point print black, at or above the
  // white point print white, with a linear stretch in between.
  static constexpr int BLACK_POINT = 16;
  static constexpr int WHITE_POINT = 232;
  // Four output levels match the 2-bit .pxc cache and the grayscale planes.
  static constexpr int OUTPUT_LEVELS = 4;

 private:
  void finalizeRow(int dstY);
  void writeRow(int dstY, const uint8_t* levels, int dstWidth);
  void release();

  GfxRenderer* renderer_ = nullptr;
  const RenderConfig* config_ = nullptr;
  PixelCache* cache_ = nullptr;
  bool* caching_ = nullptr;
  int screenWidth_ = 0;
  int screenHeight_ = 0;
  bool active_ = false;

  int srcWidth_ = 0;
  int srcHeight_ = 0;
  int dstWidth_ = 0;
  int dstHeight_ = 0;
  int curSrcY_ = 0;         // source rows consumed
  int accDstY_ = 0;         // destination row being accumulated
  uint32_t rowsInAcc_ = 0;  // source rows summed into accDstY_ so far

  uint8_t levels_[256] = {};      // levels step
  uint8_t quantLevel_[256] = {};  // diffused value -> output level

  // One working buffer (internal RAM when it fits) carved into the arrays below.
  HeapByteBuffer work_;
  uint16_t* colStart_ = nullptr;  // dstWidth + 1: first source column of each output column
  uint32_t* accSum_ = nullptr;    // dstWidth: sums for the row being accumulated
  int32_t* errCur_ = nullptr;     // dstWidth + 2 (+1 offset so x-1 and x+1 stay in range)
  int32_t* errNext_ = nullptr;    // dstWidth + 2
  uint8_t* gray8_ = nullptr;      // dstWidth
  uint8_t* lastGray8_ = nullptr;  // dstWidth
  uint8_t* outLevels_ = nullptr;  // dstWidth

  static bool enabled_;
};
