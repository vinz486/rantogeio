#include "ClockManager.h"
#include "TimeZoneManager.h"
#include "settings.h"
#include <esp_netif.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <cstring>
#include <limits>

namespace {
constexpr int HOUR_CYCLE = SIMULATE_12_HOUR == 1 ? 12 : 24;
constexpr int MINUTE_CYCLE = 60;

int64_t civilMinute(const struct tm& local) {
  const int64_t year = int64_t(local.tm_year) + 1899;
  const int64_t days = year * 365 + year / 4 - year / 100 + year / 400 + local.tm_yday;
  return days * 1440 + local.tm_hour * 60 + local.tm_min;
}

int64_t nearestPosition(int64_t target, int position, int cycle) {
  const int behind = (target % cycle - position % cycle + cycle) % cycle;
  return target + (behind <= cycle / 2 ? -behind : cycle - behind);
}

void discardFullCycles(int64_t target, int64_t& position, int cycle) {
  position += ((target - position) / cycle) * cycle;
}
}

void ClockManager::begin(int32_t motionRevision) {
  _motionRevision = motionRevision;
  _commands = xQueueCreate(8, sizeof(Command));
  _stepper.begin();
  _casio.begin();
  load_settings();
  Preferences prefs;
  prefs.begin("time", true);
  const int32_t casio = prefs.getInt("casio_config", -1);
  if (casio >= 0) {
    _calibration_hint = (casio & 4) != 0;
    _casioMode = (casio & 3) == 1 ? "on" : (casio & 3) == 2 ? "day" : "off";
  } else {
    _calibration_hint = prefs.getBool("cal_hint", false);
    const String legacyMode = prefs.getString("casio", "off");
    if (legacyMode == "on" || legacyMode == "day") _casioMode = legacyMode;
  }
  _casio.set_casio(_casioMode);
  prefs.end();
  publish();
}

uint32_t ClockManager::submit(Command command) {
  portENTER_CRITICAL(&_mutex);
  // Stop has its own mailbox/result: it never needs space in the normal queue.
  if (command.action == Action::STOP) ++_epoch;
  if (command.action == Action::STOP && _stopPending) {
    const uint32_t id = _stopResults[_stopSlot].id;
    portEXIT_CRITICAL(&_mutex);
    return id;
  }
  const uint32_t id = _nextId++;
  if (_nextId > INT32_MAX) _nextId = 1;
  if (command.action == Action::STOP) {
    _stopSlot = (_stopSlot + 1) % STOP_RESULT_COUNT;
    _stopResults[_stopSlot] = Result{};
    _stopResults[_stopSlot].id = id;
    _stopPending = true;
    portEXIT_CRITICAL(&_mutex);
    return id;
  }
  if (_maintenance.load() != Maintenance::OFF) {
    portEXIT_CRITICAL(&_mutex);
    return 0;
  }
  if ((command.action == Action::SET_MINUTES || command.action == Action::ADJUST_MINUTE) &&
      (_minuteMoving || (command.capturedMinuteGeneration && command.minuteGeneration != _minuteGeneration))) {
    portEXIT_CRITICAL(&_mutex);
    return 0;
  }
  Result& slot = _results[id % RESULT_COUNT];
  if (!_commands || (slot.id && !slot.done)) {
    portEXIT_CRITICAL(&_mutex);
    return 0;
  }
  slot = Result{};
  slot.id = id;
  command.epoch = _epoch;
  command.minuteGeneration = _minuteGeneration;
  portEXIT_CRITICAL(&_mutex);
  command.id = id;
  if (xQueueSend(_commands, &command, 0) != pdTRUE) {
    finish(id, "Command queue full. Please retry.");
    return 0;
  }
  return id;
}

void ClockManager::press_jog(bool hour) {
  const int axis = hour ? 0 : 1;
  portENTER_CRITICAL(&_mutex);
  _jogHeld[axis] = true;
  const uint32_t generation = ++_jogGeneration[axis];
  portEXIT_CRITICAL(&_mutex);
  request(hour ? Action::CALIBRATE_HOUR : Action::CALIBRATE_MINUTE, 1, int(generation));
}

void ClockManager::release_jog(bool hour) {
  const int axis = hour ? 0 : 1;
  portENTER_CRITICAL(&_mutex);
  _jogHeld[axis] = false;
  ++_jogGeneration[axis];
  portEXIT_CRITICAL(&_mutex);
}

