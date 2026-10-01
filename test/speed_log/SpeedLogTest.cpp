#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>
#include <SpeedLog.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr char kLog[] = "/.crosspoint/speed-log.csv";
constexpr char kOld[] = "/.crosspoint/speed-log.old.csv";
constexpr int kColumns = 17;

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> out;
  std::stringstream in(text);
  std::string line;
  while (std::getline(in, line)) out.push_back(line);
  return out;
}

int fields(const std::string& line) { return static_cast<int>(std::count(line.begin(), line.end(), ',')) + 1; }

// Counts data lines and the lines reported as dropped, checking every line is
// whole.
void tally(const std::vector<std::string>& all, long long& records, long long& dropped) {
  records = 0;
  dropped = 0;
  for (size_t i = 1; i < all.size(); ++i) {
    if (all[i].find(",dropped,") != std::string::npos) {
      ASSERT_EQ(fields(all[i]), 3) << all[i];
      dropped += std::stoll(all[i].substr(all[i].rfind(',') + 1));
      continue;
    }
    ASSERT_EQ(fields(all[i]), kColumns) << "line " << i << ": " << all[i];
    ++records;
  }
}

}  // namespace

// The speed log's buffer is process-wide, so every case starts by writing out
// whatever an earlier case left behind and then clearing the fake card.
class SpeedLogTest : public testing::Test {
 protected:
  void SetUp() override {
    Storage.reset();
    fakeclock::nowMs += 61000;
    SpeedLog::flush();
    Storage.reset();
    fakelog::errors = 0;
  }
};

TEST_F(SpeedLogTest, ConcurrentRecordsStayWholeLines) {
  constexpr int kThreads = 4;
  constexpr int kPerThread = 1500;
  std::atomic<bool> done{false};
  std::thread flusher([&] {
    while (!done.load()) {
      fakeclock::nowMs += 3;
      if (SpeedLog::flushDue()) SpeedLog::flush();
    }
  });
  std::vector<std::thread> writers;
  for (int t = 0; t < kThreads; ++t) {
    writers.emplace_back([t] {
      std::mt19937 rng(t + 1);
      for (int i = 0; i < kPerThread; ++i) {
        if (rng() % 5 == 0) SpeedLog::noteInput();
        SpeedLog::record(static_cast<SpeedLog::Event>(rng() % 7), rng() % 5000, static_cast<int32_t>(rng() % 900) - 1,
                         static_cast<int32_t>(rng() % 90));
      }
    });
  }
  for (auto& writer : writers) writer.join();
  done = true;
  flusher.join();
  fakeclock::nowMs += 61000;
  SpeedLog::flush();

  ASSERT_FALSE(Storage.exists(kOld));
  const auto all = lines(Storage.text(kLog));
  ASSERT_FALSE(all.empty());
  EXPECT_EQ(all[0].rfind("t_ms,event,", 0), 0u);
  EXPECT_EQ(fields(all[0]), kColumns);
  long long records = 0, dropped = 0;
  tally(all, records, dropped);
  EXPECT_EQ(records + dropped, kThreads * kPerThread);
}

TEST_F(SpeedLogTest, RotatesOnceTheLogPassesHalfAMegabyte) {
  const std::string previous(600 * 1024, 'x');
  Storage.put(kLog, previous);
  SpeedLog::record(SpeedLog::Event::Battery, 0);
  fakeclock::nowMs += 61000;
  SpeedLog::flush();
  EXPECT_EQ(Storage.text(kOld), previous);
  const auto now = lines(Storage.text(kLog));
  ASSERT_EQ(now.size(), 2u);
  EXPECT_EQ(now[0].rfind("t_ms,event,", 0), 0u);
  EXPECT_NE(now[1].find(",battery,"), std::string::npos);
}

TEST_F(SpeedLogTest, UnwritableCardIsRetriedOncePerMinuteNotEveryTick) {
  // A removed card, or the card handed to the PC in USB Drive mode, with more
  // than the 2 KB flush threshold buffered.
  Storage.failOpen = true;
  for (int i = 0; i < 60; ++i) SpeedLog::record(SpeedLog::Event::Battery, 0);
  const int opensBefore = Storage.openCalls;
  for (int tick = 0; tick < 6000; ++tick) {  // one minute of 10 ms loop ticks
    fakeclock::nowMs += 10;
    if (SpeedLog::flushDue()) SpeedLog::flush();
  }
  EXPECT_LE(Storage.openCalls - opensBefore, 2);
  EXPECT_LE(fakelog::errors.load(), 1);

  // Once the card is back, the buffered lines (and a count of any that did
  // not fit) are written.
  Storage.failOpen = false;
  for (int tick = 0; tick < 7000 && !Storage.exists(kLog); ++tick) {
    fakeclock::nowMs += 10;
    if (SpeedLog::flushDue()) SpeedLog::flush();
  }
  long long records = 0, dropped = 0;
  tally(lines(Storage.text(kLog)), records, dropped);
  EXPECT_EQ(records + dropped, 60);
}
