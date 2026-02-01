#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Preferences.h>

class WifiManager {
public:
  void begin();
  void loop();
  void start_mdns(const char* host);
  void set_credentials(String ssid, String password);

private:
  Preferences _preferences;
  unsigned long _last_wifi_check = 0;
  bool _try_reconnect = false;
};

#endif