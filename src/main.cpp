#include "settings.h"

#if ENABLE_BUTTONS == 1
#include <espasyncbutton.hpp>
#endif

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <atomic>
#include <cstring>
#include "WifiManager.h"
#include "HTML.h"
#include "OtaManager.h"


#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include "ClockManager.h"
#include "NumberParsing.h"
#include "TimeZoneManager.h"

ClockManager clock_manager;
TimeZoneManager time_zone_manager;

#if ENABLE_BUTTONS == 1
AsyncEventButton left_button(PIN_LEFT_BUTTON, LOW);
AsyncEventButton right_button(PIN_RIGHT_BUTTON, LOW);
#endif

AsyncWebServer server(80);
WifiManager wifi_manager;
OtaManager ota_manager(clock_manager);
int32_t boot_id = 0;
enum class Reset { NONE, REBOOT, FACTORY, EXECUTING };
std::atomic<Reset> pending_reset{Reset::NONE};

namespace {
struct LogEntry {
  uint32_t sequence = 0;
  char message[256] = {};
};
constexpr size_t LOG_CAPACITY = 16;
LogEntry recent_log[LOG_CAPACITY];
uint32_t last_log_sequence = 0;
portMUX_TYPE log_mutex = portMUX_INITIALIZER_UNLOCKED;
}

// Override the SDK's weak time writer so SNTP and manual time share one owner.
extern "C" void sntp_sync_time(struct timeval* sample) {
  clock_manager.submit_ntp_sample(*sample);
}

void send_message(const char *format, ...) __attribute__((format(printf, 1, 2)));
void start_server();
void apply_pending_reset();
void start_time_zone_lookup();
#if ENABLE_BUTTONS == 1
void start_buttons();
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  clock_manager.set_logger(send_message);
  boot_id = int32_t(esp_random() % INT32_MAX) + 1;
  clock_manager.begin(boot_id);
  time_zone_manager.set_logger(send_message);
  time_zone_manager.begin();
  clock_manager.set_time_zone_manager(time_zone_manager);

#if ENABLE_BUTTONS == 1
  start_buttons();
#endif

  wifi_manager.begin();
  wifi_manager.start_mdns("clock");

#if ENABLE_SNTP == 1
  clock_manager.start_ntp();
#endif
  start_server();

}

void loop() {

  apply_pending_reset();
  clock_manager.tick();
  ota_manager.expire_preparation();
  if (!ota_manager.busy()) wifi_manager.loop();
  start_time_zone_lookup();

  delay(1);
}

void add_clock_state(JsonDocument& document, const ClockManager::Snapshot& snapshot) {
  document["boot"] = boot_id;
  document["state_revision"] = snapshot.stateRevision;
  document["paused"] = snapshot.paused;
  document["hour_referenced"] = snapshot.hourReferenced;
  document["minute_referenced"] = snapshot.minuteReferenced;
  document["time_source"] = snapshot.timeSource == SNTP ? "ntp" :
                            snapshot.timeSource == WEB ? "web" :
                            snapshot.timeSource == BUTTONS ? "buttons" : "none";
}

void start_time_zone_lookup() {
  static std::atomic<bool> running{false};
  static unsigned long lastAttempt = 0;
  static bool attempted = false;
  if (running.load() || ota_manager.busy() || time_zone_manager.configured() ||
      WiFi.status() != WL_CONNECTED || (attempted && millis() - lastAttempt < 300000)) return;
  running.store(true);
  lastAttempt = millis();
  attempted = true;
  if (xTaskCreate([](void*) {
    const String zone = time_zone_manager.fetch_from_api();
    ClockManager::Command command;
    command.action = ClockManager::Action::TIME_ZONE;
    command.a = 1;
    if (!zone.isEmpty() && zone.length() < sizeof(command.text)) {
      zone.toCharArray(command.text, sizeof(command.text));
      clock_manager.submit(command);
    }
    running.store(false);
    vTaskDelete(nullptr);
  }, "clock-tz", 6144, nullptr, 1, nullptr) != pdPASS) running.store(false);
}

void send_message(const char *format, ...) {
  char buffer[sizeof(LogEntry::message)] = {};
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.println(buffer);

  portENTER_CRITICAL(&log_mutex);
  const uint32_t sequence = ++last_log_sequence;
  auto& entry = recent_log[sequence % LOG_CAPACITY];
  memcpy(entry.message, buffer, sizeof(buffer));
  entry.sequence = sequence;
  portEXIT_CRITICAL(&log_mutex);
}

