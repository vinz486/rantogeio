#pragma once
#include "Arduino.h"
constexpr int WL_CONNECTED = 3;
enum WiFiMode { WIFI_OFF, WIFI_STA, WIFI_AP, WIFI_AP_STA };
class WiFiClient {};
struct HostWiFi {
  WiFiMode currentMode = WIFI_OFF;
  int connection = 0;
  bool persistentSettings = true;
  bool autoReconnect = false;
  unsigned int attempts = 0;
  unsigned int reconnects = 0;
  String ssid, password;
  String connectedSsid;

  int status() const { return connection; }
  WiFiMode getMode() const { return currentMode; }
  String SSID() const { return connectedSsid; }
  void persistent(bool enabled) { persistentSettings = enabled; }
  void setAutoReconnect(bool enabled) { autoReconnect = enabled; }
  void disconnect() { connection = 0; }
  void mode(WiFiMode mode) { currentMode = mode; }
  void begin(const char* network, const char* passphrase) {
    ssid = network;
    password = passphrase ? passphrase : "";
    ++attempts;
  }
  bool softAP(const char*) { return true; }
  void reconnect() { ++reconnects; }
  int localIP() const { return 0; }
  int softAPIP() const { return 0; }
};
extern HostWiFi WiFi;
