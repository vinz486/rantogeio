#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "TimeZoneManager.h"

void TimeZoneManager::set_logger(logger_cb_t logger) {
  _logger = logger;
}


void TimeZoneManager::begin() {
  Preferences prefs;
  prefs.begin("time", true);
  const String name = prefs.getString("tz_name", "");
  const String legacy = prefs.getString("tz", "");
  prefs.end();
  const char* tz = convert_name(name.c_str());
  if (!tz && name.isEmpty() && !legacy.isEmpty()) tz = legacy.c_str();

  if (tz) {
    _configured = setenv("TZ", tz, 1) == 0;
    if (_configured) tzset();
    if (_configured && _logger) (*_logger)("Loaded TZ = %s", tz);
  }
}

TimeZoneManager::Result TimeZoneManager::set(const String& tz) {
  const char* posix = convert_name(tz.c_str());
  if (!posix) return Result::UNKNOWN_ZONE;
  Preferences prefs;
  if (!prefs.begin("time", false)) return Result::SAVE_FAILED;
  const bool saved = prefs.putString("tz_name", tz) > 0;
  prefs.end();
  if (!saved) return Result::SAVE_FAILED;

  if (setenv("TZ", posix, 1) != 0) return Result::SAVE_FAILED;
  tzset();
  _configured = true;
  if (_logger) (*_logger)("Set time zone to %s (%s)", tz.c_str(), posix);
  return Result::OK;
}

String TimeZoneManager::fetch_from_api() {
  WiFiClient client;
  HTTPClient http;

  if (_logger) (*_logger)("Requesting time zone from ip-api.com");

  http.begin(client, "http://ip-api.com/line/?fields=256");
  int httpResponseCode = http.GET();

  String payload = "";
  if (httpResponseCode == 200) {
    payload = http.getString();
    payload.trim();
  } else {
    if (_logger) (*_logger)("Time zone HTTP error: %d", httpResponseCode);
  }
  http.end();
  return payload;
}

const char* TimeZoneManager::convert_name(const char* tzdata) {
  if (strcmp(tzdata, "UTC") == 0 || strcmp(tzdata, "UTC0") == 0) return "UTC0";
  if (strcmp(tzdata, "America/Asuncion") == 0) return "<-03>3";
  // These zones need dated transitions beyond what a recurring POSIX rule can express.
  if (strcmp(tzdata, "Africa/Casablanca") == 0 || strcmp(tzdata, "Africa/El_Aaiun") == 0) return nullptr;
  int offset = 0;
  const char* current = TIME_ZONE_NAMES;
  while (strlen(current) > 0) {
    if (strcmp(tzdata, current) == 0) {
      const char* posix = TIME_ZONE_STRINGS;
      while (offset > 0 || strcmp(posix, "~") == 0) {
        posix = posix + strlen(posix) + 1;
        offset--;
      }
      return posix;
    }

    current = current + strlen(current) + 1;
    offset++;
  }

  return nullptr;
}
