#ifndef TIMEZONEMANAGER_H
#define TIMEZONEMANAGER_H

#include "ClockManager.h"

#include <Arduino.h>
#include <Preferences.h>

typedef void (*logger_cb_t) (const char * format, ...);

class TimeZoneManager {
public:
  void begin(ClockManager *clock);
  void set(String tz);
  void set_from_api();
  void set_logger(logger_cb_t logger);

private:
  Preferences _preferences;
  logger_cb_t _logger;

  const char* convert_name(const char* tzdata);  
};

extern const char TIME_ZONE_NAMES[];
extern const char TIME_ZONE_STRINGS[];

#endif // TIMEZONEMANAGER_H