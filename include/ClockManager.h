#ifndef CLOCKMANAGER_H
#define CLOCKMANAGER_H

#include "StepperDriver.h"
#include "MotionAxis.h"
#include "CASIO.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

typedef void (*logger_cb_t)(const char* format, ...) __attribute__((format(printf, 1, 2)));
enum time_source_t { NONE, SNTP, WEB, BUTTONS };

class ClockManager {
public:
  enum class Action {
    CALIBRATE_HOUR, CALIBRATE_MINUTE, STOP, RESUME, SET_MINUTES,
    FAST_HOURS, FAST_MINUTES, ADJUST_HOUR, ADJUST_MINUTE,
    DISPLAYED_TIME, CURRENT_TIME, CURRENT_DATE, CASIO_MODE,
    SETTINGS, LIVE_HOUR, LIVE_MINUTE
  };
  struct Command {
    uint32_t id = 0;
    uint32_t epoch = 0;
    Action action = Action::STOP;
    int a = 0, b = 0, c = 0;
    MotionSettings settings;
    char text[8] = {};
  };
  struct Result {
    uint32_t id = 0;
    bool done = false;
    bool success = false;
    char message[96] = {};
  };
  struct Snapshot {
    MotionSettings settings;
    int hour = 0, minute = 0;
    bool paused = false;
  };

  void begin();
  void tick();
  // Thread-safe producers. A nonzero id means accepted, not yet executed.
  uint32_t submit(Command command);
  uint32_t request(Action action, int a = 0, int b = 0, int c = 0);
  bool result(uint32_t id, Result& out);
  Snapshot snapshot();

  uint32_t request_calibrate_hour() { return request(Action::CALIBRATE_HOUR); }
  uint32_t request_calibrate_minute() { return request(Action::CALIBRATE_MINUTE); }
  uint32_t request_end_calibrate() { return request(Action::STOP); }
  uint32_t request_set_minutes() { return request(Action::SET_MINUTES); }
  uint32_t increment_hour() { return request(Action::ADJUST_HOUR, -1); }
  uint32_t decrement_hour() { return request(Action::ADJUST_HOUR, 1); }
  uint32_t increment_minute() { return request(Action::ADJUST_MINUTE, -1); }
  uint32_t decrement_minute() { return request(Action::ADJUST_MINUTE, 1); }

  // Initialization only, before the loop starts consuming commands.
  time_source_t time_source = NONE;
  void set_logger(logger_cb_t logger) { _logger = logger; }
  void start_ntp();
  void set_current_time(int hour, int minute, int second);
  void set_displayed_time_to_current();

private:
  enum State { RUN, PAUSED, CALIBRATE_HOUR, CALIBRATE_MINUTE, SET_MINUTES,
               FAST_FORWARD_HOURS, FAST_FORWARD_MINUTES };
  State _state = RUN;
  MotionAxis<24> _hours{787200};
  MotionAxis<60> _minutes{1883200};
  StepperDriver _stepper;
  CASIO _casio;
  bool _calibration_hint = false;
  logger_cb_t _logger = nullptr;
  int _remaining = 0;
  uint64_t _calibratedSteps = 0;
  int _settingIndex = 0, _currentIndex = -1, _zeroIndex = -1;
  unsigned long _currentMarkedAt = 0;
  bool _timeKnown = false;

  QueueHandle_t _commands = nullptr;
  portMUX_TYPE _mutex = portMUX_INITIALIZER_UNLOCKED;
  static constexpr size_t RESULT_COUNT = 32;
  Result _results[RESULT_COUNT];
  Snapshot _snapshot;
  uint32_t _nextId = 1;
  uint32_t _epoch = 0;
  bool _stopPending = false;
  Result _stopResult;

  const char* execute(const Command& command);
  bool handle_stop();
  void finish(uint32_t id, const char* error);
  void publish();
  MotionSettings settings() const;
  bool configure(const MotionSettings& settings);
  void load_settings();
  bool persist(const MotionSettings& settings);
  bool advance(bool hour, bool minute);
  const char* live_calibrate(bool hour, int direction);
  void sync_to_current_time();
  void set_displayed_time(int hour, int minute);
  void set_current_date(int day, int month, int year);
  void log_current_time();
  void start_setting_minutes();
  void mark_minute(bool zero);
  void tick_setting_minutes();
  void set_casio(const char* mode, bool hint);
};
#endif
