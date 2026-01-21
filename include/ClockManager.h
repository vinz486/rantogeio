#ifndef CLOCKMANAGER_H
#define CLOCKMANAGER_H

#include "StepperDriver.h"
#include <Preferences.h>
#include "CASIO.h"

typedef void (*logger_cb_t)(const char* format, ...);

enum time_source_t {
  NONE,
  SNTP,
  WEB,
  BUTTONS
};

class ClockManager {
public:
  time_source_t time_source = NONE;

  void begin();
  void tick();
  void start_ntp();
  void set_displayed_time(int hour, int minute);
  void set_current_time(int hour, int minute, int second);
  void set_current_date(int day, int month, int year);
  void set_displayed_time_to_current();
  void toggle_demo();
  void set_casio(String casio, bool calibration_hint);
  void save_offsets(int *hour_offsets, int *minute_offsets);

  void set_logger(logger_cb_t logger) {
    _logger = logger;
  }

  void request_calibrate_hour() {
    _state = CALIBRATE_HOUR;
  }

  void request_calibrate_minute() {
    _state = CALIBRATE_MINUTE;
  }
  
  void request_end_calibrate() {
    _calibrating = false;
    _state = RUN;
  }

  void request_set_minutes() {
    _state = SET_MINUTES;
  }

  void live_calibrate_step(bool is_hour, int direction);
  
  void request_fast_forward_hours() {
    _ff_counter = 0;
    _state = FAST_FORWARD_HOURS;
  }
  
  void request_fast_forward_minutes() {
    _ff_counter = 0;
    _state = FAST_FORWARD_MINUTES;
  }

  void increment_hour() {
    adjust_displayed_hour(-1);
  }

  void decrement_hour() {
    adjust_displayed_hour(1);
  }

  void increment_minute() {
    if (_state == SET_MINUTES)
      _flagCurrentMinute = true;
    else
      adjust_displayed_minute(-1);
  }

  void decrement_minute() {
    if (_state == SET_MINUTES)
      _flagZeroMinute = true;
    else
      adjust_displayed_minute(1);
  }

private:
  enum state_t { RUN,
                 CALIBRATE_HOUR,
                 CALIBRATE_MINUTE,
                 DEMO,
                 SET_MINUTES,
                 FAST_FORWARD_HOURS,
                 FAST_FORWARD_MINUTES };

  bool _calibrating = false;
  int _displayedHour = 0;
  int _displayedMinute = 0;
  state_t _state = RUN;
  bool _flagCurrentMinute;
  bool _flagZeroMinute;
  int _hour_offsets[24] = {0};  // Initialize to zero to avoid garbage values
  int _minute_offsets[60] = {0};  // Initialize to zero to avoid garbage values
  int _ff_counter = 0;

  StepperDriver _stepper;
  Preferences _preferences;
  CASIO _casio;
  bool _calibration_hint;
  logger_cb_t _logger;

  void sync_to_current_time();
  void log_current_time();
  void set_minutes();
  void load_offsets();
  void adjust_displayed_hour(int count);
  void adjust_displayed_minute(int count);
};

#endif
