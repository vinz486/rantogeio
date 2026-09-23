#include "ClockManager.h"
#include "settings.h"
#include <sys/time.h>
#include <cstring>

void ClockManager::begin() {
  _commands = xQueueCreate(8, sizeof(Command));
  _stepper.begin();
  load_settings();
  Preferences prefs;
  prefs.begin("time", true);
  _calibration_hint = prefs.getBool("cal_hint", false);
  _casio.set_casio(prefs.getString("casio", "off"));
  prefs.end();
  publish();
}

uint32_t ClockManager::submit(Command command) {
  portENTER_CRITICAL(&_mutex);
  // Stop has its own mailbox/result: it never needs space in the normal queue.
  if (command.action == Action::STOP && _stopPending) {
    const uint32_t id = _stopResult.id;
    portEXIT_CRITICAL(&_mutex);
    return id;
  }
  const uint32_t id = _nextId++;
  if (_nextId > INT32_MAX) _nextId = 1;
  if (command.action == Action::STOP) {
    ++_epoch; // Invalidate commands submitted before this Stop, including races.
    _stopResult = Result{};
    _stopResult.id = id;
    _stopPending = true;
    portEXIT_CRITICAL(&_mutex);
    return id;
  }
  Result& slot = _results[id % RESULT_COUNT];
  if (!_commands || (slot.id && !slot.done)) {
    portEXIT_CRITICAL(&_mutex);
    return 0;
  }
  slot = Result{};
  slot.id = id;
  command.epoch = _epoch;
  portEXIT_CRITICAL(&_mutex);
  command.id = id;
  if (xQueueSend(_commands, &command, 0) != pdTRUE) {
    finish(id, "Command queue full. Please retry.");
    return 0;
  }
  return id;
}

uint32_t ClockManager::request(Action action, int a, int b, int c) {
  Command command;
  command.action = action;
  command.a = a;
  command.b = b;
  command.c = c;
  const uint32_t id = submit(command);
  if (!id && _logger) (*_logger)("Command queue full; command rejected");
  return id;
}

bool ClockManager::result(uint32_t id, Result& out) {
  portENTER_CRITICAL(&_mutex);
  out = id == _stopResult.id ? _stopResult : _results[id % RESULT_COUNT];
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
  Result& slot = id == _stopResult.id ? _stopResult : _results[id % RESULT_COUNT];
  if (id == _stopResult.id) _stopPending = false;
  slot.done = true;
  slot.success = !error;
  snprintf(slot.message, sizeof(slot.message), "%s", error ? error : "OK");
  portEXIT_CRITICAL(&_mutex);
  if (error && _logger) (*_logger)("Command %u failed: %s", id, error);
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
  copy.paused = _state == PAUSED;
  portENTER_CRITICAL(&_mutex);
  _snapshot = copy;
  portEXIT_CRITICAL(&_mutex);
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
      if (_state == CALIBRATE_HOUR || _state == CALIBRATE_MINUTE)
        (*_logger)("Calibration done for %llu steps", (unsigned long long)_calibratedSteps);
      _state = PAUSED;
      return nullptr;
    case Action::RESUME:
      _state = RUN;
      return nullptr;
    case Action::CALIBRATE_HOUR:
    case Action::CALIBRATE_MINUTE:
      _state = command.action == Action::CALIBRATE_HOUR ? CALIBRATE_HOUR : CALIBRATE_MINUTE;
      _calibratedSteps = 0;
      return nullptr;
    case Action::FAST_HOURS:
    case Action::FAST_MINUTES:
      _state = command.action == Action::FAST_HOURS ? FAST_FORWARD_HOURS : FAST_FORWARD_MINUTES;
      _remaining = _state == FAST_FORWARD_HOURS ? 24 : 60;
      return nullptr;
    case Action::SET_MINUTES:
      start_setting_minutes();
      return nullptr;
    case Action::ADJUST_HOUR:
      _hours.adjust_position(command.a);
      return nullptr;
    case Action::ADJUST_MINUTE:
      if (_state == SET_MINUTES) mark_minute(command.a > 0);
      else _minutes.adjust_position(command.a);
      return nullptr;
    case Action::DISPLAYED_TIME:
      if (command.a < 0 || command.a >= 24 || command.b < 0 || command.b >= 60)
        return "Displayed time must be within 00:00 and 23:59.";
      set_displayed_time(command.a, command.b);
      return nullptr;
    case Action::CURRENT_TIME:
      if (command.a < 0 || command.a >= 24 || command.b < 0 || command.b >= 60)
        return "Time must be within 00:00 and 23:59.";
      set_current_time(command.a, command.b, 0);
      time_source = WEB;
      return nullptr;
    case Action::CURRENT_DATE:
      set_current_date(command.a, command.b, command.c);
      return nullptr;
    case Action::CASIO_MODE:
      set_casio(command.text, command.a != 0);
      return nullptr;
    case Action::SETTINGS: {
      if (_state != RUN && _state != PAUSED) return "Stop calibration or fast forward before saving settings.";
      auto hours = _hours;
      auto minutes = _minutes;
      const MotionSettings& config = command.settings;
      int hourSteps = 0, minuteSteps = 0;
      if (config.version != 2 || !hours.update(config.hour_target, config.hours, hourSteps) ||
          !minutes.update(config.minute_target, config.minutes, minuteSteps))
        return "Invalid steps: every digit must require a positive, representable movement.";
      if (!persist(config)) return "Could not save motion settings. Please retry.";
      if (!_stepper.move(hourSteps, minuteSteps, true)) return "Could not apply digit corrections.";
      _hours = hours;
      _minutes = minutes;
      return nullptr;
    }
    case Action::LIVE_HOUR:
    case Action::LIVE_MINUTE:
      if (_state != RUN && _state != PAUSED) return "Stop calibration or fast forward before live adjustment.";
      return live_calibrate(command.action == Action::LIVE_HOUR, command.a);
  }
  return "Unknown command.";
}

