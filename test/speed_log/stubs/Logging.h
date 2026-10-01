#pragma once

#include <atomic>

namespace fakelog {
inline std::atomic<int> errors{0};
}
#define LOG_ERR(...) (++fakelog::errors)
#define LOG_INF(...) ((void)0)
#define LOG_DBG(...) ((void)0)
