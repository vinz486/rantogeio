#include "ClockManager.h"
#include "NumberParsing.h"
#include <cassert>
#include <iostream>
#include <functional>
#include <thread>
#include <type_traits>

extern int64_t pulses[40], disabledPulses;
extern std::function<void()> duringMove;
void logger(const char*, ...) {}

static void resetHardware() {
  for (int i = 0; i < 40; ++i) pulses[i] = 0;
  disabledPulses = 0;
}

template<size_t N>
void testAxis() {
  for (int fraction = 0; fraction < 100; ++fraction) {
    MotionAxis<N> axis(100000 + fraction);
    int64_t total = 0;
    for (int i = 1; i <= 100 * int(N); ++i) {
      typename MotionAxis<N>::Move move;
      assert(axis.plan(move));
      assert(axis.position() == (i - 1) % int(N));
      total += move.steps;
      axis.commit(move);
      assert(total == int64_t(i) * (100000 + fraction) / 100);
      assert(axis.position() == i % int(N));
    }
  }
  for (int position : {0, 10, int(N) - 1}) {
    for (int delta : {-100, 100}) {
      MotionAxis<N> axis(100000);
      axis.set_position(position);
      int forward = 0;
      assert(axis.calibrate(delta, forward));
      int error = forward;
      for (int i = 1; i <= 2 * int(N) + 1; ++i) {
        typename MotionAxis<N>::Move move;
        assert(axis.plan(move));
        error += move.steps - 1000;
        axis.commit(move);
        assert(error == (i % N == 0 ? delta : 0));
      }
    }
    MotionAxis<N> axis(100000);
    axis.set_position(position);
    int forward = 0;
    assert(axis.calibrate(-100, forward) && forward == 0);
    assert(axis.calibrate(-100, forward) && forward == 0);
    assert(axis.calibrate(100, forward) && forward == 0);
    int error = 0;
    for (int i = 1; i <= 2 * int(N); ++i) {
      typename MotionAxis<N>::Move move;
      assert(axis.plan(move));
      error += move.steps - 1000;
      axis.commit(move);
      assert(error == (i % N == 0 ? -100 : 0));
    }
  }
  MotionAxis<N> axis(100033);
  typename MotionAxis<N>::Move move;
  assert(axis.plan(move));
  axis.commit(move);
  const int remainder = axis.remainder();
  int32_t offsets[N] = {};
  offsets[2] = -1000;
  assert(!axis.configure(100033, offsets));
  assert(axis.remainder() == remainder);
  assert(axis.offsets()[2] == 0);
  offsets[2] = INT32_MAX;
  assert(!axis.configure(100033, offsets));
  int forward = 0;
  assert(!axis.calibrate(-1000, forward));
  assert(!axis.set_position(-1));
  assert(!axis.set_position(N));
  offsets[2] = 0;
  assert(axis.configure(100033, offsets));
  assert(axis.remainder() == remainder);
}

static ClockManager::Result completed(ClockManager& clock, uint32_t id) {
  ClockManager::Result result;
  assert(id && clock.result(id, result) && result.done);
  return result;
}

