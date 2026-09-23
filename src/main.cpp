#include "settings.h"

#if ENABLE_BUTTONS == 1
#include <espasyncbutton.hpp>
#endif

#if ENABLE_WIFI == 1
#include <ESPAsyncWebServer.h>
#include "WifiManager.h"
#include "HTML.h"
#endif

#if ENABLE_OTA == 1
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <AsyncElegantOTA.h>
#endif


#include <Preferences.h>
#include <time.h>
#include <sys/time.h>

#include "ClockManager.h"
#include "NumberParsing.h"
#include "TimeZoneManager.h"

ClockManager clock_manager;
TimeZoneManager time_zone_manager;

#if ENABLE_BUTTONS == 1
AsyncEventButton left_button(PIN_LEFT_BUTTON, LOW);
AsyncEventButton right_button(PIN_RIGHT_BUTTON, LOW);
#endif

#if ENABLE_WIFI == 1
AsyncWebServer server(80);
AsyncEventSource events("/events");
WifiManager wifi_manager;
#endif


void send_message(const char *format, ...) __attribute__((format(printf, 1, 2)));
#if ENABLE_WIFI == 1
void start_server();
#endif
#if ENABLE_BUTTONS == 1
void start_buttons();
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  clock_manager.set_logger(send_message);
  clock_manager.begin();

#if ENABLE_BUTTONS == 1
  start_buttons();
#endif

#if ENABLE_WIFI == 1
  wifi_manager.begin();
  wifi_manager.start_mdns("clock");

  start_server();
#endif

#if ENABLE_WIFI == 1 && ENABLE_SNTP == 1
  clock_manager.start_ntp(); 
#endif

  if(clock_manager.time_source == NONE) {
    clock_manager.set_current_time(0,0,0);
  }

  time_zone_manager.set_logger(send_message);
  time_zone_manager.begin(&clock_manager);

}

void loop() {

  clock_manager.tick();
#if ENABLE_WIFI == 1
  wifi_manager.loop();
#endif

  delay(1);
}

void send_message(const char *format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, 255, format, args);
  va_end(args);

  Serial.println(buffer);

#if ENABLE_WIFI == 1
  events.send(buffer, "message", millis());
#endif
}

#if ENABLE_WIFI == 1

