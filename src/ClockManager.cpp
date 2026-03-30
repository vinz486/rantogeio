#include <Arduino.h>

#include "ClockManager.h"
#include "settings.h"

int ClockManager::get_fractional_hour_extra(int hour_from) {
  // Solution 1: quantized deterministic distribution over 24 hours
  // extra = floor((h+1)*frac/100) - floor(h*frac/100)
  int frac = HOUR_TARGET_X100 % 100;
  int current = (hour_from * frac) / 100;
  int next = ((hour_from + 1) * frac) / 100;
  return next - current;
}

int ClockManager::get_effective_hour_offset(int hour_from) {
  return _hour_offsets[hour_from] + get_fractional_hour_extra(hour_from);
}

int ClockManager::get_fractional_minute_extra(int minute_from) {
  // Deterministic distribution over 60 minutes using 2 decimals in fixed-point x100
  // extra = floor((m+1)*frac/100) - floor(m*frac/100)
  int frac = MINUTE_TARGET_X100 % 100;
  int current = (minute_from * frac) / 100;
  int next = ((minute_from + 1) * frac) / 100;
  return next - current;
}

int ClockManager::get_effective_minute_offset(int minute_from) {
  return _minute_offsets[minute_from] + get_fractional_minute_extra(minute_from);
}

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


  if (_state == SET_MINUTES) {
    set_minutes();
    _state = RUN;

    return;
  }

  if (_state == FAST_FORWARD_HOURS) {
    if (_ff_counter < 24) {
      // Offset index uses arrival semantics: Hxx applies to transition (xx-1)->xx
      int hour_to = (_displayedHour + 1) % 24;
      int hour_offset = get_effective_hour_offset(hour_to);
      
      adjust_displayed_hour(1);
      _stepper.step(true, false, hour_offset);
      
      (*_logger)("Fast forward H:%02d->%02d using H%02d (offset: %+d) [%d/24]",
                 (_displayedHour + 23) % 24,
                 _displayedHour,
                 hour_to,
                 hour_offset,
                 _ff_counter + 1);
      _ff_counter++;
    } else {
      (*_logger)("Fast forward: Hour cycle completed");
      _ff_counter = 0;
      _state = RUN;
    }
    return;
  }

  if (_state == FAST_FORWARD_MINUTES) {
    if (_ff_counter < 60) {
      // Offset index uses arrival semantics: Mxx applies to transition (xx-1)->xx
      int minute_to = (_displayedMinute + 1) % 60;
      int minute_offset = get_effective_minute_offset(minute_to);
      
      adjust_displayed_minute(1);
      _stepper.step(false, true, 0, minute_offset);
      
      (*_logger)("Fast forward M:%02d->%02d using M%02d (offset: %+d) [%d/60]",
                 (_displayedMinute + 59) % 60,
                 _displayedMinute,
                 minute_to,
                 minute_offset,
                 _ff_counter + 1);
      _ff_counter++;
    } else {
      (*_logger)("Fast forward: Minute cycle completed");
      _ff_counter = 0;
      _state = RUN;
    }
    return;
  }

  sync_to_current_time();
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
    offsetMinute--;

    // Arrival semantics: offset index is the destination bucket
    int hour_from = _displayedHour;
    int minute_from = _displayedMinute;
    int hour_to = (hour_from + 1) % 24;
    int minute_to = (minute_from + 1) % 60;
    int hour_offset = get_effective_hour_offset(hour_to);
    int minute_offset = get_effective_minute_offset(minute_to);
    
    if (hour_offset != 0) {
      (*_logger)("Applying hour offset H%02d for %02d->%02d: %+d", hour_to, hour_from, hour_to, hour_offset);
    }
    if (minute_offset != 0) {
      (*_logger)("Applying minute offset M%02d for %02d->%02d: %+d", minute_to, minute_from, minute_to, minute_offset);
    }
    
    adjust_displayed_hour(1);
    adjust_displayed_minute(1);
    
    int steps =_stepper.step(true, true, hour_offset, minute_offset);
    (*_logger)("  Advanced hour in %d steps and minute", steps);
  }

  while (offsetHour > 0) {
    offsetHour--;

    int hour_from = _displayedHour;
    int hour_to = (hour_from + 1) % 24;
    int hour_offset = get_effective_hour_offset(hour_to);
    if (hour_offset != 0) {
      (*_logger)("Applying hour offset H%02d for %02d->%02d: %+d", hour_to, hour_from, hour_to, hour_offset);
    }
    
    adjust_displayed_hour(1);
    
    int steps = _stepper.step(true, false, hour_offset);
    (*_logger)("  Advanced hour in %d steps", steps);
  }

  while (offsetMinute > 0) {
    offsetMinute--;

    int minute_from = _displayedMinute;
    int minute_to = (minute_from + 1) % 60;
    int minute_offset = get_effective_minute_offset(minute_to);
    if (minute_offset != 0) {
      (*_logger)("Applying minute offset M%02d for %02d->%02d: %+d", minute_to, minute_from, minute_to, minute_offset);
    }
    
    adjust_displayed_minute(1);
    
    int steps = _stepper.step(false, true, 0, minute_offset);
    (*_logger)("  Advanced minute in %d steps", steps);

    if (_calibration_hint) {
      _casio.beep();
    }
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

void ClockManager::live_calibrate_step(bool is_hour, int direction) {
  const int STEP_DELTA = 100; // Microsteps delta per calibration step (testing higher value for static inertia)
  
  if (is_hour) {
    int current_hour = _displayedHour;
    int next_hour = (current_hour + 1) % 24;
    
    if (direction > 0) {
      // Move forward physically: execute steps immediately
      (*_logger)("Live calibrate: Hour forward at H:%02d", current_hour);
      _stepper.step_hour(STEP_DELTA);  // Move ONLY STEP_DELTA steps
      
      // Arrival semantics: Hcurrent applies to transition (prev->current)
      _hour_offsets[current_hour] += STEP_DELTA;
      
      // Compensate on next arrival bucket
      _hour_offsets[next_hour] -= STEP_DELTA;
      
      (*_logger)("  H%02d offset: %+d, H%02d offset: %+d", 
                 current_hour, _hour_offsets[current_hour], 
                 next_hour, _hour_offsets[next_hour]);
    } else if (direction < 0) {
      // Move backward (logically): reduce offset for next cycle
      (*_logger)("Live calibrate: Hour backward at H:%02d (takes effect next cycle)", current_hour);
      
      _hour_offsets[current_hour] -= STEP_DELTA;
      
      _hour_offsets[next_hour] += STEP_DELTA;
      
      (*_logger)("  H%02d offset: %+d, H%02d offset: %+d", 
                 current_hour, _hour_offsets[current_hour], 
                 next_hour, _hour_offsets[next_hour]);
    }
    
  } else {
    // Minutes
    int current_minute = _displayedMinute;
    int next_minute = (current_minute + 1) % 60;
    
    if (direction > 0) {
      // Move forward physically: execute steps immediately
      (*_logger)("Live calibrate: Minute forward at M:%02d", current_minute);
      _stepper.step_minute(STEP_DELTA);  // Move ONLY STEP_DELTA steps
      
      _minute_offsets[current_minute] += STEP_DELTA;
      
      _minute_offsets[next_minute] -= STEP_DELTA;
      
      (*_logger)("  M%02d offset: %+d, M%02d offset: %+d", 
                 current_minute, _minute_offsets[current_minute], 
                 next_minute, _minute_offsets[next_minute]);
    } else if (direction < 0) {
      // Move backward (logically): reduce offset for next cycle
      (*_logger)("Live calibrate: Minute backward at M:%02d (takes effect next cycle)", current_minute);
      
      _minute_offsets[current_minute] -= STEP_DELTA;
      
      _minute_offsets[next_minute] += STEP_DELTA;
      
      (*_logger)("  M%02d offset: %+d, M%02d offset: %+d", 
                 current_minute, _minute_offsets[current_minute], 
                 next_minute, _minute_offsets[next_minute]);
    }
  }
  
  // Save offsets immediately
  save_offsets(_hour_offsets, _minute_offsets);
}