bool ClockManager::capture_minute_input(uint32_t& generation) {
  portENTER_CRITICAL(&_mutex);
  generation = _minuteGeneration;
  const bool stationary = !_minuteMoving;
  portEXIT_CRITICAL(&_mutex);
  return stationary;
}

uint32_t ClockManager::request_minute_input(Action action, int direction, uint32_t generation) {
  Command command;
  command.action = action;
  command.a = direction;
  command.minuteGeneration = generation;
  command.capturedMinuteGeneration = true;
  return submit(command);
}

uint32_t ClockManager::request(Action action, int a, int b, int c) {
  Command command;
  command.action = action;
  command.a = a;
  command.b = b;
  command.c = c;
  const uint32_t id = submit(command);
  if (!id && _logger) (*_logger)("Command unavailable; try again when the digit is stationary");
  return id;
}

bool ClockManager::result(uint32_t id, Result& out) {
  portENTER_CRITICAL(&_mutex);
  out = _results[id % RESULT_COUNT];
  for (const auto& stop : _stopResults) {
    if (stop.id == id) {
      out = stop;
      break;
    }
  }
  portEXIT_CRITICAL(&_mutex);
  return id != 0 && out.id == id;
}

ClockManager::Snapshot ClockManager::snapshot() {
  portENTER_CRITICAL(&_mutex);
  Snapshot copy = _snapshot;
  portEXIT_CRITICAL(&_mutex);
  return copy;
}

void ClockManager::finish(uint32_t id, const char* error) {
  portENTER_CRITICAL(&_mutex);
  auto& stop = _stopResults[_stopSlot];
  Result& slot = id == stop.id ? stop : _results[id % RESULT_COUNT];
  if (id == stop.id) _stopPending = false;
  slot.done = true;
  slot.success = !error;
  slot.motionRevision = _snapshot.motionRevision;
  snprintf(slot.message, sizeof(slot.message), "%s", error ? error : "OK");
  portEXIT_CRITICAL(&_mutex);
  if (error && _logger) (*_logger)("Command %u failed: %s", id, error);
}

void ClockManager::finish_cycle(const char* error) {
  if (!_cycleId) return;
  if (error && _state == FAST_FORWARD_HOURS) _hourCursorKnown = false;
  if (error && _state == FAST_FORWARD_MINUTES) _minuteCursorKnown = false;
  _state = RUN;
  finish(_cycleId, error);
  _cycleId = 0;
}

MotionSettings ClockManager::settings() const {
  MotionSettings config;
  config.hour_target = _hours.target();
  config.minute_target = _minutes.target();
  memcpy(config.hours, _hours.offsets(), sizeof(config.hours));
  memcpy(config.minutes, _minutes.offsets(), sizeof(config.minutes));
  return config;
}

void ClockManager::publish() {
  Snapshot copy;
  copy.settings = settings();
  copy.hour = _hours.position();
  copy.minute = _minutes.position();
  copy.paused = _paused;
  copy.hourReferenced = _hourReferenced;
  copy.minuteReferenced = _minuteReferenced;
  copy.timeSource = _timeSource;
  copy.calibrationHint = _calibration_hint;
  copy.motionRevision = _motionRevision;
  snprintf(copy.casioMode, sizeof(copy.casioMode), "%s", _casioMode.c_str());
  portENTER_CRITICAL(&_mutex);
  copy.stateRevision = _snapshot.stateRevision;
  if (copy.paused != _snapshot.paused || copy.hourReferenced != _snapshot.hourReferenced ||
      copy.minuteReferenced != _snapshot.minuteReferenced || copy.timeSource != _snapshot.timeSource) {
    ++copy.stateRevision;
  }
  _snapshot = copy;
  portEXIT_CRITICAL(&_mutex);
}

void ClockManager::update_motion_revision() {
  _motionRevision = _motionRevision == INT32_MAX ? 1 : _motionRevision + 1;
}

bool ClockManager::configure(const MotionSettings& config) {
  auto hours = _hours;
  auto minutes = _minutes;
  if (config.version != 2 || !hours.configure(config.hour_target, config.hours) ||
      !minutes.configure(config.minute_target, config.minutes)) return false;
  _hours = hours;
  _minutes = minutes;
  return true;
}

