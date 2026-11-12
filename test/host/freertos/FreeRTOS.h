#pragma once
#include <mutex>
#include <functional>
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
extern std::function<void()> afterCriticalExit;
inline void portEXIT_CRITICAL(portMUX_TYPE* mux) {
  mux->unlock();
  if (afterCriticalExit) {
    auto callback = std::move(afterCriticalExit);
    afterCriticalExit = nullptr;
    callback();
  }
}
constexpr int pdTRUE = 1;
