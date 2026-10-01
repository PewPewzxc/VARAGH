#include <Arduino.h>
#include <FontFileMirror.h>
#include <HalStorage.h>
#include <gtest/gtest.h>

#include <cstring>
#include <random>
#include <vector>

namespace {

constexpr char kPath[] = "font.cpfont";

std::vector<uint8_t> randomBytes(const size_t size, const uint32_t seed) {
  std::mt19937 rng(seed);
  std::vector<uint8_t> bytes(size);
  for (auto& b : bytes) b = static_cast<uint8_t>(rng());
  return bytes;
}

// Reads [offset, offset + len) through a FontFileReader the way SdCardFont
// does: open, seekSet, read, close.
int readThrough(FontFileMirror& mirror, const size_t offset, uint8_t* out, const size_t len) {
  FontFileReader reader(mirror, kPath);
  if (!reader.open()) return -2;
  if (!reader.seekSet(offset)) return -3;
  const int got = reader.read(out, len);
  reader.close();
  return got;
}

}  // namespace

struct FontFileMirrorTest : testing::Test {
  void SetUp() override {
    fakeheap::reset();
    Storage.reset();
  }
  void TearDown() override {
    EXPECT_TRUE(fakeheap::live.empty());
    Storage.reset();
  }
};

TEST_F(FontFileMirrorTest, RandomReadsMatchTheFileByteForByte) {
  const auto bytes = randomBytes(150000, 7);
  Storage.put(kPath, bytes);
  FontFileMirror mirror;
  ASSERT_TRUE(mirror.attach(static_cast<uint32_t>(bytes.size())));

  std::mt19937 rng(11);
  std::vector<uint8_t> out(12000);
  for (int i = 0; i < 20000; ++i) {
    const size_t offset = rng() % (bytes.size() + 64);  // sometimes past the end
    const size_t len = 1 + rng() % out.size();
    const int got = readThrough(mirror, offset, out.data(), len);
    if (offset > bytes.size()) {
      ASSERT_EQ(got, -3) << "seek past EOF must fail like HalFile::seekSet";
      continue;
    }
    const size_t expected = std::min(len, bytes.size() - offset);
    ASSERT_EQ(got, static_cast<int>(expected)) << "offset " << offset << " len " << len;
    ASSERT_EQ(0, memcmp(out.data(), bytes.data() + offset, expected)) << "offset " << offset << " len " << len;
  }
  mirror.release();
}

TEST_F(FontFileMirrorTest, SecondPassNeverTouchesTheCard) {
  const auto bytes = randomBytes(40000, 3);
  Storage.put(kPath, bytes);
  FontFileMirror mirror;
  ASSERT_TRUE(mirror.attach(static_cast<uint32_t>(bytes.size())));

  std::vector<uint8_t> out(bytes.size());
  for (size_t off = 0; off < bytes.size(); off += 16) {
    ASSERT_EQ(readThrough(mirror, off, out.data() + off, std::min<size_t>(16, bytes.size() - off)),
              static_cast<int>(std::min<size_t>(16, bytes.size() - off)));
  }
  EXPECT_EQ(out, bytes);
  const size_t sdReadsAfterFirstPass = Storage.data(kPath).readCalls;
  // Blocks are fetched once each: 40000 / 4096 -> 10 blocks, far below the
  // 2500 small reads the parser made.
  EXPECT_LE(mirror.stats().sdFills, 10U);
  EXPECT_GT(sdReadsAfterFirstPass, 0U);

  for (size_t off = 0; off < bytes.size(); off += 97) {
    uint8_t chunk[97];
    const size_t len = std::min<size_t>(sizeof(chunk), bytes.size() - off);
    ASSERT_EQ(readThrough(mirror, off, chunk, len), static_cast<int>(len));
    ASSERT_EQ(0, memcmp(chunk, bytes.data() + off, len));
  }
  EXPECT_EQ(Storage.data(kPath).readCalls, sdReadsAfterFirstPass) << "fully mirrored reads must not hit the card";
  mirror.release();
}

TEST_F(FontFileMirrorTest, WithoutPsramEveryReadGoesToTheCard) {
  fakeheap::reset(/*psram=*/false);
  const auto bytes = randomBytes(9000, 5);
  Storage.put(kPath, bytes);
  FontFileMirror mirror;
  EXPECT_FALSE(mirror.attach(static_cast<uint32_t>(bytes.size())));
  EXPECT_FALSE(mirror.attached());

  uint8_t out[300];
  ASSERT_EQ(readThrough(mirror, 8800, out, sizeof(out)), 200);  // short at EOF, as HalFile
  EXPECT_EQ(0, memcmp(out, bytes.data() + 8800, 200));
  const size_t before = Storage.data(kPath).readCalls;
  ASSERT_EQ(readThrough(mirror, 100, out, 50), 50);
  EXPECT_GT(Storage.data(kPath).readCalls, before);
}

TEST_F(FontFileMirrorTest, FailedCardReadIsRetriedNotCached) {
  const auto bytes = randomBytes(20000, 9);
  Storage.put(kPath, bytes);
  FontFileMirror mirror;
  ASSERT_TRUE(mirror.attach(static_cast<uint32_t>(bytes.size())));

  // The card stops returning data halfway through the second block.
  Storage.data(kPath).readFailAt = 6000;
  uint8_t out[64];
  EXPECT_EQ(readThrough(mirror, 5000, out, sizeof(out)), -1);
  EXPECT_FALSE(mirror.covers(4096, 4096));

  // Once the card recovers the same range reads correctly.
  Storage.data(kPath).readFailAt = std::numeric_limits<size_t>::max();
  ASSERT_EQ(readThrough(mirror, 5000, out, sizeof(out)), static_cast<int>(sizeof(out)));
  EXPECT_EQ(0, memcmp(out, bytes.data() + 5000, sizeof(out)));
  mirror.release();
}

TEST_F(FontFileMirrorTest, RespectsFileAndBudgetLimits) {
  FontFileMirror tooBig;
  EXPECT_FALSE(tooBig.attach(FontFileMirror::MAX_FILE_BYTES + 1));
  EXPECT_FALSE(tooBig.attach(0));

  // Two mirrors may not exceed the combined budget.
  FontFileMirror first, second;
  ASSERT_TRUE(first.attach(FontFileMirror::MAX_FILE_BYTES));
  EXPECT_FALSE(second.attach(FontFileMirror::MAX_TOTAL_BYTES - FontFileMirror::MAX_FILE_BYTES + 1));
  EXPECT_EQ(FontFileMirror::totalAttachedBytes(), FontFileMirror::MAX_FILE_BYTES);
  first.release();
  EXPECT_EQ(FontFileMirror::totalAttachedBytes(), 0U);
  EXPECT_TRUE(second.attach(1024));
  second.release();
}

TEST_F(FontFileMirrorTest, LeavesPsramHeadroom) {
  // 8 MB fake PSRAM: a 4 MB mirror fits, but not once PSRAM is mostly used.
  fakeheap::external.free = FontFileMirror::PSRAM_HEADROOM_BYTES + 100 * 1024;
  fakeheap::external.largest = fakeheap::external.free;
  FontFileMirror mirror;
  EXPECT_FALSE(mirror.attach(200 * 1024));
  EXPECT_TRUE(mirror.attach(50 * 1024));
  mirror.release();
}