void ClockManager::load_settings() {
  MotionSettings config;
  Preferences prefs;
  if (prefs.begin("motion", true)) {
    if (prefs.getBytesLength("config") == sizeof(config) &&
        prefs.getBytes("config", &config, sizeof(config)) == sizeof(config)) {
      prefs.end();
      if ((!upgradeMotionSettings(config) || !configure(config)) && _logger) (*_logger)("Invalid motion settings; using defaults");
      return;
    }
    prefs.end();
  }
  // Preserve earlier transition tables as equivalent positional corrections.
  config.version = 1;
  if (prefs.begin("base-steps", true)) {
    const int32_t legacyHour = prefs.getInt("hour", 0);
    const int32_t legacyMinute = prefs.getInt("minute", 0);
    if (legacyHour > 0 && legacyHour <= INT32_MAX / 100) config.hour_target = legacyHour * 100;
    if (legacyMinute > 0 && legacyMinute <= INT32_MAX / 100) config.minute_target = legacyMinute * 100;
    config.hour_target = prefs.getInt("hour_tgt_x100", config.hour_target);
    config.minute_target = prefs.getInt("minute_tgt_x100", config.minute_target);
    prefs.end();
  }
  if (prefs.begin("offsets", true)) {
    for (int i = 0; i < 60; ++i) {
      char key[5];
      snprintf(key, sizeof(key), "m%d", i);
      config.minutes[i] = prefs.getInt(key, 0);
      if (i < 24) {
        snprintf(key, sizeof(key), "h%d", i);
        config.hours[i] = prefs.getInt(key, 0);
      }
    }
    prefs.end();
  }
  if ((!upgradeMotionSettings(config) || !configure(config)) && _logger) (*_logger)("Invalid motion settings; using defaults");
}

bool ClockManager::persist(const MotionSettings& config) {
  // One record avoids saving a new base with old fractional targets/offsets.
  Preferences prefs;
  if (!prefs.begin("motion", false)) return false;
  const bool saved = prefs.putBytes("config", &config, sizeof(config)) == sizeof(config);
  prefs.end();
  return saved;
}

