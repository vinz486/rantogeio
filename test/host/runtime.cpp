#include "Arduino.h"
#include "Preferences.h"
#include "freertos/queue.h"
#include "CASIO.h"
#include <deque>
#include <memory>
#include <mutex>
#include <functional>

namespace storage {
bool failWrites = false;
unsigned int writes = 0;
std::map<std::string, std::vector<uint8_t>> data;
}
struct HostQueue {
  size_t capacity, itemSize;
  std::deque<std::vector<uint8_t>> items;
  std::mutex mutex;
  HostQueue(size_t capacity, size_t itemSize) : capacity(capacity), itemSize(itemSize) {}
};
static std::vector<std::unique_ptr<HostQueue>> queues;
QueueHandle_t xQueueCreate(unsigned int count, size_t size) {
  queues.emplace_back(new HostQueue(count, size));
  return queues.back().get();
}
int xQueueSend(QueueHandle_t queue, const void* item, unsigned int) {
  std::lock_guard<std::mutex> lock(queue->mutex);
  if (queue->items.size() == queue->capacity) return 0;
  const auto* bytes = static_cast<const uint8_t*>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return 1;
}
int xQueueReceive(QueueHandle_t queue, void* item, unsigned int) {
  std::lock_guard<std::mutex> lock(queue->mutex);
  if (queue->items.empty()) return 0;
  memcpy(item, queue->items.front().data(), queue->itemSize);
  queue->items.pop_front();
  return 1;
}

int pinState[40] = {};
int64_t pulses[40] = {};
int64_t disabledPulses = 0;
uint64_t elapsedMicros = 0;
std::function<void()> duringMove;
void pinMode(int, int) {}
void digitalWrite(int pin, int value) {
  if (value == HIGH && pinState[pin] == LOW && (pin == 18 || pin == 21)) {
    ++pulses[pin];
    if (pinState[pin == 18 ? 19 : 22] == HIGH) ++disabledPulses;
  }
  pinState[pin] = value;
}
void delayMicroseconds(unsigned int us) {
  elapsedMicros += us;
  if (duringMove && (pulses[18] || pulses[21])) {
    auto callback = duringMove;
    duringMove = nullptr;
    callback();
  }
}
unsigned long millis() { return elapsedMicros / 1000; }
static time_t fakeNow = 1700000000;
extern "C" time_t time(time_t* out) noexcept {
  if (out) *out = fakeNow;
  return fakeNow;
}
extern "C" int settimeofday(const struct timeval* tv, const struct timezone*) noexcept {
  fakeNow = tv->tv_sec;
  return 0;
}
void configTime(long, int, const char*) {}
bool getLocalTime(struct tm* value) { return localtime_r(&fakeNow, value) != nullptr; }
void CASIO::set_casio(const String&) {}
void CASIO::beep(int) {}
void CASIO::beep() {}

#include "HTTPClient.h"
HostWiFi WiFi;
namespace http {
int status = -1;
String payload;
bool ended = false;
}
