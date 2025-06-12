#include <Arduino.h>

#include "ClockManager.h"
#include "settings.h"

void ClockManager::begin() {
  _stepper.begin();

  load_offsets();

  _preferences.begin("time", false);
  String casio = _preferences.getString("casio", "");
  _calibration_hint = _preferences.getBool("cal_hint", false);
  _preferences.end();
  
  _casio.set_casio(casio);
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

void ClockManager::tick() {
  if (_state == CALIBRATE_HOUR) {
    (*_logger)("Calibrate hour");

    _calibrating = true;
    int steps = _stepper.calibrate_hour(_calibrating);

    (*_logger)("Calibration done for %d steps", steps);

    return;
  }

  if (_state == CALIBRATE_MINUTE) {
    (*_logger)("Calibrate minute");

    _calibrating = true;
    int steps = _stepper.calibrate_minute(_calibrating);

    (*_logger)("Calibration done for %d steps", steps);

    return;
  }

  if (_state == DEMO) {
    _stepper.step(true, false);
    delay(500);
    _stepper.step(true, true);
    delay(500);
    _stepper.step(false, true);

    adjust_displayed_hour(2);
    adjust_displayed_minute(2);

    return;
  }

  if (_state == SET_MINUTES) {
    set_minutes();
    _state = RUN;

    return;
  }

  sync_to_current_time();
}

void ClockManager::toggle_demo() {
  _calibrating = false;
  if (_state == DEMO) {
    (*_logger)("End demo");
    _state = RUN;
  } else {
    (*_logger)("Start demo");
    _state = DEMO;
  }
}

void ClockManager::set_displayed_time(int hour, int minute) {
  _displayedHour = hour;
  _displayedMinute = minute;

  (*_logger)("Set displayed time to %02d:%02d\n", _displayedHour, _displayedMinute);
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

void ClockManager::adjust_displayed_hour(int count) {
  _displayedHour = _displayedHour + count;
  if (_displayedHour < 0)
    _displayedHour += 24;
  else if (_displayedHour >= 24)
    _displayedHour -= 24;
}

void ClockManager::adjust_displayed_minute(int count) {
  _displayedMinute = _displayedMinute + count;
  if (_displayedMinute < 0)
    _displayedMinute += 60;
  else if (_displayedMinute >= 60)
    _displayedMinute -= 60;
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
  struct timeval new_time = { .tv_sec = t };
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
  struct timeval new_time = { .tv_sec = t };
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

void ClockManager::sync_to_current_time() {
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

  if (currentHour == _displayedHour && currentMinute == _displayedMinute) {
    return;
  }

  int offsetHour = currentHour - _displayedHour;
  if (offsetHour < 0)
    offsetHour += 24;
  else if (offsetHour >= 24)
    offsetHour -= 24;

  int offsetMinute = currentMinute - _displayedMinute;
  if (offsetMinute < 0)
    offsetMinute += 60;
  else if (offsetMinute >= 60)
    offsetMinute -= 60;

  if (offsetMinute > 50) {
    offsetMinute = 0;
  }

  if(offsetHour == 23 && currentMinute >= 50) {
    offsetHour = 0;
  }

  if(offsetHour == 0 && offsetMinute == 0) {
    return;
  }

  (*_logger)("%02d:%02d -> %02d:%02d", _displayedHour, _displayedMinute, currentHour, currentMinute);

  if (_displayedHour != currentHour) {
    _casio.beep(currentHour);
  }

  while (offsetHour > 0 && offsetMinute > 0) {
    offsetHour--;
    adjust_displayed_hour(1);
    offsetMinute--;
    adjust_displayed_minute(1);

    int hour_offset = _hour_offsets[_displayedHour];
    if (hour_offset != 0) {
      (*_logger)("Applying hour offset: %d for H:%02d", hour_offset, _displayedHour);
    }
    int minute_offset = _minute_offsets[_displayedMinute];
    if (minute_offset != 0) {
      (*_logger)("Applying minute offset: %d for M:%02d", minute_offset, _displayedMinute);
    }
    int steps =_stepper.step(true, true, hour_offset, minute_offset);
    (*_logger)("  Advanced hour in %d steps and minute", steps);
  }

  while (offsetHour > 0) {
    offsetHour--;
    adjust_displayed_hour(1);

    int hour_offset = _hour_offsets[_displayedHour];
    if (hour_offset != 0) {
      (*_logger)("Applying hour offset: %d for H:%02d", hour_offset, _displayedHour);
    }
    int steps = _stepper.step(true, false, hour_offset);
    (*_logger)("  Advanced hour in %d steps", steps);
  }

  while (offsetMinute > 0) {
    offsetMinute--;
    adjust_displayed_minute(1);

    if (_calibration_hint) {
      _casio.beep();
    }

    int minute_offset = _minute_offsets[_displayedMinute];
    if (minute_offset != 0) {
      (*_logger)("Applying minute offset: %d for M:%02d", minute_offset, _displayedMinute);
    }
    int steps = _stepper.step(false, true, 0, minute_offset);
    (*_logger)("  Advanced minute in %d steps", steps);
  }
}

void ClockManager::set_minutes() {
  (*_logger)("Setting minutes");

  _flagCurrentMinute = false;
  _flagZeroMinute = false;

  int currentIndex = -1;
  int zeroIndex = -1;
  bool timeKnown = (time_source != NONE && time_source != BUTTONS);

  unsigned long startMillis = millis();
  int currentMinuteAtStart = 0;
  if (timeKnown) {
    struct tm timeinfo;
    time_t now = time(0);
    localtime_r(&now, &timeinfo);      
    currentMinuteAtStart = timeinfo.tm_min;
  }

  for (int index = 1; index <= 60; index++) {
    _stepper.step(false, true);
    adjust_displayed_minute(1);

    (*_logger)("  Step %02d displayed=%02d zero=%02d current=%02d", index, _displayedMinute, zeroIndex, currentIndex);

    if (_flagZeroMinute) {
      _flagZeroMinute = false;
      zeroIndex = index;
      _displayedMinute = 0;
      if (timeKnown || currentIndex >= 0)
        break;
    }
    if (_flagCurrentMinute) {
      _flagCurrentMinute = false;
      currentIndex = index;
      if (zeroIndex < 0)
        _displayedMinute = currentMinuteAtStart;

      if (timeKnown || zeroIndex >= 0)
        break;
    }
  }

  (*_logger)("          displayed=%02d zero=%02d current=%02d", _displayedMinute, zeroIndex, currentIndex);

  if (!timeKnown) {
    int elapsedMinutes = (millis() - startMillis) / (60 * 1000);
    int currentMinute = currentIndex - zeroIndex + elapsedMinutes;

    if (currentMinute < 0)
      currentMinute += 60;

    time_source = BUTTONS;
    set_current_time(0, currentMinute, 0);
  }
}

void ClockManager::set_casio(String casio, bool calibration_hint) {
  
  _preferences.begin("time", false);
  _preferences.putString("casio", casio);
  _preferences.putBool("cal_hint", calibration_hint);
  _preferences.end();

  _casio.set_casio(casio);
  _calibration_hint = calibration_hint;

  (*_logger)("Set CASIO to %s, hint: %d", casio, calibration_hint);
}

void ClockManager::load_offsets() {
  _preferences.begin("offsets", true);

  for (int i = 0; i < 24; i++) {
    char key[5];
    sprintf(key, "h%d", i);
    _hour_offsets[i] = _preferences.getInt(key, 0);
  }

  for (int i = 0; i < 60; i++) {
    char key[5];
    sprintf(key, "m%d", i);
    _minute_offsets[i] = _preferences.getInt(key, 0);
  }

  _preferences.end();
  (*_logger)("Loaded offsets");
}

void ClockManager::save_offsets(int *hour_offsets, int *minute_offsets) {
  _preferences.begin("offsets", false);

  for (int i = 0; i < 24; i++) {
    char key[5];
    sprintf(key, "h%d", i);
    if (hour_offsets[i] == 0) {
      if (_preferences.isKey(key)) {
        _preferences.remove(key);
      }
    } else {
      _preferences.putInt(key, hour_offsets[i]);
    }
    _hour_offsets[i] = hour_offsets[i];
  }

  for (int i = 0; i < 60; i++) {
    char key[5];
    sprintf(key, "m%d", i);
    if (minute_offsets[i] == 0) {
      if (_preferences.isKey(key)) {
        _preferences.remove(key);
      }
    } else {
      _preferences.putInt(key, minute_offsets[i]);
    }
    _minute_offsets[i] = minute_offsets[i];
  }

  _preferences.end();
  (*_logger)("Saved offsets");
}
