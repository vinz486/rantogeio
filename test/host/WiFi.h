#pragma once
constexpr int WL_CONNECTED = 3;
class WiFiClient {};
struct HostWiFi { int status() { return WL_CONNECTED; } };
extern HostWiFi WiFi;
