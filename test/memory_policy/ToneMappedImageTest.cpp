#include <GfxRenderer.h>
#include <HalStorage.h>
#include <ImageToneMapper.h>
#include <ToneMappedImage.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>

namespace {

// Reads 2-bit level (0 = black .. 3 = white) of pixel x in a .pxc/.pxt row.
int cachedLevel(const std::vector<uint8_t>& file, const int width, const int x, const int y) {
  const int bytesPerRow = (width + 3) / 4;
  const uint8_t byte = file[4 + static_cast<size_t>(y) * bytesPerRow + x / 4];
  return (byte >> (6 - (x % 4) * 2)) & 0x03;
}

// Runs one image through the tone path exactly as the decoders do.
std::vector<uint8_t> toneMap(GfxRenderer& renderer, const std::vector<uint8_t>& gray, const int srcW, const int srcH,
                             const int dstW, const int dstH, const int x, const int y) {
  RenderConfig config;
  config.x = x;
  config.y = y;
  config.maxWidth = dstW;
  config.maxHeight = dstH;
  config.useDithering = true;
  config.cachePath = "image.pxt";

  PixelCache cache;
  bool caching = cache.begin(config.cachePath, dstW, dstH, x, y, 1);
  EXPECT_TRUE(caching);
  ToneMappedImageWriter writer;
  EXPECT_TRUE(ToneMappedImageWriter::shouldUse(config, srcW, srcH, dstW, dstH));
  EXPECT_TRUE(writer.begin(renderer, config, &cache, &caching, srcW, srcH, dstW, dstH));
  for (int row = 0; row < srcH; ++row) writer.addSourceRow(gray.data() + static_cast<size_t>(row) * srcW);
  writer.finish();
  EXPECT_TRUE(caching);
  EXPECT_TRUE(cache.finalize());
  return Storage.bytes("image.pxt");
}

bool framebufferBlack(GfxRenderer& renderer, const int x, const int y) {
  const uint8_t* fb = renderer.getWriteTarget();
  const int bytes = renderer.getDisplayWidthBytes();
  return (fb[static_cast<size_t>(y) * bytes + x / 8] & (0x80 >> (x % 8))) == 0;
}

}  // namespace

struct ToneMappedImageTest : testing::Test {
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
  }
  void TearDown() override {
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
};

TEST_F(ToneMappedImageTest, OffWhitePaperStaysCleanWhite) {
  // Scanned paper around 235 would diffuse into grey speckle without levels.
  constexpr int srcW = 240, srcH = 160, dstW = 120, dstH = 80;
  std::vector<uint8_t> gray(srcW * srcH, 235);
  GfxRenderer renderer;
  const auto file = toneMap(renderer, gray, srcW, srcH, dstW, dstH, 10, 20);
  ASSERT_EQ(file.size(), 4u + static_cast<size_t>((dstW + 3) / 4) * dstH);
  for (int y = 0; y < dstH; ++y) {
    for (int x = 0; x < dstW; ++x) {
      ASSERT_EQ(cachedLevel(file, dstW, x, y), 3) << x << "," << y;
      ASSERT_FALSE(framebufferBlack(renderer, 10 + x, 20 + y));
    }
  }
}

TEST_F(ToneMappedImageTest, GradientKeepsItsToneAndEveryRowIsCached) {
  constexpr int srcW = 400, srcH = 120, dstW = 200, dstH = 60;
  std::vector<uint8_t> gray(srcW * srcH);
  for (int y = 0; y < srcH; ++y)
    for (int x = 0; x < srcW; ++x) gray[static_cast<size_t>(y) * srcW + x] = static_cast<uint8_t>(x * 255 / (srcW - 1));
  GfxRenderer renderer;
  const auto file = toneMap(renderer, gray, srcW, srcH, dstW, dstH, 0, 0);
  ASSERT_EQ(file.size(), 4u + static_cast<size_t>((dstW + 3) / 4) * dstH);

  // Average level per tenth of the width rises monotonically from black to white.
  double previous = -1;
  for (int band = 0; band < 10; ++band) {
    double sum = 0;
    int count = 0;
    for (int y = 0; y < dstH; ++y)
      for (int x = band * dstW / 10; x < (band + 1) * dstW / 10; ++x, ++count) sum += cachedLevel(file, dstW, x, y);
    const double mean = sum / count;
    EXPECT_GE(mean + 0.05, previous) << "band " << band;
    previous = mean;
  }
  EXPECT_LT(previous, 3.01);
  EXPECT_GT(previous, 2.9);  // right edge: white
  // Left edge is solid black in both the cache and the framebuffer.
  for (int y = 0; y < dstH; ++y) {
    EXPECT_EQ(cachedLevel(file, dstW, 0, y), 0);
    EXPECT_TRUE(framebufferBlack(renderer, 0, y));
  }
}

TEST_F(ToneMappedImageTest, ClipsLikeTheOrderedDitherPath) {
  // Partly off the left and bottom edges: nothing is written outside the
  // screen, off-screen cache cells stay zero, on-screen cells are written.
  constexpr int srcW = 100, srcH = 100, dstW = 100, dstH = 100;
  std::vector<uint8_t> gray(srcW * srcH, 255);
  GfxRenderer renderer;
  const auto file = toneMap(renderer, gray, srcW, srcH, dstW, dstH, -30, 430);
  EXPECT_EQ(cachedLevel(file, dstW, 10, 10), 0);  // x = -20: off screen
  EXPECT_EQ(cachedLevel(file, dstW, 40, 10), 3);  // x = 10, y = 440: on screen, white
  EXPECT_EQ(cachedLevel(file, dstW, 40, 60), 0);  // y = 490: below the screen
}