void testController() {
  static_assert(std::is_trivially_copyable<ClockManager::Command>::value, "Queue needs value messages");
  storage::data.clear();
  ClockManager clock;
  clock.set_logger(logger);
  clock.begin();
  ClockManager::Command config;
  config.action = ClockManager::Action::SETTINGS;
  config.settings.hour_target = config.settings.minute_target = 100033;
  auto id = clock.submit(config);
  ClockManager::Result status;
  assert(clock.result(id, status) && !status.done);
  assert(clock.snapshot().settings.minute_target == 1883200);
  clock.tick();
  assert(completed(clock, id).success);
  assert(clock.snapshot().settings.minute_target == 100033);

  storage::failWrites = true;
  config.settings.minute_target = 200050;
  id = clock.submit(config);
  clock.tick();
  assert(!completed(clock, id).success);
  assert(clock.snapshot().settings.minute_target == 100033);
  storage::failWrites = false;
  config.settings.minute_target = 100033;
  config.settings.minutes[1] = -1000;
  id = clock.submit(config);
  const auto writes = storage::writes;
  clock.tick();
  assert(!completed(clock, id).success && writes == storage::writes);

  for (bool hour : {false, true}) {
    resetHardware();
    const int cycle = hour ? 24 : 60;
    for (int lap = 0; lap < 5; ++lap) {
      id = clock.request(hour ? ClockManager::Action::FAST_HOURS : ClockManager::Action::FAST_MINUTES);
      clock.tick();
      assert(completed(clock, id).success);
      for (int i = 0; i < cycle; ++i) clock.tick();
      assert(pulses[hour ? 21 : 18] == int64_t((lap + 1) * cycle) * 100033 / 100);
    }
  }

  config.settings = MotionSettings{};
  config.settings.hour_target = config.settings.minute_target = 100000;
  id = clock.submit(config); clock.tick(); assert(completed(clock, id).success);
  id = clock.request(ClockManager::Action::CURRENT_TIME, 0, 0); clock.tick();
  id = clock.request(ClockManager::Action::DISPLAYED_TIME, 23, 59); clock.tick();
  resetHardware();
  uint32_t liveId = 0;
  duringMove = [&]() {
    const int64_t before = pulses[18] + pulses[21];
    std::thread web([&]() { liveId = clock.request(ClockManager::Action::LIVE_MINUTE, 1); });
    web.join();
    assert(pulses[18] + pulses[21] == before);
    assert(clock.result(liveId, status) && !status.done);
  };
  clock.tick();
  assert(pulses[18] == 1000 && pulses[21] == 1000);
  assert(clock.snapshot().hour == 0 && clock.snapshot().minute == 0);
  clock.tick();
  assert(completed(clock, liveId).success);
  assert(pulses[18] == 1100 && pulses[21] == 1000 && disabledPulses == 0);

  const auto before = clock.snapshot();
  storage::failWrites = true;
  id = clock.request(ClockManager::Action::LIVE_MINUTE, 1); clock.tick();
  assert(!completed(clock, id).success && pulses[18] == 1100);
  assert(clock.snapshot().settings.minutes[0] == before.settings.minutes[0]);
  storage::failWrites = false;
  id = clock.request(ClockManager::Action::DISPLAYED_TIME, 24, -1); clock.tick();
  assert(!completed(clock, id).success);
  assert(clock.snapshot().hour == 0 && clock.snapshot().minute == 0);

  clock.request_calibrate_hour(); clock.tick();
  resetHardware(); clock.tick();
  assert(pulses[21] == 32);
  id = clock.request_end_calibrate(); clock.tick();
  assert(completed(clock, id).success);
  clock.tick(); assert(pulses[21] == 32);
  clock.request_calibrate_hour(); id = clock.request_end_calibrate();
  clock.tick(); clock.tick();
  assert(completed(clock, id).success && pulses[21] == 32);

  // Minute setting uses the same arrival offset as normal/fast movements.
  clock.request_set_minutes(); clock.tick();
  resetHardware(); clock.tick();
  assert(pulses[18] == 900); // Transition to M01 compensates the live position correction on M00.
  id = clock.decrement_minute(); clock.tick(); // Mark the displayed zero.
  assert(completed(clock, id).success && clock.snapshot().minute == 0);
  clock.tick(); assert(pulses[18] == 900);

  uint32_t stale[8];
  for (int i = 0; i < 8; ++i) {
    stale[i] = clock.request_calibrate_hour();
    assert(stale[i]);
  }
  assert(!clock.request_calibrate_hour());
  id = clock.request_end_calibrate();
  assert(id && clock.request_end_calibrate() == id);
  clock.tick(); // Stop bypasses the full queue.
  assert(completed(clock, id).success && clock.snapshot().paused);
  resetHardware();
  for (int i = 0; i < 8; ++i) {
    clock.tick();
    assert(!completed(clock, stale[i]).success);
  }
  clock.tick();
  assert(pulses[18] == 0 && pulses[21] == 0);
  clock.request(ClockManager::Action::RESUME); clock.tick();
  assert(!clock.snapshot().paused);

  ClockManager rebooted;
  rebooted.set_logger(logger); rebooted.begin();
  assert(rebooted.snapshot().settings.minute_target == clock.snapshot().settings.minute_target);
  assert(rebooted.snapshot().settings.minutes[0] == clock.snapshot().settings.minutes[0]);

  // With no time source, both reference marks are needed before setting time.
  rebooted.request_set_minutes(); rebooted.tick();
  rebooted.decrement_minute(); rebooted.tick(); // Zero at traversal index 0.
  assert(rebooted.time_source == NONE);
  for (int i = 0; i < 5; ++i) rebooted.tick();
  rebooted.increment_minute(); rebooted.tick(); // Current digit is five after zero.
  assert(rebooted.time_source == BUTTONS && rebooted.snapshot().minute == 5);
  resetHardware(); rebooted.tick();
  assert(pulses[18] == 0 && pulses[21] == 0);
}