bool ClockManager::advance(bool hour, bool minute) {
  MotionAxis<24>::Move h{};
  MotionAxis<60>::Move m{};
  if ((hour && !_hours.plan(h)) || (minute && !_minutes.plan(m))) {
    (*_logger)("Invalid movement; position and fractional remainder unchanged");
    return false;
  }
  if (!_stepper.move(hour ? h.steps : 0, minute ? m.steps : 0)) return false;
  if (hour) _hours.commit(h);
  if (minute) _minutes.commit(m);
  (*_logger)("Advanced to %02d:%02d: hour=%d minute=%d microsteps",
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
  MotionSettings config = settings();
  memcpy(config.hours, hours.offsets(), sizeof(config.hours));
  memcpy(config.minutes, minutes.offsets(), sizeof(config.minutes));
  if (!persist(config)) return "Could not save calibration. No adjustment applied.";
  if (!_stepper.move(hour ? forwardSteps : 0, hour ? 0 : forwardSteps, true))
    return "Could not execute calibration.";
  _hours = hours;
  _minutes = minutes;
  return nullptr;
}

bool ClockManager::handle_stop() {
  portENTER_CRITICAL(&_mutex);
  const uint32_t id = _stopPending ? _stopResult.id : 0;
  portEXIT_CRITICAL(&_mutex);
  if (!id) return false;
  Command stop;
  stop.action = Action::STOP;
  execute(stop);
  publish();
  finish(id, nullptr);
  return true;
}

void ClockManager::tick() {
  if (handle_stop()) return;
  Command command;
  if (_commands && xQueueReceive(_commands, &command, 0) == pdTRUE) {
    portENTER_CRITICAL(&_mutex);
    const bool cancelled = command.epoch != _epoch;
    portEXIT_CRITICAL(&_mutex);
    const char* error = cancelled ? "Command cancelled by Stop." : execute(command);
    publish();
    finish(command.id, error);
    return;
  }
  if (_state == CALIBRATE_HOUR || _state == CALIBRATE_MINUTE) {
    // Bounded batches: queued Stop is processed before the next batch (~85 ms).
    const bool hour = _state == CALIBRATE_HOUR;
    if (_stepper.move(hour ? 32 : 0, hour ? 0 : 32, true)) _calibratedSteps += 32;
    return;
  }
  if (_state == SET_MINUTES) {
    tick_setting_minutes();
    return;
  }
  if (_state == FAST_FORWARD_HOURS || _state == FAST_FORWARD_MINUTES) {
    const bool hour = _state == FAST_FORWARD_HOURS;
    if (!advance(hour, !hour) || --_remaining == 0) _state = RUN;
    return;
  }
  if (_state != PAUSED) sync_to_current_time();
}

void ClockManager::sync_to_current_time() {
  struct tm timeinfo;
  time_t now = time(0);
  localtime_r(&now, &timeinfo);
  int currentHour = timeinfo.tm_hour;
#if SIMULATE_12_HOUR == 1
  currentHour = currentHour % 12 ? currentHour % 12 : 12;
#endif
  const int currentMinute = timeinfo.tm_min;
  int hours = (currentHour - _hours.position() + 24) % 24;
  int minutes = (currentMinute - _minutes.position() + 60) % 60;
  // Preserve the existing wait-for-time behavior after manual forward changes.
  if (minutes > 50) minutes = 0;
  if (hours == 23 && currentMinute >= 50) hours = 0;
  if (!hours && !minutes) return;
  if (hours) _casio.beep(currentHour);
  // One transition per tick: process new commands and re-read time between moves.
  if (advance(hours > 0, minutes > 0) && !hours && _calibration_hint) _casio.beep();
}

void ClockManager::start_setting_minutes() {
  _state = SET_MINUTES;
  _settingIndex = 0;
  _currentIndex = _zeroIndex = -1;
  _timeKnown = time_source != NONE && time_source != BUTTONS;
}

void ClockManager::mark_minute(bool zero) {
  if (zero) {
    _zeroIndex = _settingIndex;
    _minutes.set_position(0);
  } else {
    _currentIndex = _settingIndex;
    _currentMarkedAt = millis();
    if (_timeKnown) {
      struct tm timeinfo;
      time_t now = time(0);
      localtime_r(&now, &timeinfo);
      _minutes.set_position(timeinfo.tm_min);
    }
  }
  if (_timeKnown) _state = RUN;
  else if (_currentIndex >= 0 && _zeroIndex >= 0) {
    const int elapsed = (millis() - _currentMarkedAt) / 60000;
    const int current = ((_currentIndex - _zeroIndex + elapsed) % 60 + 60) % 60;
    time_source = BUTTONS;
    set_current_time(0, current, 0);
    _state = RUN;
  }
}

void ClockManager::tick_setting_minutes() {
  if (_settingIndex >= 60 || !advance(false, true)) {
    _state = RUN;
    (*_logger)("Minute setting ended without both reference marks");
    return;
  }
  ++_settingIndex;
}

void ClockManager::set_displayed_time(int hour, int minute) {
  if (hour < 0 || hour >= 24 || minute < 0 || minute >= 60) return;
  _hours.set_position(hour);
  _minutes.set_position(minute);
  publish();
}

void ClockManager::set_casio(const char* mode, bool hint) {
  Preferences prefs;
  prefs.begin("time", false);
  prefs.putString("casio", mode);
  prefs.putBool("cal_hint", hint);
  prefs.end();
  _casio.set_casio(mode);
  _calibration_hint = hint;
}

void ClockManager::start_ntp() {
  configTime(0, 0, "pool.ntp.org");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    (*_logger)("Failed to obtain time");
    return;
  }

  time_source = SNTP;

  log_current_time();
  set_displayed_time_to_current();
  _state = RUN;
}