void sendCommand(AsyncWebServerRequest* request, ClockManager::Command command) {
  const uint32_t id = clock_manager.submit(command);
  if (!id) {
    request->send(503, "text/plain", "Command queue full. Please retry.");
    return;
  }
  request->send(202, "application/json", "{\"id\":" + String(id) + "}");
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


void start_server() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", CONFIG_HTML);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
  });

  server.on("/set-displayed", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("h", true) && request->hasParam("m", true)) {
      int h = request->getParam("h", true)->value().toInt();
      int m = request->getParam("m", true)->value().toInt();

      sendAction(request, ClockManager::Action::DISPLAYED_TIME, h, m);

    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("h", true) && request->hasParam("m", true)) {
      int h = request->getParam("h", true)->value().toInt();
      int m = request->getParam("m", true)->value().toInt();

      sendAction(request, ClockManager::Action::CURRENT_TIME, h, m);
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-date", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("d", true) && request->hasParam("m", true) && request->hasParam("y", true)) {
      int d = request->getParam("d", true)->value().toInt();
      int m = request->getParam("m", true)->value().toInt();
      int y = request->getParam("y", true)->value().toInt();

      sendAction(request, ClockManager::Action::CURRENT_DATE, d, m, y);
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-tz", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("tz", true)) {
      String tz = request->getParam("tz", true)->value();
      time_zone_manager.set(tz);

      request->send_P(200, "text/plain", "OK");
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
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
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String password = request->getParam("password", true)->value();

      wifi_manager.set_credentials(ssid, password);

      request->send(200, "text/plain", ssid);
      delay(3000);
      wifi_manager.begin();

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
      int direction = request->getParam("direction", true)->value().toInt();
      
      if ((type != "hour" && type != "minute") || (direction != -1 && direction != 1)) {
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
    String response = "{";

    // WiFi SSID
    Preferences wifi_prefs;
    wifi_prefs.begin("wifi", true);
    response += "\"wifi_ssid\":\"" + wifi_prefs.getString("ssid", "") + "\",";
    wifi_prefs.end();

    // Time Zone
    Preferences tz_prefs;
    tz_prefs.begin("time", true);
    response += "\"tz_name\":\"" + tz_prefs.getString("tz_name", "") + "\",";
    tz_prefs.end();

    // CASIO mode
    Preferences time_prefs;
    time_prefs.begin("time", true);
    response += "\"casio_mode\":\"" + time_prefs.getString("casio", "off") + "\",";
    response += "\"casio_cal_hint\":" + String(time_prefs.getBool("cal_hint", false)) + ",";
    time_prefs.end();

    // Date
    struct tm timeinfo;
    time_t now = time(0);
    localtime_r(&now, &timeinfo);
    response += "\"date\":{";
    response += "\"day\":" + String(timeinfo.tm_mday) + ",";
    response += "\"month\":" + String(timeinfo.tm_mon + 1) + ",";
    response += "\"year\":" + String(timeinfo.tm_year + 1900);
    response += "},";

    const auto snapshot = clock_manager.snapshot();
    const auto& config = snapshot.settings;
    response += "\"paused\":" + String(snapshot.paused ? "true" : "false") + ",";
    response += "\"base_steps\":{";
    response += "\"hour\":" + String(config.hour_target / 100) + ",";
    response += "\"hour_target_x100\":" + String(config.hour_target) + ",";
    response += "\"minute\":" + String(config.minute_target / 100) + ",";
    response += "\"minute_target_x100\":" + String(config.minute_target);
    response += "},";
    response += "\"offsets\":{\"hours\":[";
    for (int i = 0; i < 24; ++i) {
      response += String(config.hours[i]);
      if (i < 23) response += ",";
    }
    response += "],\"minutes\":[";
    for (int i = 0; i < 60; ++i) {
      response += String(config.minutes[i]);
      if (i < 59) response += ",";
    }
    response += "]}}";

    request->send(200, "application/json", response);
  });

  server.on("/set-offsets", HTTP_POST, [](AsyncWebServerRequest *request) {
    ClockManager::Command command;
    command.action = ClockManager::Action::SETTINGS;
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
    ClockManager::Result result;
    if (!request->hasParam("id") || !parseInteger(request->getParam("id")->value().c_str(), id) || id <= 0) {
      request->send(400, "text/plain", "Invalid command id.");
    } else if (!clock_manager.result(uint32_t(id), result)) {
      request->send(404, "text/plain", "Command status expired. Reload settings to verify the result.");
    } else {
      String response = "{\"done\":";
      response += result.done ? "true" : "false";
      response += ",\"success\":";
      response += result.success ? "true" : "false";
      response += ",\"message\":\"" + String(result.message) + "\"}";
      request->send(200, "application/json", response);
    }
  });

  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", "OK");
    delay(100);
    ESP.restart();
  });

  server.on("/factory-reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", "OK");
    
    // Clear all preferences
    const char* namespaces[] = {"wifi", "time", "base-steps", "offsets", "motion", "time-zone"};
    for (const char* ns : namespaces) {
      Preferences p;
      p.begin(ns, false);
      p.clear();
      p.end();
    }
    
    delay(100);
    ESP.restart();
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected. Last message ID that it got is: %u\n", client->lastId());
    }

    client->send("Connected", NULL, millis(), 500);
  });
  server.addHandler(&events);

# if ENABLE_OTA == 1
  AsyncElegantOTA.begin(&server);
# endif

  server.begin();
}
#endif

#if ENABLE_BUTTONS == 1
void start_buttons() {
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  left_button.begin();

  left_button.onLongPress([]() {
    send_message("Left long press");
    clock_manager.request_calibrate_hour();
  });

  left_button.onLongRelease([]() {
    send_message("Left long release");
    clock_manager.request_end_calibrate();
  });

  left_button.onClick([]() {
    send_message("Left click");
    clock_manager.increment_hour();
  });

  left_button.onMultiClick([](int32_t counter) {
    if (counter == 2) {
      send_message("Left double click");
      clock_manager.decrement_hour();
    }
  });

  left_button.enable();

  right_button.begin();

  right_button.onLongPress([]() {
    send_message("Right long press");
    clock_manager.request_calibrate_minute();
  });

  right_button.onLongRelease([]() {
    send_message("Right long release");
    clock_manager.request_end_calibrate();
  });

  right_button.onClick([]() {
    send_message("Right click");
    clock_manager.increment_minute();
  });

  right_button.onMultiClick([](int32_t counter) {
    if (counter == 2) {
      send_message("Right double click");
      clock_manager.decrement_minute();
    } else if (counter == 3) {
      send_message("Right triple click");
      clock_manager.request_set_minutes();
    }
  });

  right_button.enable();
}
#endif