// Full UI settings use positional corrections too, including adjacent digits.
template<size_t N>
void testPositionEdits() {
  for (int current : {0, 10, int(N) - 1}) {
    for (int correction : {-100, 100}) {
      MotionAxis<N> axis(100033);
      axis.set_position(current);
      int32_t offsets[N] = {};
      offsets[current] = correction;
      offsets[(current + 1) % N] = 40;
      offsets[(current + N - 1) % N] = -30;
      int forward = -1;
      assert(axis.update(100033, offsets, forward));
      assert(forward == (correction > 0 ? correction : 0));
      int64_t total = forward;
      for (int i = 1; i <= 2 * int(N); ++i) {
        typename MotionAxis<N>::Move move;
        assert(axis.plan(move));
        total += move.steps;
        axis.commit(move);
        assert(total == int64_t(i) * 100033 / 100 + offsets[axis.position()]);
      }
    }
  }
  MotionAxis<N> axis(100000);
  axis.set_position(10);
  int32_t offsets[N] = {};
  offsets[9] = 100;
  int forward = -1;
  assert(axis.update(100000, offsets, forward) && forward == 0);
  int total = 0;
  for (int i = 1; i <= 2 * int(N); ++i) {
    typename MotionAxis<N>::Move move;
    assert(axis.plan(move));
    total += move.steps;
    axis.commit(move);
    assert(total == i * 1000 + offsets[axis.position()]);
  }
}

