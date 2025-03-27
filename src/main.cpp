#include <Arduino.h>
#include "settings.h"

#if ENABLE_BUTTONS == 1
#include <OneButton.h>
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

ClockManager clock_manager;
TimeZoneManager time_zone_manager;

#if ENABLE_BUTTONS == 1
OneButton left_button(PIN_LEFT_BUTTON, true); // true = LOW active
OneButton right_button(PIN_RIGHT_BUTTON, true); // true = LOW active
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
#if ENABLE_BUTTONS == 1
  left_button.tick();
  right_button.tick();
#endif

  clock_manager.tick();

  delay(10); // Ridotto per garantire una migliore reattività dei pulsanti
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

String processor(const String& var)
{
  if(var == "TEST_VAR")
    return F("Hello world!");
  return String();
}

void start_server() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", CONFIG_HTML, processor);
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
      clock_manager.set_casio(casio);

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
// Handler per click singolo sul pulsante sinistro
void handleLeftClick() {
  send_message("Left click");
  clock_manager.increment_hour();
}

// Handler per doppio click sul pulsante sinistro
void handleLeftDoubleClick() {
  send_message("Left double click");
  clock_manager.decrement_hour();
}

// Handler per click lungo sul pulsante sinistro
void handleLeftLongPressStart() {
  send_message("Left long press");
  clock_manager.request_calibrate_hour();
}

// Handler per rilascio dopo click lungo sul pulsante sinistro
void handleLeftLongPressStop() {
  send_message("Left long release");
  clock_manager.request_end_calibrate();
}

// Handler per click multiplo sul pulsante sinistro (callback senza parametri)
void handleLeftMultiClick() {
  send_message("Left triple click");
  clock_manager.toggle_demo();
}

// Handler per click singolo sul pulsante destro
void handleRightClick() {
  send_message("Right click");
  clock_manager.increment_minute();
}

// Handler per doppio click sul pulsante destro
void handleRightDoubleClick() {
  send_message("Right double click");
  clock_manager.decrement_minute();
}

// Handler per click lungo sul pulsante destro
void handleRightLongPressStart() {
  send_message("Right long press");
  clock_manager.request_calibrate_minute();
}

// Handler per rilascio dopo click lungo sul pulsante destro
void handleRightLongPressStop() {
  send_message("Right long release");
  clock_manager.request_end_calibrate();
}

// Handler per click multiplo sul pulsante destro (callback senza parametri)
void handleRightMultiClick() {
  send_message("Right triple click");
  clock_manager.request_set_minutes();
}

void start_buttons() {
  // Configurazione per il pulsante sinistro
  left_button.attachClick(handleLeftClick);
  left_button.attachDoubleClick(handleLeftDoubleClick);
  left_button.attachLongPressStart(handleLeftLongPressStart);
  left_button.attachLongPressStop(handleLeftLongPressStop);
  left_button.attachMultiClick(handleLeftMultiClick);
  
  // Configurazione per il pulsante destro
  right_button.attachClick(handleRightClick);
  right_button.attachDoubleClick(handleRightDoubleClick);
  right_button.attachLongPressStart(handleRightLongPressStart);
  right_button.attachLongPressStop(handleRightLongPressStop);
  right_button.attachMultiClick(handleRightMultiClick);
}
#endif