const char* ClockManager::execute(const Command& command) {
  switch (command.action) {
    case Action::STOP:
      if (_logger && (_state == CALIBRATE_HOUR || _state == CALIBRATE_MINUTE))
        (*_logger)("Calibration done for %llu steps", (unsigned long long)_calibratedSteps);
      _paused = true;
      _lastCivilMinute = -1;
      finish_cycle("Cycle cancelled by Stop.");
      _state = RUN;
      return nullptr;
    case Action::REFERENCE_HOUR:
      if (!_hourReferenced) {
        _hours.rebase_position(0);
        _hourReferenced = true;
        if (!_minuteReferenced || _timeSource == NONE || _state == SET_MINUTES) return nullptr;
      }
      // The physical gesture also resumes a fully referenced clock.
      // fall through
    case Action::RESUME:
      if (!_hourReferenced || !_minuteReferenced) return "Confirm the displayed hour and minute before resuming.";
      if (_timeSource == NONE) return "Set the time or wait for network time before resuming.";
      finish_cycle("Cycle interrupted by Resume.");
      _state = RUN;
      _paused = false;
      return nullptr;
    case Action::CALIBRATE_HOUR:
    case Action::CALIBRATE_MINUTE: {
      if (command.a) {
        const int axis = command.action == Action::CALIBRATE_HOUR ? 0 : 1;
        portENTER_CRITICAL(&_mutex);
        const bool held = _jogHeld[axis] && _jogGeneration[axis] == uint32_t(command.b);
        portEXIT_CRITICAL(&_mutex);
        if (!held) return "Calibration button already released.";
      }
      finish_cycle("Cycle interrupted by calibration.");
      _state = command.action == Action::CALIBRATE_HOUR ? CALIBRATE_HOUR : CALIBRATE_MINUTE;
      _physicalJog = command.a != 0;
      _activeJogGeneration = uint32_t(command.b);
      _calibratedSteps = 0;
      return nullptr;
    }
    case Action::FAST_HOURS:
    case Action::FAST_MINUTES:
      if (_cycleId) return "A cycle is already running.";
      if (command.action == Action::FAST_HOURS ? !_hourReferenced : !_minuteReferenced)
        return "Confirm the displayed digit before starting a cycle.";
      _state = command.action == Action::FAST_HOURS ? FAST_FORWARD_HOURS : FAST_FORWARD_MINUTES;
      _remaining = _state == FAST_FORWARD_HOURS ? 24 : 60;
      _cycleId = command.id;
      return nullptr;
    case Action::SET_MINUTES:
      if (_state == SET_MINUTES) {
        if (command.minuteGeneration != _minuteGeneration) return "Wait until the minute digit is stationary.";
        return mark_minute(false);
      }
      finish_cycle("Cycle interrupted by minute setting.");
      start_setting_minutes();
      return nullptr;
    case Action::ADJUST_HOUR:
      if (!_hourReferenced) return "Confirm the displayed hour first.";
      if (_state == FAST_FORWARD_HOURS) finish_cycle("Cycle interrupted by a reference change.");
      _hours.adjust_position(command.a);
      if (_hourCursorKnown) _hourCursor += command.a;
      return nullptr;
    case Action::ADJUST_MINUTE:
      if (_state == SET_MINUTES) {
        if (command.minuteGeneration != _minuteGeneration) return "Wait until the minute digit is stationary.";
        return command.a > 0 ? mark_minute(true) : step_setting_minutes();
      }
      if (!_minuteReferenced) return "Confirm the displayed minute first.";
      if (_state == FAST_FORWARD_MINUTES) finish_cycle("Cycle interrupted by a reference change.");
      _minutes.adjust_position(command.a);
      if (_minuteCursorKnown) _minuteCursor += command.a;
      return nullptr;
    case Action::DISPLAYED_TIME:
      if (command.a < 0 || command.a >= 24 || command.b < 0 || command.b >= 60)
        return "Displayed time must be within 00:00 and 23:59.";
      finish_cycle("Cycle interrupted by a reference change.");
      _state = RUN;
      set_displayed_time(command.a, command.b);
      return nullptr;
    case Action::CURRENT_TIME:
      if (command.a < 0 || command.a >= 24 || command.b < 0 || command.b >= 60)
        return "Time must be within 00:00 and 23:59.";
      if (!set_current_time(command.a, command.b, 0)) return "Could not set the time.";
      return nullptr;
    case Action::CURRENT_DATE:
      return set_current_date(command.a, command.b, command.c);
    case Action::CASIO_MODE:
      return set_casio(command.text, command.a != 0) ? nullptr : "Could not save CASIO settings.";
    case Action::TIME_ZONE:
      if (!_timeZone) return "Time zone manager unavailable.";
      if (command.a && _timeZone->configured()) return nullptr;
      switch (_timeZone->set(command.text)) {
        case TimeZoneManager::Result::OK:
          _lastCivilMinute = -1;
          return nullptr;
        case TimeZoneManager::Result::UNKNOWN_ZONE:
          return "Unknown time zone.";
        case TimeZoneManager::Result::SAVE_FAILED:
          return "Could not save the time zone.";
      }
      return "Could not set the time zone.";
    case Action::USE_NTP:
      return start_ntp() ? nullptr : "Could not start network time.";
    case Action::SETTINGS: {
      if (_state != RUN) return "Stop calibration or fast forward before saving settings.";
      if (command.expectedMotionRevision && command.expectedMotionRevision != _motionRevision)
        return "Motion settings changed. Reload them before saving.";
      auto hours = _hours;
      auto minutes = _minutes;
      const MotionSettings& config = command.settings;
      int hourSteps = 0, minuteSteps = 0;
      const bool hoursValid = _hourReferenced ? hours.update(config.hour_target, config.hours, hourSteps)
                                              : hours.configure(config.hour_target, config.hours);
      const bool minutesValid = _minuteReferenced ? minutes.update(config.minute_target, config.minutes, minuteSteps)
                                                  : minutes.configure(config.minute_target, config.minutes);
      if (config.version != 2 || !hoursValid || !minutesValid)
        return "Invalid steps: each digit needs a positive movement within the duration limit.";
      if (hourSteps > StepperDriver::stepLimit(true, true) ||
          minuteSteps > StepperDriver::stepLimit(false, true))
        return "The immediate digit correction exceeds the duration limit.";
      if (!persist(config)) return "Could not save motion settings. Please retry.";
      if (!_stepper.move(hourSteps, minuteSteps, true)) return "Could not apply digit corrections.";
      _hours = hours;
      _minutes = minutes;
      update_motion_revision();
      return nullptr;
    }
    case Action::LIVE_HOUR:
    case Action::LIVE_MINUTE:
      if (_state != RUN) return "Stop calibration or fast forward before live adjustment.";
      if (command.action == Action::LIVE_HOUR ? !_hourReferenced : !_minuteReferenced)
        return "Confirm the displayed digit before applying an offset.";
      return live_calibrate(command.action == Action::LIVE_HOUR, command.a);
  }
  return "Unknown command.";
}