void testSettingsAndStop() {
  storage::data.clear();
  ClockManager clock;
  clock.set_logger(logger); clock.begin();
  clock.set_current_time(0, 0, 0);
  clock.request_end_calibrate(); clock.tick();
  assert(clock.snapshot().paused);
  ClockManager::Command config;
  config.action = ClockManager::Action::SETTINGS;
  config.settings.hour_target = config.settings.minute_target = 100000;
  config.settings.hours[0] = 80;
  config.settings.minutes[0] = 120;
  resetHardware();
  auto id = clock.submit(config); clock.tick();
  assert(completed(clock, id).success);
  assert(clock.snapshot().paused);
  assert(pulses[21] == 80 && pulses[18] == 120);
  assert(clock.snapshot().settings.minutes[1] == 0);
  // Identical save cannot move the current digit twice.
  id = clock.submit(config); clock.tick();
  assert(completed(clock, id).success && pulses[18] == 120);
  config.settings.minutes[0] = 220;
  storage::failWrites = true;
  id = clock.submit(config); clock.tick();
  assert(!completed(clock, id).success && pulses[18] == 120);
  storage::failWrites = false;
  config.settings.minutes[0] = -100;
  id = clock.submit(config); clock.tick();
  assert(completed(clock, id).success && pulses[18] == 120);
  clock.request(ClockManager::Action::FAST_MINUTES); clock.tick();
  for (int i = 0; i < 60; ++i) clock.tick();
  assert(pulses[18] == 60000 - 100); // Only M00 is now 100 behind its original position.
  clock.request(ClockManager::Action::FAST_MINUTES); clock.tick();
  for (int i = 0; i < 10; ++i) clock.tick();
  resetHardware();
  uint32_t stop = 0, stale = 0;
  duringMove = [&]() {
    stale = clock.request_calibrate_hour();
    stop = clock.request_end_calibrate();
  };
  clock.tick(); // Finish the currently executing digit without losing position.
  assert(pulses[18] == 1000 && clock.snapshot().minute == 11);
  clock.tick();
  assert(completed(clock, stop).success && clock.snapshot().paused);
  clock.tick(); assert(!completed(clock, stale).success);
  for (int i = 0; i < 10; ++i) clock.tick();
  assert(pulses[18] == 1000 && pulses[21] == 0);
  clock.request(ClockManager::Action::RESUME); clock.tick();
  clock.tick();
  assert(pulses[18] == 2000 && !clock.snapshot().paused);
}

#include "TimeZoneManager.h"
#include "HTTPClient.h"
#include <cstdarg>
static std::string lastLog;
void captureLog(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  lastLog = buffer;
}
void testTimeZone() {
  TimeZoneManager tz;
  tz.set_logger(captureLog);
  tz.set("Europe/Rome");
  Preferences prefs;
  prefs.begin("time", true);
  assert(prefs.getString("tz_name", "") == "Europe/Rome");
  const auto saved = prefs.getString("tz", "");
  assert(!saved.empty());
  const auto writes = storage::writes;
  http::status = -7;
  tz.set_from_api();
  assert(http::ended && lastLog.find("-7") != std::string::npos);
  assert(storage::writes == writes && prefs.getString("tz", "") == saved);
  http::status = 500;
  http::payload = "Internal Server Error";
  tz.set_from_api();
  assert(http::ended && storage::writes == writes);
  http::status = 200;
  http::payload = " Europe/London\n";
  tz.set_from_api();
  assert(prefs.getString("tz_name", "") == "Europe/London");
  assert(prefs.getString("tz", "") != saved);
}

int main() {
  testAxis<24>(); testAxis<60>();
  testPositionEdits<24>(); testPositionEdits<60>();
  MotionSettings legacy;
  legacy.version = 1;
  legacy.minutes[10] = 100; legacy.minutes[11] = -100;
  assert(upgradeMotionSettings(legacy) && legacy.version == 2);
  assert(legacy.minutes[10] == 100 && legacy.minutes[11] == 0);
  int32_t value;
  assert(parseTarget("18874,50", value) && value == 1887450);
  assert(parseTarget("7872.33", value) && value == 787233);
  assert(parseTarget("21474836.47", value) && value == INT32_MAX);
  for (const char* bad : {"", "0", "-1", "nan", "1e4", "12.345", "21474836.48", "123garbage"})
    assert(!parseTarget(bad, value));
  assert(parseInteger("-2147483648", value) && value == INT32_MIN);
  assert(!parseInteger("2147483648", value));
  assert(!parseInteger("12.5", value));
  StepperDriver driver;
  driver.begin(); resetHardware();
  assert(!driver.move(10, -1) && pulses[21] == 0);
  assert(driver.move(1003, 998) && pulses[21] == 1003 && pulses[18] == 998);
  assert(disabledPulses == 0);
  testController();
  testSettingsAndStop();
  testTimeZone();
  std::cout << "PASS: fractional carry, calibration, validation, pulse counts, queue ownership, persistence and Stop\n";
}