String read_device_log() {
  portENTER_CRITICAL(&log_mutex);
  const uint32_t last = last_log_sequence;
  portEXIT_CRITICAL(&log_mutex);
  String response;
  response.reserve(LOG_CAPACITY * sizeof(LogEntry::message));
  const size_t count = last < LOG_CAPACITY ? last : LOG_CAPACITY;
  for (size_t remaining = count; remaining > 0; --remaining) {
    const uint32_t sequence = last - remaining + 1;
    portENTER_CRITICAL(&log_mutex);
    const auto entry = recent_log[sequence % LOG_CAPACITY];
    portEXIT_CRITICAL(&log_mutex);
    if (entry.sequence != sequence) continue;
    response += entry.message;
    response += '\n';
  }
  return response;
}

void apply_pending_reset() {
  if (pending_reset.load() == Reset::NONE) return;

  delay(150); // Let the request task send its response before closing Wi-Fi.
  const Reset reset = pending_reset.exchange(Reset::EXECUTING);
  if (reset == Reset::FACTORY) {
    const char* namespaces[] = {"wifi", "time", "base-steps", "offsets", "motion", "time-zone"};
    for (const char* name : namespaces) {
      Preferences prefs;
      if (prefs.begin(name, false)) {
        if (!prefs.clear()) send_message("Could not clear settings namespace %s", name);
        prefs.end();
      } else {
        send_message("Could not open settings namespace %s", name);
      }
    }
    if (!WiFi.eraseAP()) send_message("Could not clear Wi-Fi driver settings");
  }
  ESP.restart();
}

bool schedule_reset(Reset requested) {
  Reset pending = pending_reset.load();
  while (pending != Reset::EXECUTING) {
    if (pending == Reset::FACTORY || pending == requested) return true;
    if (pending_reset.compare_exchange_weak(pending, requested)) return true;
  }
  return false;
}

void sendCommand(AsyncWebServerRequest* request, ClockManager::Command command) {
  const uint32_t id = clock_manager.submit(command);
  if (!id) {
    request->send(503, "text/plain", "Command queue full. Please retry.");
    return;
  }
  request->send(202, "application/json", "{\"id\":" + String(id) + ",\"boot\":" + String(boot_id) + "}");
}

void sendAction(AsyncWebServerRequest* request, ClockManager::Action action,
                int a = 0, int b = 0, int c = 0) {
  ClockManager::Command command;
  command.action = action;
  command.a = a;
  command.b = b;
  command.c = c;
  sendCommand(request, command);
}


bool readInteger(AsyncWebServerRequest* request, const char* name, int32_t& value) {
  return request->hasParam(name, true) &&
         parseInteger(request->getParam(name, true)->value().c_str(), value);
}

void sendTimeAction(AsyncWebServerRequest* request, ClockManager::Action action) {
  int32_t hour, minute;
  if (!readInteger(request, "h", hour) || !readInteger(request, "m", minute)) {
    request->send(400, "text/plain", "Enter a numeric hour and minute.");
    return;
  }
  sendAction(request, action, hour, minute);
}

