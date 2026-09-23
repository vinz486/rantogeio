#pragma once
#include "Arduino.h"
#include "WiFi.h"
namespace http {
extern int status;
extern String payload;
extern bool ended;
}
class HTTPClient {
public:
  void begin(WiFiClient&, const char*) { http::ended = false; }
  int GET() { return http::status; }
  String getString() { return http::payload; }
  void end() { http::ended = true; }
};
