#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Preferences.h>
#include <atomic>

class WifiManager {
public:
  void begin();
  void loop();
  bool connecting() const { return _connecting; }
  void start_mdns(const char* host);
  static bool valid_credentials(const String& ssid, const String& password);
  bool set_credentials(const String& ssid, const String& password);
  String saved_ssid() const;

private:
  struct Credentials {
    char ssid[33];
    char password[65];
  };
  Credentials read_credentials() const;
  void start_ap();
  std::atomic<bool> _reload{false};
  unsigned long _reload_at = 0;
  unsigned long _connect_at = 0;
  unsigned long _last_wifi_check = 0;
  bool _reload_scheduled = false;
  bool _connecting = false;
  bool _try_reconnect = false;
  bool _stationLost = false;
  unsigned long _stationLostAt = 0;
  String _connectingSsid;
};

#endif