void ClockManager::set_displayed_time_to_current() {
  struct tm timeinfo;
  time_t now = time(0);
  localtime_r(&now, &timeinfo);
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;

#if SIMULATE_12_HOUR == 1
  if (currentHour == 0) {
    currentHour = 12;
  } else if (currentHour > 12) {
    currentHour -= 12;
  }
#endif

  set_displayed_time(currentHour, currentMinute);
}

void ClockManager::set_current_time(int hour, int minute, int second) {
  struct tm timeinfo;
  time_t now = time(0);
  localtime_r(&now, &timeinfo);

  timeinfo.tm_hour = hour;
  timeinfo.tm_min = minute;
  timeinfo.tm_sec = second;
  timeinfo.tm_isdst = -1;

  time_t t = mktime(&timeinfo);
  struct timeval new_time = {t, 0};
  settimeofday(&new_time, NULL);

  log_current_time();
}

void ClockManager::set_current_date(int day, int month, int year) {
  struct tm timeinfo;
  time_t now = time(0);
  localtime_r(&now, &timeinfo);

  timeinfo.tm_mday = day;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_isdst = -1;

  time_t t = mktime(&timeinfo);
  struct timeval new_time = {t, 0};
  settimeofday(&new_time, NULL);

  log_current_time();
}

void ClockManager::log_current_time() {
  struct tm timeinfo;
  time_t now = time(0);
  localtime_r(&now, &timeinfo);
  char buffer[32];
  strftime(buffer, 31, "%Y-%m-%d %H:%M:%S %z", &timeinfo);
  (*_logger)("Current time is %s", buffer);
}