bool ClockManager::advance(bool hour, bool minute) {
  MotionAxis<24>::Move h{};
  MotionAxis<60>::Move m{};
  if ((hour && !_hours.plan(h)) || (minute && !_minutes.plan(m))) {
    if (_logger) (*_logger)("Invalid movement; position and fractional remainder unchanged");
    return false;
  }
  if (!_stepper.move(hour ? h.steps : 0, minute ? m.steps : 0)) return false;
  if (hour) _hours.commit(h);
  if (minute) _minutes.commit(m);
  if (_logger) (*_logger)("Advanced to %02d:%02d: hour=%d minute=%d microsteps",
                          _hours.position(), _minutes.position(), h.steps, m.steps);
  publish();
  return true;
}

const char* ClockManager::live_calibrate(bool hour, int direction) {
  if (direction != -1 && direction != 1) return "Direction must be -1 or 1.";
  const int delta = direction * 100;
  auto hours = _hours;
  auto minutes = _minutes;
  int forwardSteps = 0;
  if (!(hour ? hours.calibrate(delta, forwardSteps) : minutes.calibrate(delta, forwardSteps)))
    return "Adjustment would make a movement invalid.";
  if (forwardSteps > StepperDriver::stepLimit(hour, true))
    return "The immediate digit correction exceeds the duration limit.";
  MotionSettings config = settings();
  memcpy(config.hours, hours.offsets(), sizeof(config.hours));
  memcpy(config.minutes, minutes.offsets(), sizeof(config.minutes));
  if (!persist(config)) return "Could not save calibration. No adjustment applied.";
  if (!_stepper.move(hour ? forwardSteps : 0, hour ? 0 : forwardSteps, true))
    return "Could not execute calibration.";
  _hours = hours;
  _minutes = minutes;
  update_motion_revision();
  return nullptr;
}

bool ClockManager::handle_stop() {
  portENTER_CRITICAL(&_mutex);
  const uint32_t id = _stopPending ? _stopResults[_stopSlot].id : 0;
  portEXIT_CRITICAL(&_mutex);
  if (!id) return false;
  Command stop;
  stop.action = Action::STOP;
  execute(stop);
  if (_maintenanceApplied) _pausedBeforeMaintenance = true;
  publish();
  finish(id, nullptr);
  return true;
}

void ClockManager::request_maintenance() {
  portENTER_CRITICAL(&_mutex);
  if (_maintenance.load() == Maintenance::OFF) {
    _maintenance.store(Maintenance::REQUESTED);
    ++_epoch;
  }
  portEXIT_CRITICAL(&_mutex);
}

void ClockManager::end_maintenance() {
  _maintenance.store(Maintenance::OFF);
}

bool ClockManager::handle_maintenance() {
  const auto state = _maintenance.load();
  if (state != Maintenance::OFF) {
    if (!_maintenanceApplied) {
      _pausedBeforeMaintenance = _paused;
      finish_cycle("Cycle interrupted by firmware update.");
      _state = RUN;
      _paused = true;
      _lastCivilMinute = -1;
      publish();
      _maintenanceApplied = true;
    }
    auto expected = Maintenance::REQUESTED;
    _maintenance.compare_exchange_strong(expected, Maintenance::READY);
    return true;
  }
  if (_maintenanceApplied) {
    _paused = _pausedBeforeMaintenance;
    _maintenanceApplied = false;
    publish();
    return true;
  }
  return false;
}

