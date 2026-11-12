#include "ClockManager.h"
#include "NumberParsing.h"
#include <cassert>
#include <iostream>
#include <functional>
#include <thread>
#include <type_traits>

extern int64_t pulses[40], disabledPulses;
extern int pinModes[40];
extern unsigned int tones;
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

using Action = ClockManager::Action;

static ClockManager::Result execute(ClockManager& clock, Action action, int a = 0, int b = 0, int c = 0) {
  const auto id = clock.request(action, a, b, c);
  clock.tick();
  return completed(clock, id);
}

void testController() {
  static_assert(std::is_trivially_copyable<ClockManager::Command>::value, "Queue needs value messages");
  storage::data.clear();
  ClockManager clock;
  clock.set_logger(logger);
  clock.begin();
  assert(pinModes[16] == OUTPUT);
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

  const auto writesBeforeStaleSave = storage::writes;
  config.expectedMotionRevision = clock.snapshot().motionRevision - 1;
  id = clock.submit(config); clock.tick();
  assert(!completed(clock, id).success && storage::writes == writesBeforeStaleSave);
  config.expectedMotionRevision = clock.snapshot().motionRevision;

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

  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  for (bool hour : {false, true}) {
    resetHardware();
    const int cycle = hour ? 24 : 60;
    for (int lap = 0; lap < 5; ++lap) {
      id = clock.request(hour ? ClockManager::Action::FAST_HOURS : ClockManager::Action::FAST_MINUTES);
      clock.tick();
      assert(clock.result(id, status) && !status.done);
      for (int i = 0; i < cycle; ++i) clock.tick();
      assert(completed(clock, id).success);
      assert(pulses[hour ? 21 : 18] == int64_t((lap + 1) * cycle) * 100033 / 100);
    }
  }

  config.settings = MotionSettings{};
  config.settings.hour_target = config.settings.minute_target = 100000;
  id = clock.submit(config); clock.tick(); assert(completed(clock, id).success);
  id = clock.request(ClockManager::Action::CURRENT_TIME, 0, 0); clock.tick();
  id = clock.request(ClockManager::Action::DISPLAYED_TIME, 23, 59); clock.tick();
  assert(execute(clock, Action::RESUME).success);
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
  assert(pulses[18] == 0); // The procedure waits for an explicit step.
  assert(execute(clock, Action::ADJUST_MINUTE, -1).success);
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
  assert(!execute(clock, Action::RESUME).success); // Raw hour jog invalidated its reference.
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  assert(execute(clock, Action::RESUME).success);
  assert(!clock.snapshot().paused);

  ClockManager rebooted;
  rebooted.set_logger(logger); rebooted.begin();
  assert(rebooted.snapshot().settings.minute_target == clock.snapshot().settings.minute_target);
  assert(rebooted.snapshot().settings.minutes[0] == clock.snapshot().settings.minutes[0]);

  // With no time source, both reference marks are needed before setting time.
  rebooted.request_set_minutes(); rebooted.tick();
  rebooted.decrement_minute(); rebooted.tick(); // Zero at traversal index 0.
  assert(rebooted.snapshot().timeSource == NONE);
  for (int i = 0; i < 5; ++i) assert(execute(rebooted, Action::ADJUST_MINUTE, -1).success);
  assert(execute(rebooted, Action::SET_MINUTES).success); // Mark current digit five after zero.
  assert(rebooted.snapshot().timeSource == BUTTONS && rebooted.snapshot().minute == 5);
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
  assert(execute(clock, Action::CURRENT_TIME, 0, 0).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
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
  const auto cycle = clock.request(ClockManager::Action::FAST_MINUTES); clock.tick();
  for (int i = 0; i < 60; ++i) clock.tick();
  assert(clock.snapshot().paused && completed(clock, cycle).success);
  assert(pulses[18] == 60000 - 100); // Only M00 is now 100 behind its original position.
  const auto interrupted = clock.request(ClockManager::Action::FAST_MINUTES); clock.tick();
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
  assert(!completed(clock, interrupted).success);
  clock.tick(); assert(!completed(clock, stale).success);
  for (int i = 0; i < 10; ++i) clock.tick();
  assert(pulses[18] == 1000 && pulses[21] == 0);
  assert(execute(clock, Action::CURRENT_TIME, 0, 12).success);
  assert(execute(clock, Action::RESUME).success);
  clock.tick();
  assert(pulses[18] == 2000 && !clock.snapshot().paused);
}

void testCommandFailures() {
  storage::data.clear();
  ClockManager clock;
  clock.begin();
  const auto stop = clock.request_end_calibrate();
  const auto staleResume = clock.request(ClockManager::Action::RESUME);
  assert(clock.request_end_calibrate() == stop);
  clock.tick(); clock.tick();
  assert(clock.snapshot().paused && !completed(clock, staleResume).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  const auto cycle = clock.request(ClockManager::Action::FAST_HOURS);
  clock.tick();
  const auto reference = clock.request(ClockManager::Action::DISPLAYED_TIME, 0, 0);
  clock.tick();
  assert(completed(clock, reference).success && !completed(clock, cycle).success);

  const time_t before = time(nullptr);
  auto id = clock.request(ClockManager::Action::CURRENT_DATE, 29, 2, 2025);
  clock.tick();
  assert(!completed(clock, id).success && time(nullptr) == before);
  id = clock.request(ClockManager::Action::CURRENT_DATE, 29, 2, 2024);
  clock.tick();
  assert(completed(clock, id).success);

  ClockManager::Command command;
  command.action = ClockManager::Action::SETTINGS;
  for (auto& offset : command.settings.hours) offset = 50000;
  const auto writes = storage::writes;
  resetHardware();
  id = clock.submit(command); clock.tick();
  assert(!completed(clock, id).success && storage::writes == writes && pulses[21] == 0);

  command.action = ClockManager::Action::CASIO_MODE;
  strcpy(command.text, "on");
  storage::failWrites = true;
  id = clock.submit(command); clock.tick();
  assert(!completed(clock, id).success && strcmp(clock.snapshot().casioMode, "off") == 0);
  storage::failWrites = false;
  id = clock.submit(command); clock.tick();
  assert(completed(clock, id).success && strcmp(clock.snapshot().casioMode, "on") == 0);
  assert(execute(clock, Action::CURRENT_TIME, 2, 59).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 2, 59).success);
  assert(execute(clock, Action::RESUME).success);
  tones = 0;
  clock.tick();
  assert(tones == 0);
  advanceWallTime(60);
  clock.tick();
  assert(clock.snapshot().hour == 3 && tones == 2);
  clock.tick();
  assert(tones == 2);
}

void testClockRecovery() {
  storage::data.clear();
  ClockManager clock;
  clock.begin();
  resetHardware();
  assert(clock.snapshot().paused && !clock.snapshot().hourReferenced && !clock.snapshot().minuteReferenced);
  assert(!execute(clock, Action::RESUME).success);
  assert(execute(clock, Action::CURRENT_TIME, 9, 0).success);
  assert(!execute(clock, Action::RESUME).success);
  clock.tick();
  assert(pulses[18] == 0 && pulses[21] == 0);

  assert(execute(clock, Action::DISPLAYED_TIME, 9, 0).success);
  assert(execute(clock, Action::RESUME).success);
  clock.tick();
  assert(execute(clock, Action::STOP).success);
  advanceWallTime(51 * 60);
  assert(execute(clock, Action::RESUME).success);
  for (int i = 0; i < 51; ++i) clock.tick();
  assert(clock.snapshot().minute == 51 && pulses[18] == 51 * 18832);

  resetHardware();
  assert(execute(clock, Action::CURRENT_TIME, 8, 46).success);
  clock.tick();
  assert(pulses[18] == 0 && pulses[21] == 0);
  assert(clock.snapshot().hour == 9 && clock.snapshot().minute == 51);

  clock.press_jog(true);
  clock.release_jog(true); // Release while the press is still queued.
  clock.tick(); clock.tick();
  assert(clock.snapshot().hourReferenced && pulses[21] == 0);
  clock.press_jog(false);
  clock.tick(); clock.tick();
  assert(!clock.snapshot().minuteReferenced && pulses[18] == 32);
  clock.release_jog(true); // A release from the other button cannot end this jog.
  clock.tick();
  assert(pulses[18] == 64);
  clock.release_jog(false);
  clock.tick(); clock.tick();
  assert(pulses[18] == 64 && !execute(clock, Action::RESUME).success);
}

void testTimeSourcesAndMinuteMarks() {
  storage::data.clear();
  ClockManager clock;
  clock.begin();
  clock.start_ntp();
  assert(execute(clock, Action::CURRENT_DATE, 29, 2, 2024).success);
  assert(clock.snapshot().timeSource == NONE && networkTimeEnabled() && !execute(clock, Action::RESUME).success);
  assert(execute(clock, Action::SET_MINUTES).success);
  resetHardware();
  clock.tick();
  assert(pulses[18] == 0);

  uint32_t beforeStep = 0;
  assert(clock.capture_minute_input(beforeStep));
  duringMove = [&] {
    uint32_t movementGeneration;
    assert(!clock.capture_minute_input(movementGeneration));
    assert(!clock.request_minute_input(Action::ADJUST_MINUTE, 1, movementGeneration));
  };
  assert(execute(clock, Action::ADJUST_MINUTE, -1).success);
  assert(!clock.request_minute_input(Action::ADJUST_MINUTE, 1, beforeStep));
  assert(!clock.snapshot().minuteReferenced && pulses[18] == 18832);

  const struct timeval network = {1700000000, 0};
  clock.submit_ntp_sample(network);
  delay(2000); // Compensation for time spent waiting for the motor/loop.
  clock.tick();
  assert(clock.snapshot().timeSource == SNTP && time(nullptr) == network.tv_sec + 2);
  const time_t beforeMark = time(nullptr);
  assert(execute(clock, Action::ADJUST_MINUTE, 1).success);
  assert(clock.snapshot().minuteReferenced && clock.snapshot().minute == 0);
  assert(clock.snapshot().timeSource == SNTP && time(nullptr) == beforeMark);
  assert(!execute(clock, Action::RESUME).success); // Hours still unreferenced.
  assert(execute(clock, Action::REFERENCE_HOUR).success);
  assert(clock.snapshot().hourReferenced && !clock.snapshot().paused);

  assert(execute(clock, Action::CURRENT_TIME, 15, 37).success);
  assert(!networkTimeEnabled());
  const auto manual = time(nullptr);
  clock.submit_ntp_sample(network);
  clock.tick();
  assert(clock.snapshot().timeSource == WEB && time(nullptr) == manual);
  assert(execute(clock, Action::USE_NTP).success);
  assert(clock.snapshot().timeSource == WEB && networkTimeEnabled());
  clock.submit_ntp_sample(network);
  clock.tick();
  assert(clock.snapshot().timeSource == SNTP && time(nullptr) == network.tv_sec);
}

void testLegacyTargetsAndDeferredOffset() {
  storage::data.clear();
  Preferences prefs;
  assert(prefs.begin("base-steps", false));
  prefs.putInt("hour", 8000);
  prefs.putInt("minute", 18874);
  prefs.putInt("hour_tgt_x100", 800033);
  prefs.end();
  ClockManager clock;
  clock.begin();
  assert(clock.snapshot().settings.hour_target == 800033);
  assert(clock.snapshot().settings.minute_target == 1887400);
  assert(execute(clock, Action::CURRENT_TIME, 0, 0).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  assert(execute(clock, Action::LIVE_MINUTE, 1).success);
  assert(execute(clock, Action::LIVE_MINUTE, -1).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  assert(execute(clock, Action::RESUME).success);
  resetHardware();
  advanceWallTime(60);
  clock.tick();
  assert(pulses[18] == 18774 && clock.snapshot().minute == 1);
}

void testStopInterleavingAndMaintenance() {
  storage::data.clear();
  ClockManager clock;
  clock.begin();
  assert(execute(clock, Action::CURRENT_TIME, 0, 0).success);
  assert(execute(clock, Action::DISPLAYED_TIME, 0, 0).success);
  assert(execute(clock, Action::RESUME).success);

  uint32_t firstStop = 0, resume = 0, secondStop = 0;
  extern std::function<void()> beforeQueueReceive;
  beforeQueueReceive = [&] {
    firstStop = clock.request_end_calibrate();
    resume = clock.request(Action::RESUME);
    afterCriticalExit = [&] { secondStop = clock.request_end_calibrate(); };
  };
  clock.tick();
  assert(secondStop == firstStop && completed(clock, firstStop).success);
  assert(!completed(clock, resume).success && clock.snapshot().paused);
  const auto laterStop = clock.request_end_calibrate();
  clock.tick();
  assert(laterStop != firstStop && completed(clock, firstStop).success && completed(clock, laterStop).success);

  assert(execute(clock, Action::RESUME).success);
  advanceWallTime(60);
  duringMove = [&] {
    clock.request_maintenance();
    assert(!clock.maintenance_ready());
    assert(!clock.request(Action::RESUME));
  };
  resetHardware();
  clock.tick();
  assert(pulses[18] == 18832 && !clock.maintenance_ready());
  clock.tick();
  assert(clock.maintenance_ready() && clock.snapshot().paused);
  clock.end_maintenance();
  clock.request_maintenance();
  assert(!clock.maintenance_ready());
  clock.tick();
  assert(clock.maintenance_ready() && pulses[18] == 18832);
  clock.end_maintenance();
  clock.tick();
  assert(!clock.maintenance_ready() && !clock.snapshot().paused);
}

#include "TimeZoneManager.h"
#include "HTTPClient.h"
#include "WifiManager.h"
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
  assert(tz.set("Europe/Rome") == TimeZoneManager::Result::OK);
  Preferences prefs;
  prefs.begin("time", true);
  assert(prefs.getString("tz_name", "") == "Europe/Rome");
  const std::string saved = getenv("TZ");
  assert(saved == "CET-1CEST,M3.5.0,M10.5.0/3");
  const auto writes = storage::writes;
  http::status = -7;
  assert(tz.fetch_from_api().isEmpty());
  assert(http::ended && lastLog.find("-7") != std::string::npos);
  assert(storage::writes == writes && getenv("TZ") == saved);
  http::status = 500;
  http::payload = "Internal Server Error";
  assert(tz.fetch_from_api().isEmpty());
  assert(http::ended && storage::writes == writes);
  http::status = 200;
  http::payload = "Unknown/Zone";
  assert(tz.set(tz.fetch_from_api()) == TimeZoneManager::Result::UNKNOWN_ZONE);
  assert(storage::writes == writes && getenv("TZ") == saved);
  storage::failWrites = true;
  assert(tz.set("Europe/London") == TimeZoneManager::Result::SAVE_FAILED);
  assert(storage::writes == writes && getenv("TZ") == saved);
  storage::failWrites = false;
  http::status = 200;
  http::payload = " Europe/London\n";
  assert(tz.set(tz.fetch_from_api()) == TimeZoneManager::Result::OK);
  assert(prefs.getString("tz_name", "") == "Europe/London");
  assert(getenv("TZ") != saved);
}

void testDstRollback() {
  TimeZoneManager zone;
  assert(zone.set("Europe/Rome") == TimeZoneManager::Result::OK);
  ClockManager clock;
  clock.begin();
  clock.start_ntp();
  struct tm local{};
  local.tm_year = 124;
  local.tm_mon = 9;
  local.tm_mday = 27;
  local.tm_hour = 2;
  local.tm_min = 58;
  local.tm_isdst = 1;
  const struct timeval sample = {mktime(&local), 0};
  clock.submit_ntp_sample(sample);
  clock.tick();
  assert(execute(clock, Action::DISPLAYED_TIME, 2, 58).success);
  assert(execute(clock, Action::RESUME).success);
  clock.tick();
  resetHardware();
  advanceWallTime(120); // 02:58 DST -> 02:00 standard with 02:59 not observed.
  clock.tick(); clock.tick();
  assert(clock.snapshot().hour == 2 && clock.snapshot().minute == 0);
  assert(pulses[21] == 0 && pulses[18] == 2 * 18832);
  assert(zone.set("America/Asuncion") == TimeZoneManager::Result::OK);
  assert(strcmp(getenv("TZ"), "<-03>3") == 0);
  assert(zone.set("Africa/Casablanca") == TimeZoneManager::Result::UNKNOWN_ZONE);
  assert(zone.set("UTC") == TimeZoneManager::Result::OK);
}

void testQueuedTimeZoneAndDate() {
  TimeZoneManager zone;
  assert(zone.set("UTC") == TimeZoneManager::Result::OK);
  ClockManager clock;
  clock.begin();
  clock.set_time_zone_manager(zone);
  assert(execute(clock, Action::CURRENT_DATE, 30, 3, 2024).success);
  assert(execute(clock, Action::CURRENT_TIME, 1, 30).success);
  const auto before = time(nullptr);
  ClockManager::Command command;
  command.action = Action::TIME_ZONE;
  strcpy(command.text, "Europe/Rome");
  const auto tz = clock.submit(command);
  const auto date = clock.request(Action::CURRENT_DATE, 31, 3, 2024);
  assert(strcmp(getenv("TZ"), "UTC0") == 0);
  clock.tick();
  assert(completed(clock, tz).success && strcmp(getenv("TZ"), "CET-1CEST,M3.5.0,M10.5.0/3") == 0);
  clock.tick(); // Rome's 02:30 does not exist on this date.
  assert(!completed(clock, date).success && time(nullptr) == before);
  assert(zone.set("UTC") == TimeZoneManager::Result::OK);
}

void testWifi() {
  storage::data.clear();
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", "Legacy");
  prefs.putString("password", "old-password");
  prefs.end();

  WifiManager wifi;
  wifi.begin();
  assert(wifi.connecting() && WiFi.ssid == "Legacy" && WiFi.password == "old-password");
  assert(!WiFi.persistentSettings && WiFi.currentMode == WIFI_STA);
  delay(10000);
  wifi.loop();
  assert(!wifi.connecting() && WiFi.currentMode == WIFI_AP);
  delay(30000);
  wifi.loop();
  assert(WiFi.reconnects == 0 && WiFi.attempts == 2 && WiFi.currentMode == WIFI_AP_STA);

  assert(wifi.set_credentials("Open network", ""));
  wifi.loop();
  assert(WiFi.attempts == 2 && wifi.saved_ssid() == "Open network");
  delay(1000);
  wifi.loop();
  assert(wifi.connecting() && WiFi.attempts == 3 && WiFi.currentMode == WIFI_AP_STA);
  assert(WiFi.ssid == "Open network" && WiFi.password.empty());
  WiFi.connection = WL_CONNECTED;
  WiFi.connectedSsid = "Legacy";
  wifi.loop();
  assert(wifi.connecting() && WiFi.currentMode == WIFI_AP_STA);
  WiFi.connectedSsid = "Open network";
  wifi.loop();
  assert(!wifi.connecting() && WiFi.currentMode == WIFI_STA && WiFi.autoReconnect);
  WiFi.connection = 0;
  wifi.loop();
  delay(30000);
  wifi.loop();
  assert(WiFi.reconnects == 1);

  assert(!wifi.set_credentials("Another network", "short"));
  assert(!WifiManager::valid_credentials("SSID", String(std::string(64, 'g'))));
  assert(WifiManager::valid_credentials("SSID", String(std::string(64, 'f'))));
  storage::failWrites = true;
  assert(!wifi.set_credentials("Another network", "new-password"));
  storage::failWrites = false;
  delay(1000);
  wifi.loop();
  assert(wifi.saved_ssid() == "Open network" && WiFi.attempts == 3);
  delay(30000);
  wifi.loop();
  assert(wifi.connecting() && WiFi.currentMode == WIFI_AP_STA && WiFi.attempts == 4);
}

void testOta();

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
  testCommandFailures();
  testClockRecovery();
  testTimeSourcesAndMinuteMarks();
  testLegacyTargetsAndDeferredOffset();
  testStopInterleavingAndMaintenance();
  testTimeZone();
  testDstRollback();
  testQueuedTimeZoneAndDate();
  testWifi();
  testOta();
  std::cout << "PASS: clock recovery, time sources, calibration, persistence, Stop, Wi-Fi and OTA\n";
}
