#include "WifiManager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

WifiManager::Credentials WifiManager::read_credentials() const {
  Credentials credentials{};
  Preferences prefs;
  if (prefs.begin("wifi", true)) {
    if (prefs.getBytesLength("credentials") != sizeof(credentials) ||
        prefs.getBytes("credentials", &credentials, sizeof(credentials)) != sizeof(credentials)) {
      prefs.getString("ssid", "").toCharArray(credentials.ssid, sizeof(credentials.ssid));
      prefs.getString("password", "").toCharArray(credentials.password, sizeof(credentials.password));
    }
    prefs.end();
  }
  credentials.ssid[sizeof(credentials.ssid) - 1] = '\0';
  credentials.password[sizeof(credentials.password) - 1] = '\0';
  return credentials;
}

void WifiManager::begin() {
  const bool keep_ap = (WiFi.getMode() & WIFI_AP) != 0;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  _try_reconnect = false;
  _connecting = false;
  _stationLost = false;

  const auto credentials = read_credentials();
  if (!credentials.ssid[0]) {
    start_ap();
    return;
  }

  WiFi.mode(keep_ap ? WIFI_AP_STA : WIFI_STA);
  _connectingSsid = credentials.ssid;
  WiFi.begin(credentials.ssid, credentials.password[0] ? credentials.password : nullptr);
  _connect_at = millis();
  _connecting = true;
  Serial.println("Connecting to Wi-Fi");
}

void WifiManager::start_ap() {
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP("clock")) {
    Serial.print("Setup AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Setup AP failed");
  }
}

void WifiManager::loop() {
  const unsigned long now = millis();
  if (_reload.exchange(false)) {
    _reload_at = now;
    _reload_scheduled = true;
  }
  if (_reload_scheduled && now - _reload_at >= 1000) {
    _reload_scheduled = false;
    begin();
    return;
  }

  if (_connecting) {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == _connectingSsid) {
      WiFi.mode(WIFI_STA);
      WiFi.setAutoReconnect(true);
      _connecting = false;
      _try_reconnect = true;
      _last_wifi_check = now;
      Serial.print("Wi-Fi IP address: ");
      Serial.println(WiFi.localIP());
    } else if (now - _connect_at >= 10000) {
      _connecting = false;
      start_ap();
      _try_reconnect = true;
      _last_wifi_check = now;
    }
  } else if (_try_reconnect) {
    if (WiFi.status() == WL_CONNECTED) {
      _stationLost = false;
      return;
    }
    if (WiFi.getMode() == WIFI_AP) {
      if (now - _last_wifi_check >= 30000) begin();
      return;
    }
    if (!_stationLost) {
      _stationLost = true;
      _stationLostAt = now;
      _last_wifi_check = now;
    } else if (now - _stationLostAt >= 60000) {
      start_ap();
      begin();
    } else if (now - _last_wifi_check >= 30000) {
      _last_wifi_check = now;
      WiFi.reconnect();
    }
  }
}

void WifiManager::start_mdns(const char* host) {
  Serial.println(MDNS.begin(host) ? "mDNS responder started" : "Error setting up mDNS responder");
}

bool WifiManager::valid_credentials(const String& ssid, const String& password) {
  if (ssid.isEmpty() || ssid.length() > 32) return false;
  const size_t length = password.length();
  if (!length || (length >= 8 && length <= 63)) return true;
  if (length != 64) return false;
  for (size_t i = 0; i < length; ++i) {
    const char digit = password[i];
    if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') ||
          (digit >= 'A' && digit <= 'F'))) return false;
  }
  return true;
}

bool WifiManager::set_credentials(const String& ssid, const String& password) {
  if (!valid_credentials(ssid, password)) return false;
  Credentials credentials{};
  ssid.toCharArray(credentials.ssid, sizeof(credentials.ssid));
  password.toCharArray(credentials.password, sizeof(credentials.password));
  Preferences prefs;
  if (!prefs.begin("wifi", false)) return false;
  const bool saved = prefs.putBytes("credentials", &credentials, sizeof(credentials)) == sizeof(credentials);
  prefs.end();
  if (saved) _reload = true;
  return saved;
}

String WifiManager::saved_ssid() const {
  return String(read_credentials().ssid);
}