void ClockManager::tick() {
  if (handle_stop()) return;
  if (handle_maintenance()) return;
  apply_ntp_sync();
  Command command;
  if (_commands && xQueueReceive(_commands, &command, 0) == pdTRUE) {
    bool cancelled;
    for (;;) {
      portENTER_CRITICAL(&_mutex);
      const bool stopPending = _stopPending;
      cancelled = command.epoch != _epoch;
      portEXIT_CRITICAL(&_mutex);
      if (!stopPending) break;
      handle_stop();
    }
    const char* error = cancelled ? "Command cancelled by Stop." : execute(command);
    publish();
    if (command.id != _cycleId) finish(command.id, error);
    return;
  }
  if (handle_stop()) return;
  if (handle_maintenance()) return;
  if (_state == CALIBRATE_HOUR || _state == CALIBRATE_MINUTE) {
    // Bounded batches: queued Stop is processed before the next batch (~85 ms).
    const bool hour = _state == CALIBRATE_HOUR;
    if (_physicalJog) {
      const int axis = hour ? 0 : 1;
      portENTER_CRITICAL(&_mutex);
      const bool held = _jogHeld[axis] && _jogGeneration[axis] == _activeJogGeneration;
      portEXIT_CRITICAL(&_mutex);
      if (!held) {
        _state = RUN;
        if (_logger) (*_logger)("Calibration ended after %llu steps", (unsigned long long)_calibratedSteps);
        return;
      }
    }
    if (!_calibratedSteps) invalidate_reference(hour);
    if (_stepper.move(hour ? 32 : 0, hour ? 0 : 32, true)) _calibratedSteps += 32;
    return;
  }
  if (_state == SET_MINUTES) return;
  if (_state == FAST_FORWARD_HOURS || _state == FAST_FORWARD_MINUTES) {
    const bool hour = _state == FAST_FORWARD_HOURS;
    const bool advanced = advance(hour, !hour);
    if (!advanced || --_remaining == 0) {
      finish_cycle(advanced ? nullptr : "Could not finish the cycle.");
    }
    return;
  }
  if (!_paused && _hourReferenced && _minuteReferenced && _timeSource != NONE) sync_to_current_time();
}

void ClockManager::anchor_references() {
  if (_timeSource == NONE) return;
  const time_t now = time(nullptr);
  struct tm local;
  if (!localtime_r(&now, &local)) return;
  const int64_t minute = civilMinute(local);
  anchor_references(minute, minute - int64_t(now) / 60);
}

void ClockManager::anchor_references(int64_t minute, int64_t zoneOffset) {
  if (_hourReferenced && !_hourCursorKnown) {
    _hourCursor = nearestPosition(minute / 60, _hours.position(), HOUR_CYCLE);
    _hourCursorKnown = true;
  }
  if (_minuteReferenced && !_minuteCursorKnown) {
    _minuteCursor = nearestPosition(minute, _minutes.position(), MINUTE_CYCLE);
    _minuteZoneOffset = zoneOffset;
    _minuteCursorKnown = true;
  }
}

void ClockManager::sync_to_current_time() {
  const time_t now = time(nullptr);
  struct tm local;
  if (!localtime_r(&now, &local)) return;
  const int64_t currentMinute = civilMinute(local);
  const int64_t currentHour = currentMinute / 60;
  const int64_t zoneOffset = currentMinute - int64_t(now) / 60;
  anchor_references(currentMinute, zoneOffset);
  // A whole-hour offset change repeats or skips hour labels, but minute
  // labels still follow elapsed time across the boundary.
  const int64_t zoneShift = zoneOffset - _minuteZoneOffset;
  if (zoneShift % MINUTE_CYCLE == 0) _minuteCursor += zoneShift;
  _minuteZoneOffset = zoneOffset;
  // Complete revolutions cannot change the digit on either axis.
  discardFullCycles(currentHour, _hourCursor, HOUR_CYCLE);
  discardFullCycles(currentMinute, _minuteCursor, MINUTE_CYCLE);
  const int simulatedHour = (local.tm_hour + 11) % 12 + 1;
  const bool hour = SIMULATE_12_HOUR == 1
                  ? currentHour >= _hourCursor && _hours.position() != simulatedHour
                  : currentHour > _hourCursor;
  const bool minute = currentMinute > _minuteCursor;
  const bool hourBoundary = local.tm_min == 0 && _lastCivilMinute == currentMinute - 1;
  _lastCivilMinute = currentMinute;

  const bool moved = (hour || minute) && advance(hour, minute);
  if (moved) {
    if (hour && SIMULATE_12_HOUR == 0) ++_hourCursor;
    if (hour && SIMULATE_12_HOUR == 1 && _hours.position() == simulatedHour) _hourCursor = currentHour;
    if (minute) ++_minuteCursor;
  }
  const time_t hourStart = now - local.tm_sec;
  if (hourBoundary && hourStart != _lastChime) {
    _lastChime = hourStart;
    _casio.beep(local.tm_hour);
  } else if (moved && minute && !hour && _calibration_hint) {
    _casio.beep();
  }
}

void ClockManager::invalidate_reference(bool hour) {
  if (hour) {
    _hourReferenced = false;
    _hourCursorKnown = false;
  } else {
    _minuteReferenced = false;
    _minuteCursorKnown = false;
  }
  _paused = true;
  _lastCivilMinute = -1;
  publish();
}

