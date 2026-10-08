#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>

// Runs a board game's computer move on a task of its own. The search is deep
// recursion and takes seconds: on the main task it would not fit the stack and
// would freeze the Back button. The task ends by itself when the work is done.
class GameThinker {
 public:
  using Work = void (*)(void* context);

  ~GameThinker() { cancel(); }

  // Starts `work(context)` on the worker task. timeLimitMs bounds the thinking
  // through shouldStop(). Returns false while an earlier job is still running
  // or when the task cannot be created.
  bool start(Work work, void* context, uint32_t timeLimitMs);
  bool running() const { return state_.load(std::memory_order_acquire) == State::Running; }
  // True once after a job has finished.
  bool takeFinished();
  // Asks a running job to stop and waits until it has.
  void cancel();

  // For the job: polled by the search, gives other tasks a turn and reports
  // whether the time is up or the job was cancelled.
  bool shouldStop();
  static bool stopCallback(void* thinker) { return static_cast<GameThinker*>(thinker)->shouldStop(); }

 private:
  enum class State : uint8_t { Idle, Running, Finished };
  static constexpr uint32_t kStackBytes = 20 * 1024;

  static void taskEntry(void* context);

  std::atomic<State> state_{State::Idle};
  std::atomic<bool> stop_{false};
  Work work_ = nullptr;
  void* context_ = nullptr;
  uint32_t startedMs_ = 0;
  uint32_t timeLimitMs_ = 0;
};
