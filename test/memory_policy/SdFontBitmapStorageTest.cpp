#include <Arduino.h>
#include <HalStorage.h>
#include <SdCardFont.h>
#include <gtest/gtest.h>

#include <cstring>

namespace {
void put16(std::vector<uint8_t>& b, size_t o, uint16_t v) {
  b[o] = v;
  b[o + 1] = v >> 8;
}
void put32(std::vector<uint8_t>& b, size_t o, uint32_t v) {
  put16(b, o, v);
  put16(b, o + 2, v >> 16);
}
template <class T>
void append(std::vector<uint8_t>& b, const T& v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  b.insert(b.end(), p, p + sizeof(v));
}
std::vector<uint8_t> fixture() {
  std::vector<uint8_t> b(64, 0);
  memcpy(b.data(), "CPFONT\0\0", 8);
  put16(b, 8, CPFONT_VERSION);
  b[12] = 1;
  put32(b, 36, 2);
  put32(b, 40, 4);
  b[44] = 16;
  put16(b, 45, 12);
  put16(b, 47, uint16_t(-4));
  put32(b, 56, 64);
  append(b, EpdUnicodeInterval{'A', 'C', 0});
  append(b, EpdUnicodeInterval{0xFFFD, 0xFFFD, 3});
  for (uint32_t i = 0; i < 4; i++) {
    EpdGlyph g{};
    g.width = 8;
    g.height = 8;
    g.advanceX = 128;
    g.dataLength = 8;
    g.dataOffset = i * 8;
    append(b, g);
  }
  for (uint8_t i = 0; i < 32; i++) b.push_back(i);
  return b;
}
}  // namespace
struct SdFontBitmapStorageTest : testing::Test {
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
    Storage.put("font.cpfont", fixture());
    // These cases pin down the per-page glyph buffers alone; the PSRAM file
    // mirror has its own cases below.
    FontFileMirror::setEnabled(false);
  }
  void TearDown() override {
    FontFileMirror::setEnabled(true);
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
};
TEST_F(SdFontBitmapStorageTest, ProductionLoadGrowRetainReleaseAndReload) {
  SdCardFont font;
  ASSERT_TRUE(font.load("font.cpfont"));
  ASSERT_EQ(font.prewarm("A", 1), 0);
  auto* data = font.getEpdFont()->data;
  ASSERT_NE(data->bitmap, nullptr);
  EXPECT_EQ(byteBufferPool(data->bitmap), MemoryPool::Psram);
  EXPECT_EQ(memcmp(data->bitmap, fixture().data() + 64 + 24 + 4 * sizeof(EpdGlyph), 8), 0);
  const auto* first = data->bitmap;
  font.clearCache();
  ASSERT_EQ(font.prewarm("A", 1), 0);
  EXPECT_EQ(font.getEpdFont()->data->bitmap, first);
  ASSERT_EQ(font.prewarm("ABC", 1), 0);
  data = font.getEpdFont()->data;
  EXPECT_EQ(byteBufferPool(data->bitmap), MemoryPool::Psram);
  for (int i = 0; i < 32; i++) EXPECT_EQ(data->bitmap[i], i);
  font.releaseForLowMemory();
  EXPECT_TRUE(fakeheap::live.empty());
  EXPECT_EQ(font.getEpdFont()->data->bitmap, nullptr);
  ASSERT_EQ(font.prewarm("B", 1), 0);
  EXPECT_NE(font.getEpdFont()->data->bitmap, nullptr);
  ASSERT_TRUE(font.load("font.cpfont"));
  EXPECT_TRUE(fakeheap::live.empty());
}
TEST_F(SdFontBitmapStorageTest, FailureInvalidatesViewsAndCanRecover) {
  SdCardFont font;
  ASSERT_TRUE(font.load("font.cpfont"));
  ASSERT_EQ(font.prewarm("A", 1), 0);
  fakeheap::external.fail = 1;
  fakeheap::internal.largest = 1;
  EXPECT_GT(font.prewarm("ABC", 1), 0);
  EXPECT_TRUE(font.lastPrewarmFailed());
  EXPECT_EQ(font.getEpdFont()->data->bitmap, nullptr);
  EXPECT_TRUE(fakeheap::live.empty());
  fakeheap::internal.largest = 1024 * 1024;
  ASSERT_EQ(font.prewarm("ABC", 1), 0);
  EXPECT_NE(font.getEpdFont()->data->bitmap, nullptr);
}
TEST_F(SdFontBitmapStorageTest, C3AndAdmittedInternalFallbackAndMetadataOnly) {
  for (bool psram : {false, true}) {
    fakeheap::reset(psram);
    SdCardFont font;
    ASSERT_TRUE(font.load("font.cpfont"));
    ASSERT_EQ(font.prewarm("A", 1, true), 0);
    EXPECT_TRUE(fakeheap::live.empty());
    EXPECT_EQ(font.getEpdFont()->data->bitmap, nullptr);
    fakeheap::external.fail = 1;
    ASSERT_EQ(font.prewarm("ABC", 1), 0);
    EXPECT_EQ(byteBufferPool(font.getEpdFont()->data->bitmap), MemoryPool::Internal);
    font.releaseForLowMemory();
    EXPECT_TRUE(fakeheap::live.empty());
  }
}

