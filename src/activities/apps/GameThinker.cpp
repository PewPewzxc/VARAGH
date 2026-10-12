#include "GameThinker.h"

#include <Arduino.h>
#include <Logging.h>

#ifndef SIMULATOR
#include <HalPowerManager.h>
#endif

namespace {

// A screen refresh normally lowers the processor clock while it waits for the
// panel. Not while a job is thinking: the move just played is being drawn at
// the very moment the answer to it is being worked out.
void holdFullSpeed(const bool hold) {
#ifndef SIMULATOR
  if (hold) {
    powerManager.beginBackgroundJob();
  } else {
    powerManager.endBackgroundJob();
  }
#else
  (void)hold;
#endif
}

}  // namespace

bool GameThinker::start(const Work work, void* context, const uint32_t timeLimitMs) {
  if (running()) return false;
  work_ = work;
  context_ = context;
  timeLimitMs_ = timeLimitMs;
  startedMs_ = millis();
  lastYieldMs_ = startedMs_;
  stop_.store(false, std::memory_order_release);
  state_.store(State::Running, std::memory_order_release);
  holdFullSpeed(true);
  // On the two-core boards the search gets the core the screen is not drawn from.
  const BaseType_t core = portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY;
  if (xTaskCreatePinnedToCore(taskEntry, "AppThinker", kStackBytes, this, 1, nullptr, core) != pdPASS) {
    LOG_ERR("APPS", "Could not start the thinking task (free heap %u)", ESP.getFreeHeap());
    holdFullSpeed(false);
    state_.store(State::Idle, std::memory_order_release);
    return false;
  }
  return true;
}

void GameThinker::taskEntry(void* context) {
  auto* self = static_cast<GameThinker*>(context);
  self->work_(self->context_);
  LOG_DBG("APPS", "Thinking took %lu ms, stack left %u", static_cast<unsigned long>(millis() - self->startedMs_),
          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
  holdFullSpeed(false);
  self->state_.store(State::Finished, std::memory_order_release);
  vTaskDelete(nullptr);
}

bool GameThinker::takeFinished() {
  State expected = State::Finished;
  return state_.compare_exchange_strong(expected, State::Idle, std::memory_order_acq_rel);
}

void GameThinker::cancel() {
  stop_.store(true, std::memory_order_release);
  while (running()) vTaskDelay(1);
  state_.store(State::Idle, std::memory_order_release);
}

bool GameThinker::shouldStop() {
  // The search asks about every thousand positions, far more often than the
  // idle task and its watchdog need their tick.
  const uint32_t now = millis();
  if (now - lastYieldMs_ >= kYieldEveryMs) {
    vTaskDelay(1);
    lastYieldMs_ = millis();
  }
  return stop_.load(std::memory_order_acquire) || (timeLimitMs_ != 0 && now - startedMs_ >= timeLimitMs_);
}
