#include <GfxRenderer.h>
#include <HalStorage.h>
#include <JPEGDEC.h>
#include <JpegToFramebufferConverter.h>
#include <ToneMappedImage.h>
#include <gtest/gtest.h>

#include <tuple>
#include <vector>

namespace {

struct Case {
  int width, height;  // source JPEG
  int mcu, mcusPerDraw;
  int maxWidth, maxHeight;
};

// Mirrors JpegToFramebufferConverter's size maths (float target scale, then
// JPEGDEC's power-of-two DCT step).
void expectedGeometry(const Case& c, int& scaledW, int& scaledH, int& dstW, int& dstH) {
  const float scaleX = (c.maxWidth > 0 && c.width > c.maxWidth) ? (float)c.maxWidth / c.width : 1.0f;
  const float scaleY = (c.maxHeight > 0 && c.height > c.maxHeight) ? (float)c.maxHeight / c.height : 1.0f;
  float target = scaleX < scaleY ? scaleX : scaleY;
  if (target > 1.0f) target = 1.0f;
  dstW = (int)(c.width * target);
  dstH = (int)(c.height * target);
  const int denom = target <= 0.125f ? 8 : target <= 0.25f ? 4 : target <= 0.5f ? 2 : 1;
  scaledW = (c.width + denom - 1) / denom;
  scaledH = (c.height + denom - 1) / denom;
}

}  // namespace

class JpegTonePathTest : public testing::TestWithParam<Case> {
 protected:
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
    jpegdec_test::reset();
    Storage.put("image.jpg", {0xff, 0xd8, 0xff, 0xd9});
  }
  void TearDown() override {
    jpegdec_test::reset();
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
};

// The decoder gathers JPEGDEC's MCU runs into whole rows before tone mapping.
// Its cache must equal feeding the same image rows straight to the writer.
TEST_P(JpegTonePathTest, BandAssemblyMatchesDirectRows) {
  const Case c = GetParam();
  jpegdec_test::emitBlocks = true;
  jpegdec_test::width = c.width;
  jpegdec_test::height = c.height;
  jpegdec_test::mcu = c.mcu;
  jpegdec_test::mcusPerDraw = c.mcusPerDraw;

  int scaledW, scaledH, dstW, dstH;
  expectedGeometry(c, scaledW, scaledH, dstW, dstH);
  ASSERT_LE(dstW, scaledW);
  ASSERT_LE(dstH, scaledH);

  GfxRenderer decodedRenderer;
  RenderConfig config;
  config.x = 5;
  config.y = 7;
  config.maxWidth = c.maxWidth;
  config.maxHeight = c.maxHeight;
  config.useDithering = true;
  config.cachePath = "decoded.pxt";
  JpegToFramebufferConverter converter;
  ASSERT_TRUE(converter.decodeToFramebuffer("image.jpg", decodedRenderer, config));
  const auto decoded = Storage.bytes("decoded.pxt");

  std::vector<uint8_t> rows(static_cast<size_t>(scaledW) * scaledH);
  for (int y = 0; y < scaledH; ++y)
    for (int x = 0; x < scaledW; ++x) rows[static_cast<size_t>(y) * scaledW + x] = jpegdec_test::pixel(x, y, scaledW, scaledH);

  GfxRenderer directRenderer;
  RenderConfig direct = config;
  direct.cachePath = "direct.pxt";
  PixelCache cache;
  bool caching = cache.begin(direct.cachePath, dstW, dstH, direct.x, direct.y, 1);
  ASSERT_TRUE(caching);
  ToneMappedImageWriter writer;
  ASSERT_TRUE(writer.begin(directRenderer, direct, &cache, &caching, scaledW, scaledH, dstW, dstH));
  for (int y = 0; y < scaledH; ++y) writer.addSourceRow(rows.data() + static_cast<size_t>(y) * scaledW);
  writer.finish();
  ASSERT_TRUE(cache.finalize());
  const auto expected = Storage.bytes("direct.pxt");

  ASSERT_EQ(decoded.size(), expected.size());
  EXPECT_EQ(decoded, expected);
  EXPECT_EQ(std::vector<uint8_t>(decodedRenderer.getWriteTarget(),
                                 decodedRenderer.getWriteTarget() + decodedRenderer.getDisplayWidthBytes() * 480),
            std::vector<uint8_t>(directRenderer.getWriteTarget(),
                                 directRenderer.getWriteTarget() + directRenderer.getDisplayWidthBytes() * 480));
}

INSTANTIATE_TEST_SUITE_P(Shapes, JpegTonePathTest,
                         testing::Values(Case{320, 240, 16, 3, 160, 120},    // DCT 1/2, then 1:1
                                         Case{333, 250, 16, 2, 200, 300},    // no DCT step, odd widths
                                         Case{1000, 700, 8, 5, 480, 800},    // 4:4:4 MCUs, DCT 1/2
                                         Case{257, 129, 16, 1, 100, 100},    // one MCU per draw
                                         Case{64, 48, 16, 4, 64, 48},        // exact 1:1
                                         Case{1601, 2403, 16, 7, 480, 700},  // tall page image, DCT 1/2
                                         Case{77, 35, 8, 3, 70, 30}));       // tiny, partial MCU rows