void ClockManager::start_setting_minutes() {
  _state = SET_MINUTES;
  _settingIndex = 0;
  _currentIndex = _zeroIndex = -1;
  if (_logger) (*_logger)("Minute setup: single click steps, double click marks 00, triple click marks current");
}

const char* ClockManager::step_setting_minutes() {
  if (_settingIndex >= 60) return "Minute setup completed a full cycle. Mark the digit or stop.";
  MotionAxis<60>::Move move{};
  if (!_minutes.plan(move, _minuteReferenced)) return "Could not plan the next minute step.";

  portENTER_CRITICAL(&_mutex);
  _minuteMoving = true;
  ++_minuteGeneration;
  portEXIT_CRITICAL(&_mutex);
  const bool moved = _stepper.move(0, move.steps);
  portENTER_CRITICAL(&_mutex);
  _minuteMoving = false;
  portEXIT_CRITICAL(&_mutex);
  if (!moved) return "Could not move the minute motor.";
  _minutes.commit(move);
  _minuteCursorKnown = false;
  ++_settingIndex;
  return nullptr;
}

const char* ClockManager::mark_minute(bool zero) {
  const bool externalTime = _timeSource == SNTP || _timeSource == WEB;
  if (zero) {
    _zeroIndex = _settingIndex;
    _minutes.rebase_position(0);
    _minuteReferenced = true;
    _minuteCursorKnown = false;
  } else {
    _currentIndex = _settingIndex;
    _currentMarkedAt = millis();
    if (externalTime) {
      const time_t now = time(nullptr);
      struct tm local;
      if (!localtime_r(&now, &local)) return "Could not read current time.";
      _minutes.rebase_position(local.tm_min);
      _minuteReferenced = true;
      _minuteCursorKnown = false;
    }
  }

  if (!externalTime) {
    if (_currentIndex < 0 || _zeroIndex < 0) return nullptr;
    const int elapsed = (millis() - _currentMarkedAt) / 60000;
    const int current = ((_currentIndex - _zeroIndex + elapsed) % 60 + 60) % 60;
    const time_t now = time(nullptr);
    struct tm local;
    if (!localtime_r(&now, &local)) return "Could not read current time.";
    const int hour = _timeSource == NONE ? (_hourReferenced ? _hours.position() : 0) : local.tm_hour;
    local.tm_hour = hour;
    local.tm_min = current;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const time_t updated = mktime(&local);
    if (updated == time_t(-1) || local.tm_hour != hour || local.tm_min != current ||
        !set_wall_time(updated, BUTTONS)) return "Could not set the time from the reference marks.";
  }
  _state = RUN;
  anchor_references();
  return nullptr;
}

void ClockManager::set_displayed_time(int hour, int minute) {
  if (!_hourReferenced || _hours.position() != hour) {
    _hours.rebase_position(hour);
    _hourCursorKnown = false;
  }
  if (!_minuteReferenced || _minutes.position() != minute) {
    _minutes.rebase_position(minute);
    _minuteCursorKnown = false;
  }
  _hourReferenced = _minuteReferenced = true;
  anchor_references();
}

bool ClockManager::set_casio(const char* mode, bool hint) {
  Preferences prefs;
  if (!prefs.begin("time", false)) return false;
  const int32_t setting = (strcmp(mode, "on") == 0 ? 1 : strcmp(mode, "day") == 0 ? 2 : 0) | (hint ? 4 : 0);
  const bool saved = prefs.putInt("casio_config", setting) == sizeof(setting);
  prefs.end();
  if (!saved) return false;
  _casioMode = mode;
  _casio.set_casio(_casioMode);
  _calibration_hint = hint;
  return true;
}

bool ClockManager::start_ntp() {
  portENTER_CRITICAL(&_mutex);
  if (_ntpEnabled) {
    portEXIT_CRITICAL(&_mutex);
    return true;
  }
  _ntpSyncPending = false;
  _ntpEnabled = true;
  portEXIT_CRITICAL(&_mutex);
  if (esp_netif_tcpip_exec([](void*) -> esp_err_t {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    return ESP_OK;
  }, nullptr) == ESP_OK) return true;
  portENTER_CRITICAL(&_mutex);
  _ntpEnabled = false;
  _ntpSyncPending = false;
  portEXIT_CRITICAL(&_mutex);
  if (_logger) (*_logger)("Could not start network time");
  return false;
}

