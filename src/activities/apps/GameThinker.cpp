#include "GameThinker.h"

#include <Arduino.h>
#include <Logging.h>

bool GameThinker::start(const Work work, void* context, const uint32_t timeLimitMs) {
  if (running()) return false;
  work_ = work;
  context_ = context;
  timeLimitMs_ = timeLimitMs;
  startedMs_ = millis();
  stop_.store(false, std::memory_order_release);
  state_.store(State::Running, std::memory_order_release);
  // On the two-core boards the search gets the core the screen is not drawn from.
  const BaseType_t core = portNUM_PROCESSORS > 1 ? 0 : tskNO_AFFINITY;
  if (xTaskCreatePinnedToCore(taskEntry, "AppThinker", kStackBytes, this, 1, nullptr, core) != pdPASS) {
    LOG_ERR("APPS", "Could not start the thinking task (free heap %u)", ESP.getFreeHeap());
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
  // One tick for the idle task and its watchdog, about every thousand positions.
  vTaskDelay(1);
  return stop_.load(std::memory_order_acquire) || (timeLimitMs_ != 0 && millis() - startedMs_ >= timeLimitMs_);
}
