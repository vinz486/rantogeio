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
#include "TimeZoneManager.h"

// Global variables for configurable base steps (in microsteps)
int HOUR_STEPS_BASE = 7872;    // Default value (492 * 16 microstepping)
int MINUTE_STEPS_BASE = 18832; // Default value (1177 * 16 microstepping)

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


typedef void (*logger_cb_t) (const char * format, ...);

void send_message(const char *format, ...);
#if ENABLE_WIFI == 1
void start_server();
#endif
#if ENABLE_BUTTONS == 1
void start_buttons();
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

#if ENABLE_BUTTONS == 1
  start_buttons();
#endif

  // Load base steps from preferences with migration from old format
  Preferences base_prefs;
  base_prefs.begin("base-steps", true);
  HOUR_STEPS_BASE = base_prefs.getInt("hour", 7872);
  MINUTE_STEPS_BASE = base_prefs.getInt("minute", 18832);
  
  // Migrate old values (if they're in the old format without microstepping)
  if (HOUR_STEPS_BASE < 1000) {  // Old format was ~492
    send_message("Migrating old base steps format...");
    HOUR_STEPS_BASE *= 16;
    MINUTE_STEPS_BASE *= 16;
    base_prefs.end();
    base_prefs.begin("base-steps", false);
    base_prefs.putInt("hour", HOUR_STEPS_BASE);
    base_prefs.putInt("minute", MINUTE_STEPS_BASE);
    send_message("Migrated: hour=%d, minute=%d", HOUR_STEPS_BASE, MINUTE_STEPS_BASE);
  }
  base_prefs.end();

  clock_manager.set_logger(send_message);
  clock_manager.begin();

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

  delay(1000);
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

String test = "Ciao";


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

      clock_manager.set_displayed_time(h, m);
      request->send_P(200, "text/plain", "OK");

    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("h", true) && request->hasParam("m", true)) {
      int h = request->getParam("h", true)->value().toInt();
      int m = request->getParam("m", true)->value().toInt();

      clock_manager.set_current_time(h, m, 0);
      clock_manager.time_source = WEB;

      request->send_P(200, "text/plain", "OK");
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/set-date", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("d", true) && request->hasParam("m", true) && request->hasParam("y", true)) {
      int d = request->getParam("d", true)->value().toInt();
      int m = request->getParam("m", true)->value().toInt();
      int y = request->getParam("y", true)->value().toInt();

      clock_manager.set_current_date(d, m, y);

      request->send_P(200, "text/plain", "OK");
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

      clock_manager.set_casio(casio, calibration_hint);

      request->send_P(200, "text/plain", "OK");
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
    clock_manager.request_calibrate_hour();
    request->send_P(200, "text/plain", "OK");
  });

  server.on("/calibrate-minute", HTTP_POST, [](AsyncWebServerRequest *request) {
    clock_manager.request_calibrate_minute();
    request->send_P(200, "text/plain", "OK");
  });

  server.on("/calibrate-end", HTTP_POST, [](AsyncWebServerRequest *request) {
    clock_manager.request_end_calibrate();
    request->send_P(200, "text/plain", "OK");
  });

  server.on("/toggle-demo", HTTP_POST, [](AsyncWebServerRequest *request) {
    clock_manager.toggle_demo();
    request->send_P(200, "text/plain", "OK");
  });

  server.on("/live-calibrate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("type", true) && request->hasParam("direction", true)) {
      String type = request->getParam("type", true)->value();
      int direction = request->getParam("direction", true)->value().toInt();
      
      bool is_hour = (type == "hour");
      clock_manager.live_calibrate_step(is_hour, direction);
      
      request->send_P(200, "text/plain", "OK");
    } else {
      request->send_P(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/fast-forward-hours", HTTP_POST, [](AsyncWebServerRequest *request) {
    clock_manager.request_fast_forward_hours();
    request->send_P(200, "text/plain", "OK");
  });

  server.on("/fast-forward-minutes", HTTP_POST, [](AsyncWebServerRequest *request) {
    clock_manager.request_fast_forward_minutes();
    request->send_P(200, "text/plain", "OK");
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
    tz_prefs.begin("time-zone", true);
    response += "\"tz_name\":\"" + tz_prefs.getString("name", "") + "\",";
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

    // Base steps
    response += "\"base_steps\":{";
    response += "\"hour\":" + String(HOUR_STEPS_BASE) + ",";
    response += "\"minute\":" + String(MINUTE_STEPS_BASE);
    response += "},";

    // Offsets
    response += "\"offsets\":{\"hours\":[";
    Preferences offsets_prefs;
    offsets_prefs.begin("offsets", true);
    for (int i = 0; i < 24; i++) {
      char key[5];
      sprintf(key, "h%d", i);
      response += offsets_prefs.getInt(key, 0);
      if (i < 23) response += ",";
    }
    response += "],\"minutes\":[";
    for (int i = 0; i < 60; i++) {
      char key[5];
      sprintf(key, "m%d", i);
      response += offsets_prefs.getInt(key, 0);
      if (i < 59) response += ",";
    }
    response += "]}}";
    offsets_prefs.end();

    request->send(200, "application/json", response);
  });

  server.on("/set-offsets", HTTP_POST, [](AsyncWebServerRequest *request) {
    int hour_offsets[24];
    int minute_offsets[60];

    // Handle base steps if provided
    if (request->hasParam("base_hour", true)) {
      HOUR_STEPS_BASE = request->getParam("base_hour", true)->value().toInt();
    }
    if (request->hasParam("base_minute", true)) {
      MINUTE_STEPS_BASE = request->getParam("base_minute", true)->value().toInt();
    }

    // Save base steps to preferences
    if (request->hasParam("base_hour", true) || request->hasParam("base_minute", true)) {
      Preferences base_prefs;
      base_prefs.begin("base-steps", false);
      base_prefs.putInt("hour", HOUR_STEPS_BASE);
      base_prefs.putInt("minute", MINUTE_STEPS_BASE);
      base_prefs.end();
    }

    for (int i = 0; i < 24; i++) {
      char key[5];
      sprintf(key, "h%d", i);
      if (request->hasParam(key, true)) {
        hour_offsets[i] = request->getParam(key, true)->value().toInt();
      } else {
        hour_offsets[i] = 0;
      }
    }

    for (int i = 0; i < 60; i++) {
      char key[5];
      sprintf(key, "m%d", i);
      if (request->hasParam(key, true)) {
        minute_offsets[i] = request->getParam(key, true)->value().toInt();
      } else {
        minute_offsets[i] = 0;
      }
    }

    clock_manager.save_offsets(hour_offsets, minute_offsets);
    request->send_P(200, "text/plain", "OK");
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
    } else if (counter == 3) {
      send_message("Left triple click");
      clock_manager.toggle_demo();
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