void start_server() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", CONFIG_HTML);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
  });

  server.on("/set-displayed", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendTimeAction(request, ClockManager::Action::DISPLAYED_TIME);
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendTimeAction(request, ClockManager::Action::CURRENT_TIME);
  });

  server.on("/set-date", HTTP_POST, [](AsyncWebServerRequest *request) {
    int32_t day, month, year;
    if (!readInteger(request, "d", day) || !readInteger(request, "m", month) ||
        !readInteger(request, "y", year)) {
      request->send(400, "text/plain", "Enter a numeric day, month and year.");
      return;
    }
    sendAction(request, ClockManager::Action::CURRENT_DATE, day, month, year);
  });

  server.on("/set-tz", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("tz", true)) {
      request->send_P(400, "text/plain", "Missing parameters");
      return;
    }
    const String zone = request->getParam("tz", true)->value();
    ClockManager::Command command;
    command.action = ClockManager::Action::TIME_ZONE;
    if (zone.isEmpty() || zone.length() >= sizeof(command.text)) {
      request->send(400, "text/plain", "Enter a supported time zone name.");
      return;
    }
    zone.toCharArray(command.text, sizeof(command.text));
    sendCommand(request, command);
  });

  server.on("/use-ntp", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::USE_NTP);
  });

  server.on("/set-casio", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("casio", true)) {
      
      String casio = request->getParam("casio", true)->value();
      bool calibration_hint = request->hasParam("calibration_hint", true);

      if (casio != "off" && casio != "on" && casio != "day") {
        request->send(400, "text/plain", "Invalid CASIO mode.");
        return;
      }
      ClockManager::Command command;
      command.action = ClockManager::Action::CASIO_MODE;
      command.a = calibration_hint;
      snprintf(command.text, sizeof(command.text), "%s", casio.c_str());
      sendCommand(request, command);
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (ota_manager.busy() || pending_reset.load() != Reset::NONE) {
      request->send(409, "text/plain", "Wait until the firmware update or restart finishes.");
      return;
    }
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();

      if (!WifiManager::valid_credentials(ssid, password)) {
        request->send(400, "text/plain", "Enter a valid SSID and leave the password empty for an open network, or enter a valid WPA key.");
      } else if (!wifi_manager.set_credentials(ssid, password)) {
        request->send(500, "text/plain", "Could not save Wi-Fi credentials.");
      } else {
        request->send(200, "text/plain", "Credentials saved. Connecting to Wi-Fi.");
      }

    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/calibrate-hour", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::CALIBRATE_HOUR);
  });

  server.on("/calibrate-minute", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::CALIBRATE_MINUTE);
  });

  server.on("/calibrate-end", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::STOP);
  });

  server.on("/resume", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::RESUME);
  });


  server.on("/live-calibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("type", true) && request->hasParam("direction", true)) {
      String type = request->getParam("type", true)->value();
      int32_t direction;
      if ((type != "hour" && type != "minute") || !readInteger(request, "direction", direction) ||
          (direction != -1 && direction != 1)) {
        request->send(400, "text/plain", "Invalid calibration axis or direction.");
        return;
      }
      sendAction(request, type == "hour" ? ClockManager::Action::LIVE_HOUR
                                          : ClockManager::Action::LIVE_MINUTE, direction);
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/fast-forward-hours", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::FAST_HOURS);
  });

  server.on("/fast-forward-minutes", HTTP_POST, [](AsyncWebServerRequest *request) {
    sendAction(request, ClockManager::Action::FAST_MINUTES);
  });

  server.on("/get-settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument settings(3072);
    settings["wifi_ssid"] = wifi_manager.saved_ssid();

    Preferences prefs;
    if (prefs.begin("time", true)) {
      settings["tz_name"] = prefs.getString("tz_name", "");
      prefs.end();
    }

    const time_t now = time(nullptr);
    struct tm localTime;
    if (localtime_r(&now, &localTime)) {
      auto date = settings.createNestedObject("date");
      date["day"] = localTime.tm_mday;
      date["month"] = localTime.tm_mon + 1;
      date["year"] = localTime.tm_year + 1900;
    }

    const auto snapshot = clock_manager.snapshot();
    const auto& config = snapshot.settings;
    add_clock_state(settings, snapshot);
    settings["max_year"] = sizeof(time_t) == 4 ? 2038 : 9999;
    settings["motion_revision"] = snapshot.motionRevision;
    settings["casio_mode"] = String(snapshot.casioMode);
    settings["casio_cal_hint"] = snapshot.calibrationHint;
    auto base = settings.createNestedObject("base_steps");
    base["hour"] = config.hour_target / 100;
    base["hour_target_x100"] = config.hour_target;
    base["minute"] = config.minute_target / 100;
    base["minute_target_x100"] = config.minute_target;

    auto offsets = settings.createNestedObject("offsets");
    auto hours = offsets.createNestedArray("hours");
    for (auto offset : config.hours) hours.add(offset);
    auto minutes = offsets.createNestedArray("minutes");
    for (auto offset : config.minutes) minutes.add(offset);

    if (settings.overflowed()) {
      request->send(500, "text/plain", "Could not assemble settings.");
      return;
    }
    String response;
    serializeJson(settings, response);
    request->send(200, "application/json", response);
  });

  server.on("/set-offsets", HTTP_POST, [](AsyncWebServerRequest *request) {
    ClockManager::Command command;
    command.action = ClockManager::Action::SETTINGS;
    if (request->hasParam("motion_revision", true) &&
        (!readInteger(request, "motion_revision", command.expectedMotionRevision) ||
         command.expectedMotionRevision <= 0)) {
      request->send(400, "text/plain", "Invalid motion settings revision.");
      return;
    }
    if (!request->hasParam("hour_target", true) || !request->hasParam("minute_target", true) ||
        !parseTarget(request->getParam("hour_target", true)->value().c_str(), command.settings.hour_target) ||
        !parseTarget(request->getParam("minute_target", true)->value().c_str(), command.settings.minute_target)) {
      request->send(400, "text/plain", "Targets must be positive numbers (at least 1), with up to two decimals.");
      return;
    }
    for (int i = 0; i < 60; ++i) {
      char key[5];
      snprintf(key, sizeof(key), "m%d", i);
      if (!request->hasParam(key, true) ||
          !parseInteger(request->getParam(key, true)->value().c_str(), command.settings.minutes[i])) {
        request->send(400, "text/plain", "All minute offsets must be valid integers.");
        return;
      }
      if (i < 24) {
        snprintf(key, sizeof(key), "h%d", i);
        if (!request->hasParam(key, true) ||
            !parseInteger(request->getParam(key, true)->value().c_str(), command.settings.hours[i])) {
          request->send(400, "text/plain", "All hour offsets must be valid integers.");
          return;
        }
      }
    }
    sendCommand(request, command);
  });

  server.on("/command-status", HTTP_GET, [](AsyncWebServerRequest *request) {
    int32_t id = 0;
    int32_t boot = 0;
    ClockManager::Result result;
    if (!request->hasParam("id") || !parseInteger(request->getParam("id")->value().c_str(), id) || id <= 0) {
      request->send(400, "text/plain", "Invalid command id.");
    } else if (!request->hasParam("boot") || !parseInteger(request->getParam("boot")->value().c_str(), boot) ||
               boot != boot_id || !clock_manager.result(uint32_t(id), result)) {
      request->send(404, "text/plain", "Command status expired. Reload settings to verify the result.");
    } else {
      StaticJsonDocument<384> status;
      status["done"] = result.done;
      status["success"] = result.success;
      status["motion_revision"] = result.motionRevision;
      status["message"] = String(result.message);
      add_clock_state(status, clock_manager.snapshot());
      String response;
      serializeJson(status, response);
      request->send(200, "application/json", response);
    }
  });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    const bool scheduled = !ota_manager.busy() && schedule_reset(Reset::REBOOT);
    request->send(scheduled ? 200 : 409, "text/plain", scheduled ? "Reboot scheduled." : "Firmware update or restart in progress.");
  });

  server.on("/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    const bool scheduled = !ota_manager.busy() && schedule_reset(Reset::FACTORY);
    request->send(scheduled ? 200 : 409, "text/plain", scheduled ? "Factory reset scheduled." : "Firmware update or restart in progress.");
  });

  server.on("/clock-state", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<192> document;
    add_clock_state(document, clock_manager.snapshot());
    String response;
    serializeJson(document, response);
    request->send(200, "application/json", response);
  });
  server.on("/device-log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", read_device_log());
  });

  ota_manager.begin(server, [] { schedule_reset(Reset::REBOOT); },
                    [] { return pending_reset.load() == Reset::NONE; });

  server.begin();
}

