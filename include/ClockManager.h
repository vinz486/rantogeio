#ifndef CLOCKMANAGER_H
#define CLOCKMANAGER_H

#include "StepperDriver.h"
#include "MotionAxis.h"
#include "CASIO.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <sys/time.h>

typedef void (*logger_cb_t)(const char* format, ...) __attribute__((format(printf, 1, 2)));
enum time_source_t { NONE, SNTP, WEB, BUTTONS };

class ClockManager {
public:
  enum class Action {
    CALIBRATE_HOUR, CALIBRATE_MINUTE, STOP, RESUME, SET_MINUTES,
    FAST_HOURS, FAST_MINUTES, ADJUST_HOUR, ADJUST_MINUTE,
    DISPLAYED_TIME, CURRENT_TIME, CURRENT_DATE, CASIO_MODE,
    SETTINGS, LIVE_HOUR, LIVE_MINUTE, TIME_ZONE, USE_NTP, REFERENCE_HOUR
  };
  struct Command {
    uint32_t id = 0;
    uint32_t epoch = 0;
    uint32_t minuteGeneration = 0;
    bool capturedMinuteGeneration = false;
    Action action = Action::STOP;
    int a = 0, b = 0, c = 0;
    MotionSettings settings;
    int32_t expectedMotionRevision = 0;
    char text[64] = {};
  };
  struct Result {
    uint32_t id = 0;
    bool done = false;
    bool success = false;
    int32_t motionRevision = 0;
    char message[96] = {};
  };
  struct Snapshot {
    MotionSettings settings;
    int hour = 0, minute = 0;
    bool paused = false;
    bool hourReferenced = false;
    bool minuteReferenced = false;
    time_source_t timeSource = NONE;
    uint32_t stateRevision = 0;
    bool calibrationHint = false;
    char casioMode[8] = "off";
    int32_t motionRevision = 1;
  };

  void begin(int32_t motionRevision = 1);
  void tick();
  // Thread-safe producers. A nonzero id means accepted, not yet executed.
  uint32_t submit(Command command);
  uint32_t request(Action action, int a = 0, int b = 0, int c = 0);
  bool result(uint32_t id, Result& out);
  Snapshot snapshot();
  void request_maintenance();
  void end_maintenance();
  bool maintenance_ready() const { return _maintenance.load() == Maintenance::READY; }

  uint32_t request_calibrate_hour() { return request(Action::CALIBRATE_HOUR); }
  uint32_t request_calibrate_minute() { return request(Action::CALIBRATE_MINUTE); }
  uint32_t request_end_calibrate() { return request(Action::STOP); }
  uint32_t request_set_minutes() { return request(Action::SET_MINUTES); }
  void press_jog(bool hour);
  void release_jog(bool hour);
  bool capture_minute_input(uint32_t& generation);
  uint32_t request_minute_input(Action action, int direction, uint32_t generation);
  uint32_t increment_hour() { return request(Action::ADJUST_HOUR, -1); }
  uint32_t decrement_hour() { return request(Action::ADJUST_HOUR, 1); }
  uint32_t increment_minute() { return request(Action::ADJUST_MINUTE, -1); }
  uint32_t decrement_minute() { return request(Action::ADJUST_MINUTE, 1); }

  void set_logger(logger_cb_t logger) { _logger = logger; }
  void set_time_zone_manager(class TimeZoneManager& manager) { _timeZone = &manager; }
  bool start_ntp();
  void submit_ntp_sample(const struct timeval& value);

private:
  enum State { RUN, CALIBRATE_HOUR, CALIBRATE_MINUTE, SET_MINUTES,
               FAST_FORWARD_HOURS, FAST_FORWARD_MINUTES };
  State _state = RUN;
  bool _paused = true;
  bool _hourReferenced = false;
  bool _minuteReferenced = false;
  bool _hourCursorKnown = false;
  bool _minuteCursorKnown = false;
  int64_t _hourCursor = 0;
  int64_t _minuteCursor = 0;
  int64_t _minuteZoneOffset = 0;
  int64_t _lastCivilMinute = -1;
  time_source_t _timeSource = NONE;
  TimeZoneManager* _timeZone = nullptr;
  bool _ntpSyncPending = false;
  struct timeval _ntpSample{};
  unsigned long _ntpSampleAt = 0;
  enum class Maintenance { OFF, REQUESTED, READY };
  std::atomic<Maintenance> _maintenance{Maintenance::OFF};
  bool _maintenanceApplied = false;
  bool _pausedBeforeMaintenance = true;
  bool _ntpEnabled = false;
  MotionAxis<24> _hours{787200, StepperDriver::stepLimit(true)};
  MotionAxis<60> _minutes{1883200, StepperDriver::stepLimit(false)};
  StepperDriver _stepper;
  CASIO _casio;
  String _casioMode = "off";
  bool _calibration_hint = false;
  int32_t _motionRevision = 1;
  time_t _lastChime = -1;
  logger_cb_t _logger = nullptr;
  int _remaining = 0;
  uint32_t _cycleId = 0;
  uint64_t _calibratedSteps = 0;
  int _settingIndex = 0, _currentIndex = -1, _zeroIndex = -1;
  unsigned long _currentMarkedAt = 0;
  bool _physicalJog = false;
  uint32_t _activeJogGeneration = 0;
  bool _minuteMoving = false;
  uint32_t _minuteGeneration = 0;
  bool _jogHeld[2] = {};
  uint32_t _jogGeneration[2] = {};

  QueueHandle_t _commands = nullptr;
  portMUX_TYPE _mutex = portMUX_INITIALIZER_UNLOCKED;
  static constexpr size_t RESULT_COUNT = 32;
  Result _results[RESULT_COUNT];
  Snapshot _snapshot;
  uint32_t _nextId = 1;
  uint32_t _epoch = 0;
  bool _stopPending = false;
  static constexpr size_t STOP_RESULT_COUNT = 8;
  Result _stopResults[STOP_RESULT_COUNT];
  size_t _stopSlot = STOP_RESULT_COUNT - 1;

  const char* execute(const Command& command);
  bool handle_stop();
  bool handle_maintenance();
  void finish(uint32_t id, const char* error);
  void finish_cycle(const char* error);
  void publish();
  void update_motion_revision();
  MotionSettings settings() const;
  bool configure(const MotionSettings& settings);
  void load_settings();
  bool persist(const MotionSettings& settings);
  bool advance(bool hour, bool minute);
  const char* live_calibrate(bool hour, int direction);
  void sync_to_current_time();
  void anchor_references();
  void anchor_references(int64_t minute, int64_t zoneOffset);
  void apply_ntp_sync();
  bool set_wall_time(time_t value, time_source_t source);
  void invalidate_reference(bool hour);
  void set_displayed_time(int hour, int minute);
  bool set_current_time(int hour, int minute, int second);
  const char* set_current_date(int day, int month, int year);
  void log_current_time();
  void start_setting_minutes();
  const char* mark_minute(bool zero);
  const char* step_setting_minutes();
  bool set_casio(const char* mode, bool hint);
};
#endif