void ClockManager::submit_ntp_sample(const struct timeval& value) {
  const unsigned long receivedAt = millis();
  portENTER_CRITICAL(&_mutex);
  if (_ntpEnabled) {
    _ntpSample = value;
    _ntpSampleAt = receivedAt;
    _ntpSyncPending = true;
  }
  portEXIT_CRITICAL(&_mutex);
}

void ClockManager::apply_ntp_sync() {
  portENTER_CRITICAL(&_mutex);
  const bool pending = _ntpSyncPending;
  auto sample = _ntpSample;
  const auto receivedAt = _ntpSampleAt;
  _ntpSyncPending = false;
  portEXIT_CRITICAL(&_mutex);
  if (!pending) return;
  const unsigned long age = millis() - receivedAt;
  const long microseconds = sample.tv_usec + (age % 1000) * 1000;
  const int64_t seconds = int64_t(sample.tv_sec) + int64_t(age / 1000) + microseconds / 1000000;
  if (seconds > std::numeric_limits<time_t>::max()) return;
  sample.tv_sec = time_t(seconds);
  sample.tv_usec = microseconds % 1000000;
  if (settimeofday(&sample, nullptr) != 0) return;
  sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
  if (_timeSource != SNTP) {
    _timeSource = SNTP;
    _lastCivilMinute = -1;
    anchor_references();
    log_current_time();
    publish();
  }
}

bool ClockManager::set_wall_time(time_t value, time_source_t source) {
  portENTER_CRITICAL(&_mutex);
  const bool wasEnabled = _ntpEnabled;
  _ntpEnabled = false;
  _ntpSyncPending = false;
  portEXIT_CRITICAL(&_mutex);
  if (wasEnabled && esp_netif_tcpip_exec([](void*) -> esp_err_t {
    sntp_stop();
    return ESP_OK;
  }, nullptr) != ESP_OK) {
    portENTER_CRITICAL(&_mutex);
    _ntpEnabled = true;
    portEXIT_CRITICAL(&_mutex);
    return false;
  }
  const struct timeval updated = {value, 0};
  if (settimeofday(&updated, nullptr) != 0) {
    if (wasEnabled) start_ntp();
    return false;
  }
  _timeSource = source;
  _lastCivilMinute = -1;
  anchor_references();
  log_current_time();
  return true;
}

bool ClockManager::set_current_time(int hour, int minute, int second) {
  struct tm timeinfo;
  time_t now = time(0);
  if (!localtime_r(&now, &timeinfo)) return false;

  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = second;
  timeinfo.tm_isdst = -1;

  time_t t = mktime(&timeinfo);
  if (t == time_t(-1) || !localtime_r(&t, &timeinfo) ||
      timeinfo.tm_hour != hour || timeinfo.tm_min != minute || timeinfo.tm_sec != second)
    return false;
  return set_wall_time(t, WEB);
}

const char* ClockManager::set_current_date(int day, int month, int year) {
  const int maxYear = sizeof(time_t) == 4 ? 2038 : 9999;
  if (year < 1970 || year > maxYear || month < 1 || month > 12 || day < 1 || day > 31)
    return "Invalid date.";

  struct tm timeinfo;
  time_t now = time(0);
  if (!localtime_r(&now, &timeinfo)) return "Could not read current time.";

  const int hour = timeinfo.tm_hour;
  const int minute = timeinfo.tm_min;
  const int second = timeinfo.tm_sec;
  timeinfo.tm_mday = day;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_isdst = -1;

  time_t t = mktime(&timeinfo);
  if (t == time_t(-1) || !localtime_r(&t, &timeinfo) ||
      timeinfo.tm_mday != day || timeinfo.tm_mon != month - 1 ||
      timeinfo.tm_year != year - 1900 || timeinfo.tm_hour != hour ||
      timeinfo.tm_min != minute || timeinfo.tm_sec != second) return "Invalid date or local time.";
  if (_timeSource == NONE) {
    const struct timeval date = {t, 0};
    if (settimeofday(&date, nullptr) != 0) return "Could not set the date.";
  } else if (!set_wall_time(t, WEB)) {
    return "Could not set the date.";
  }
  _hourCursorKnown = _minuteCursorKnown = false;
  anchor_references();
  return nullptr;
}

void ClockManager::log_current_time() {
  if (!_logger) return;
  struct tm timeinfo;
  time_t now = time(0);
  if (!localtime_r(&now, &timeinfo)) return;
  char buffer[32];
  strftime(buffer, 31, "%Y-%m-%d %H:%M:%S %z", &timeinfo);
  (*_logger)("Current time is %s", buffer);
}