#if ENABLE_BUTTONS == 1
struct MinuteGesture {
  uint32_t generation = 0;
  bool started = false;
  bool valid = false;
};
MinuteGesture minute_gesture;

void capture_minute_gesture() {
  uint32_t generation;
  const bool stationary = clock_manager.capture_minute_input(generation);
  if (!minute_gesture.started) {
    minute_gesture.generation = generation;
    minute_gesture.started = true;
    minute_gesture.valid = stationary;
  } else if (!stationary || generation != minute_gesture.generation) {
    minute_gesture.valid = false;
  }
}

void finish_minute_gesture(ClockManager::Action action, int direction) {
  if (!minute_gesture.valid || !clock_manager.request_minute_input(action, direction, minute_gesture.generation)) {
    send_message("Minute input ignored. Wait for the digit to stop and try again.");
  }
  minute_gesture = {};
}

void start_buttons() {
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  left_button.begin();

  left_button.onLongPress([]() {
    send_message("Left long press");
    clock_manager.press_jog(true);
  });

  left_button.onLongRelease([]() {
    send_message("Left long release");
    clock_manager.release_jog(true);
  });

  left_button.onClick([]() {
    send_message("Left click");
    clock_manager.increment_hour();
  });

  left_button.onMultiClick([](int32_t counter) {
    if (counter == 2) {
      send_message("Left double click");
      clock_manager.decrement_hour();
    } else if (counter == 3) {
      clock_manager.request(ClockManager::Action::REFERENCE_HOUR);
    }
  });

  left_button.enable();

  right_button.begin();
  right_button.onPress(capture_minute_gesture);
  right_button.onRelease(capture_minute_gesture);

  right_button.onLongPress([]() {
    send_message("Right long press");
    minute_gesture = {};
    clock_manager.press_jog(false);
  });

  right_button.onLongRelease([]() {
    send_message("Right long release");
    minute_gesture = {};
    clock_manager.release_jog(false);
  });

  right_button.onClick([]() {
    send_message("Right click");
    finish_minute_gesture(ClockManager::Action::ADJUST_MINUTE, -1);
  });

  right_button.onMultiClick([](int32_t counter) {
    if (counter == 2) {
      send_message("Right double click");
      finish_minute_gesture(ClockManager::Action::ADJUST_MINUTE, 1);
    } else if (counter == 3) {
      send_message("Right triple click");
      finish_minute_gesture(ClockManager::Action::SET_MINUTES, 0);
    } else {
      minute_gesture = {};
    }
  });

  right_button.enable();
}
#endif
