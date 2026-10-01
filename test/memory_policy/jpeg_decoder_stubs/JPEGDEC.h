#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

struct JPEGFILE {
  int32_t iPos = 0;
  int32_t iSize = 0;
  void* fHandle = nullptr;
};

struct JPEGDRAW {
  int x = 0;
  int y = 0;
  int iWidth = 0;
  int iHeight = 0;
  int iWidthUsed = 0;
  int iBpp = 0;
  uint16_t* pPixels = nullptr;
  void* pUser = nullptr;
};

using JPEG_READ_CALLBACK = int32_t(JPEGFILE*, uint8_t*, int32_t);
using JPEG_SEEK_CALLBACK = int32_t(JPEGFILE*, int32_t);
using JPEG_DRAW_CALLBACK = int(JPEGDRAW*);
using JPEG_OPEN_CALLBACK = void*(const char*, int32_t*);
using JPEG_CLOSE_CALLBACK = void(void*);

inline constexpr int JPEG_MODE_PROGRESSIVE = 1;
inline constexpr int EIGHT_BIT_GRAYSCALE = 3;
inline constexpr int JPEG_SCALE_HALF = 2;
inline constexpr int JPEG_SCALE_QUARTER = 4;
inline constexpr int JPEG_SCALE_EIGHTH = 8;

namespace jpegdec_test {
inline int openResult = 1;
inline int decodeResult = 1;
inline int closeCalls = 0;
inline int width = 320;
inline int height = 240;
// When set, decode() streams a synthetic image through the draw callback the
// way JPEGDEC does: MCU rows top to bottom, each row as left-to-right runs of
// `mcusPerDraw` MCUs, the last run padded to whole MCUs (iWidthUsed < iWidth).
inline bool emitBlocks = false;
inline int mcu = 16;
inline int mcusPerDraw = 3;
inline uint8_t pixel(int x, int y, int w, int h) {
  return static_cast<uint8_t>((x * 255 / (w > 1 ? w - 1 : 1) + (y * 37) % (h > 0 ? h : 1)) & 0xFF);
}

inline void reset() {
  openResult = 1;
  decodeResult = 1;
  closeCalls = 0;
  width = 320;
  height = 240;
  emitBlocks = false;
  mcu = 16;
  mcusPerDraw = 3;
}
}  // namespace jpegdec_test

class JPEGDEC {
 public:
  int open(const char* filename, JPEG_OPEN_CALLBACK* openCallback, JPEG_CLOSE_CALLBACK* closeCallback,
           JPEG_READ_CALLBACK*, JPEG_SEEK_CALLBACK*, JPEG_DRAW_CALLBACK* drawCallback) {
    draw_ = drawCallback;
    closeCallback_ = closeCallback;
    handle_ = (*openCallback)(filename, &file_.iSize);
    file_.fHandle = handle_;
    if (!handle_) return 0;
    return jpegdec_test::openResult;
  }

  void close() {
    ++jpegdec_test::closeCalls;
    if (closeCallback_) (*closeCallback_)(handle_);
  }

  int getLastError() const { return 77; }
  int getWidth() const { return jpegdec_test::width; }
  int getHeight() const { return jpegdec_test::height; }
  int getJPEGType() const { return 0; }
  void setPixelType(int) {}
  void setUserPointer(void* user) { user_ = user; }
  int decode(int, int, int scale) {
    if (!jpegdec_test::emitBlocks || !draw_) return jpegdec_test::decodeResult;
    const int denom = scale == JPEG_SCALE_EIGHTH ? 8 : scale == JPEG_SCALE_QUARTER ? 4 : scale == JPEG_SCALE_HALF ? 2 : 1;
    const int w = (jpegdec_test::width + denom - 1) / denom;
    const int h = (jpegdec_test::height + denom - 1) / denom;
    const int m = jpegdec_test::mcu;
    std::vector<uint8_t> pixels;
    for (int by = 0; by < h; by += m) {
      for (int bx = 0; bx < w; bx += m * jpegdec_test::mcusPerDraw) {
        const int remaining = w - bx;
        const int runW = std::min(m * jpegdec_test::mcusPerDraw, (remaining + m - 1) / m * m);
        pixels.assign(static_cast<size_t>(runW) * m, 0);
        for (int r = 0; r < m; ++r)
          for (int c = 0; c < runW; ++c)
            pixels[static_cast<size_t>(r) * runW + c] = jpegdec_test::pixel(bx + c, by + r, w, h);
        JPEGDRAW d;
        d.x = bx;
        d.y = by;
        d.iWidth = runW;
        d.iHeight = m;
        d.iWidthUsed = std::min(runW, remaining);
        d.pPixels = reinterpret_cast<uint16_t*>(pixels.data());
        d.pUser = user_;
        if (!draw_(&d)) return 0;
      }
    }
    return jpegdec_test::decodeResult;
  }

 private:
  JPEGFILE file_{};
  void* handle_ = nullptr;
  JPEG_CLOSE_CALLBACK* closeCallback_ = nullptr;
  JPEG_DRAW_CALLBACK* draw_ = nullptr;
  void* user_ = nullptr;
  uint8_t padding_[128]{};
};
