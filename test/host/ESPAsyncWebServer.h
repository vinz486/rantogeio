#pragma once
#include "Arduino.h"
#include <functional>
#include <map>
#include <utility>

enum Method { HTTP_GET, HTTP_POST };

class AsyncWebServerRequest {
public:
  struct Parameter {
    String data;
    const String& value() const { return data; }
  };
  struct Client {
    void setRxTimeout(int seconds) { timeout = seconds; }
    int timeout = 0;
  };
  explicit AsyncWebServerRequest(const char* size = "") : _size{size} {}
  bool hasParam(const char* name) const { return String(name) == "size" && !_size.data.empty(); }
  Parameter* getParam(const char*) { return &_size; }
  void send(int code, const char*, const char* message) { status = code; body = message; }
  void send_P(int code, const char* type, const char* message) { send(code, type, message); }
  void onDisconnect(std::function<void()> callback) { _disconnect = std::move(callback); }
  void disconnect() { if (_disconnect) _disconnect(); }
  Client* client() { return &_client; }

  int status = 0;
  String body;

private:
  Parameter _size;
  Client _client;
  std::function<void()> _disconnect;
};

class AsyncWebServer {
public:
  using Handler = std::function<void(AsyncWebServerRequest*)>;
  using Upload = std::function<void(AsyncWebServerRequest*, String, size_t, uint8_t*, size_t, bool)>;
  void on(const char* path, Method method, Handler handler, Upload upload = {}) {
    routes[{path, method}] = {std::move(handler), std::move(upload)};
  }
  void dispatch(const char* path, Method method, AsyncWebServerRequest& request) {
    routes.at({path, method}).handler(&request);
  }
  void chunk(AsyncWebServerRequest& request, size_t index, uint8_t* data, size_t len, bool final) {
    routes.at({"/update", HTTP_POST}).upload(&request, "firmware.bin", index, data, len, final);
  }
private:
  struct Route { Handler handler; Upload upload; };
  std::map<std::pair<std::string, Method>, Route> routes;
};