TEST_F(ToneMappedImageTest, ExtremeShrinksKeepTheirTone) {
  // A 2048x3072 PNG drawn at 8x12 averages 256x256 = 65536 samples per pixel,
  // past a 16-bit counter: a white image must still come out white.
  RenderConfig config;
  config.useDithering = true;
  constexpr int srcW = 2048, srcH = 3072, dstW = 8, dstH = 12;
  ASSERT_TRUE(ToneMappedImageWriter::shouldUse(config, srcW, srcH, dstW, dstH));
  std::vector<uint8_t> gray(static_cast<size_t>(srcW) * srcH, 255);
  GfxRenderer renderer;
  const auto file = toneMap(renderer, gray, srcW, srcH, dstW, dstH, 4, 4);
  for (int y = 0; y < dstH; ++y)
    for (int x = 0; x < dstW; ++x) ASSERT_EQ(cachedLevel(file, dstW, x, y), 3) << x << "," << y;
}

namespace {
struct ReferenceRows {
  std::vector<uint8_t> levels;
  int width = 0;
};
void referenceSink(void* ctx, const int dstY, const uint8_t* levels, const int dstWidth) {
  auto* rows = static_cast<ReferenceRows*>(ctx);
  std::copy(levels, levels + dstWidth, rows->levels.begin() + static_cast<size_t>(dstY) * rows->width);
}
}  // namespace

TEST_F(ToneMappedImageTest, MatchesTheSdkToneMapperExactly) {
  // The writer's streamlined averaging and diffusion must reproduce the SDK's
  // freeink::ImageToneMapper fed with levelled rows, pixel for pixel.
  std::mt19937 rng(20260925);
  for (int iter = 0; iter < 300; ++iter) {
    fakeheap::reset();
    Storage.reset();
    const int srcW = 1 + static_cast<int>(rng() % 900);
    const int srcH = 1 + static_cast<int>(rng() % 700);
    const int dstW = 1 + static_cast<int>(rng() % std::min(srcW, 480));
    const int dstH = 1 + static_cast<int>(rng() % std::min(srcH, 480));
    std::vector<uint8_t> gray(static_cast<size_t>(srcW) * srcH);
    const int kind = static_cast<int>(rng() % 3);
    for (size_t i = 0; i < gray.size(); ++i) {
      gray[i] = kind == 0 ? static_cast<uint8_t>(rng()) : static_cast<uint8_t>((i * 7 + i / srcW * 13) & 0xFF);
      if (kind == 2 && rng() % 5 == 0) gray[i] = 240;
    }

    GfxRenderer renderer;
    const auto file = toneMap(renderer, gray, srcW, srcH, dstW, dstH, 0, 0);

    ReferenceRows ref;
    ref.width = dstW;
    ref.levels.assign(static_cast<size_t>(dstW) * dstH, 0xFF);
    freeink::ImageToneMapper mapper;
    ASSERT_TRUE(mapper.begin(srcW, srcH, dstW, dstH, ToneMappedImageWriter::OUTPUT_LEVELS, referenceSink, &ref));
    std::vector<uint8_t> levelled(static_cast<size_t>(srcW));
    for (int y = 0; y < srcH; ++y) {
      for (int x = 0; x < srcW; ++x) {
        const int v = (gray[static_cast<size_t>(y) * srcW + x] - ToneMappedImageWriter::BLACK_POINT) * 255 /
                      (ToneMappedImageWriter::WHITE_POINT - ToneMappedImageWriter::BLACK_POINT);
        levelled[x] = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
      }
      mapper.addSourceRow(levelled.data());
    }
    mapper.finish();

    for (int y = 0; y < dstH; ++y)
      for (int x = 0; x < dstW; ++x)
        ASSERT_EQ(cachedLevel(file, dstW, x, y), ref.levels[static_cast<size_t>(y) * dstW + x])
            << "iteration " << iter << " " << srcW << "x" << srcH << " -> " << dstW << "x" << dstH << " at " << x
            << "," << y;
  }
}

TEST_F(ToneMappedImageTest, OnlyDownscalesOnPsramBoardsWithDithering) {
  RenderConfig config;
  config.useDithering = true;
  EXPECT_TRUE(ToneMappedImageWriter::shouldUse(config, 800, 600, 400, 300));
  EXPECT_TRUE(ToneMappedImageWriter::shouldUse(config, 400, 300, 400, 300));
  EXPECT_FALSE(ToneMappedImageWriter::shouldUse(config, 200, 150, 400, 300));  // upscale keeps bilinear
  config.useDithering = false;
  EXPECT_FALSE(ToneMappedImageWriter::shouldUse(config, 800, 600, 400, 300));
  fakeheap::reset(/*psram=*/false);
  config.useDithering = true;
  EXPECT_FALSE(ToneMappedImageWriter::shouldUse(config, 800, 600, 400, 300));
}