TEST_F(SdFontBitmapStorageTest, ExternalBitmapsStillHonorUnderuseAndInternalPressureRelease) {
  // A dense page followed by disjoint sparse pages must not pin its high-water
  // bitmap forever, even though the payload lives externally.
  std::vector<uint8_t> bytes(64, 0);
  memcpy(bytes.data(), "CPFONT\0\0", 8);
  put16(bytes, 8, CPFONT_VERSION);
  bytes[12] = 1;
  put32(bytes, 36, 2);
  put32(bytes, 40, 601);
  bytes[44] = 16;
  put16(bytes, 45, 12);
  put32(bytes, 56, 64);
  append(bytes, EpdUnicodeInterval{0x100, 0x100 + 599, 0});
  append(bytes, EpdUnicodeInterval{0xFFFD, 0xFFFD, 600});
  for (uint32_t i = 0; i < 601; ++i) {
    EpdGlyph g{};
    g.width = 8;
    g.height = 8;
    g.advanceX = 128;
    g.dataLength = 8;
    g.dataOffset = i * 8;
    append(bytes, g);
  }
  bytes.resize(bytes.size() + 601 * 8, 0xA5);
  Storage.put("large.cpfont", bytes);
  const auto range = [](int first, int count) {
    std::string result;
    for (int i = first; i < first + count; ++i) {
      const uint32_t cp = 0x100 + i;
      result.push_back(char(0xC0 | (cp >> 6)));
      result.push_back(char(0x80 | (cp & 63)));
    }
    return result;
  };
  SdCardFont font;
  ASSERT_TRUE(font.load("large.cpfont"));
  ASSERT_EQ(font.prewarm(range(0, 500).c_str(), 1), 0);
  ASSERT_NE(font.getEpdFont()->data->bitmap, nullptr);
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(font.prewarm(range(500 + i * 25, 25).c_str(), 1), 0);
    font.clearCache();
    EXPECT_EQ(fakeheap::live.empty(), i == 2);
  }
  ASSERT_EQ(font.prewarm(range(0, 20).c_str(), 1), 0);
  fakeheap::internal.free = 39 * 1024;
  font.clearCache();
  EXPECT_TRUE(fakeheap::live.empty());
  EXPECT_EQ(font.getEpdFont()->data->bitmap, nullptr);
}

// --- PSRAM file mirror together with SdCardFont ---

struct SdFontMirrorTest : testing::Test {
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
    Storage.put("font.cpfont", fixture());
  }
  void TearDown() override {
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
};

TEST_F(SdFontMirrorTest, RepeatPrewarmsAreServedFromMemory) {
  SdCardFont font;
  ASSERT_TRUE(font.load("font.cpfont"));
  ASSERT_TRUE(font.isMirrored());
  ASSERT_EQ(font.prewarm("ABC", 1), 0);
  const auto* data = font.getEpdFont()->data;
  for (int i = 0; i < 24; i++) EXPECT_EQ(data->bitmap[i], i);

  // Dropping the per-page buffers keeps the mirror while PSRAM is plentiful,
  // so the rebuild reads nothing from the card.
  font.releaseForLowMemory();
  ASSERT_TRUE(font.isMirrored());
  const size_t cardReads = Storage.data("font.cpfont").readCalls;
  ASSERT_EQ(font.prewarm("ABC", 1), 0);
  data = font.getEpdFont()->data;
  for (int i = 0; i < 24; i++) EXPECT_EQ(data->bitmap[i], i);
  EXPECT_EQ(Storage.data("font.cpfont").readCalls, cardReads);
  EXPECT_GT(font.mirrorStats().hitReads, 0u);
  font.releaseForLowMemory();
}

TEST_F(SdFontMirrorTest, PsramPressureReleasesTheMirrorAndReadsFallBackToTheCard) {
  SdCardFont font;
  ASSERT_TRUE(font.load("font.cpfont"));
  ASSERT_TRUE(font.isMirrored());
  ASSERT_EQ(font.prewarm("A", 1), 0);
  font.releaseForLowMemory();

  fakeheap::external.free = 64 * 1024;  // far below the mirror's PSRAM headroom
  font.releaseForLowMemory();
  EXPECT_FALSE(font.isMirrored());
  fakeheap::external.free = 8 * 1024 * 1024 - 0;  // restore for the rebuild below

  const size_t cardReads = Storage.data("font.cpfont").readCalls;
  ASSERT_EQ(font.prewarm("BC", 1), 0);
  EXPECT_GT(Storage.data("font.cpfont").readCalls, cardReads);
  const auto* data = font.getEpdFont()->data;
  for (int i = 0; i < 16; i++) EXPECT_EQ(data->bitmap[i], 8 + i);
  font.releaseForLowMemory();
}

TEST_F(SdFontMirrorTest, WithoutPsramTheFontBehavesAsBefore) {
  fakeheap::reset(/*psram=*/false);
  SdCardFont font;
  ASSERT_TRUE(font.load("font.cpfont"));
  EXPECT_FALSE(font.isMirrored());
  ASSERT_EQ(font.prewarm("ABC", 1), 0);
  const auto* data = font.getEpdFont()->data;
  for (int i = 0; i < 24; i++) EXPECT_EQ(data->bitmap[i], i);
  font.releaseForLowMemory();
}

TEST_F(SdFontMirrorTest, ReloadAndDestructionFreeTheMirror) {
  {
    SdCardFont font;
    ASSERT_TRUE(font.load("font.cpfont"));
    ASSERT_TRUE(font.isMirrored());
    ASSERT_TRUE(font.load("font.cpfont"));  // reload replaces, never stacks
    EXPECT_EQ(FontFileMirror::totalAttachedBytes(), fixture().size());
  }
  EXPECT_EQ(FontFileMirror::totalAttachedBytes(), 0u);
}
