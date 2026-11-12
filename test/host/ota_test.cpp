#include "ClockManager.h"
#include "OtaManager.h"
#include "Update.h"
#include <cassert>

HostUpdate Update;
namespace {
int reboots = 0;
bool canPrepare = true;
void restart() { ++reboots; }
bool available() { return canPrepare; }
}

void testOta() {
  ClockManager clock;
  clock.begin();
  AsyncWebServer server;
  OtaManager ota(clock);
  ota.begin(server, restart, available);
  const auto status = [&](const char* path, Method method) {
    AsyncWebServerRequest request;
    server.dispatch(path, method, request);
    return request.status;
  };

  assert(status("/update", HTTP_GET) == 200);
  canPrepare = false;
  assert(status("/update/prepare", HTTP_POST) == 409 && !ota.busy());
  canPrepare = true;
  assert(status("/update/prepare", HTTP_POST) == 202 && ota.busy());
  assert(status("/update/status", HTTP_GET) == 425);
  uint8_t bytes[] = {1, 2, 3, 4};
  AsyncWebServerRequest premature("4");
  server.chunk(premature, 0, bytes, 4, true);
  server.dispatch("/update", HTTP_POST, premature);
  assert(premature.status == 409 && Update.begins == 0);
  clock.tick();
  assert(status("/update/status", HTTP_GET) == 200);

  AsyncWebServerRequest disconnected("4");
  server.chunk(disconnected, 0, bytes, 2, false);
  assert(Update.running && disconnected.client()->timeout == 15);
  disconnected.disconnect();
  assert(!Update.running && Update.aborts == 1 && !ota.busy() && reboots == 0);

  assert(status("/update/prepare", HTTP_POST) == 202);
  assert(status("/update/status", HTTP_GET) == 425);
  clock.tick();
  assert(status("/update/status", HTTP_GET) == 200);
  AsyncWebServerRequest truncated("4");
  server.chunk(truncated, 0, bytes, 2, true);
  server.dispatch("/update", HTTP_POST, truncated);
  assert(truncated.status == 400 && !ota.busy() && reboots == 0);

  assert(status("/update/prepare", HTTP_POST) == 202);
  clock.tick();
  delay(30000);
  ota.expire_preparation();
  assert(!ota.busy() && !clock.maintenance_ready());

  assert(status("/update/prepare", HTTP_POST) == 202);
  clock.tick();
  AsyncWebServerRequest image("4");
  server.chunk(image, 0, bytes, 2, false);
  AsyncWebServerRequest competing("4");
  server.chunk(competing, 0, bytes, 4, true);
  server.dispatch("/update", HTTP_POST, competing);
  assert(competing.status == 409 && Update.written == 2);
  server.chunk(image, 2, bytes + 2, 2, true);
  server.dispatch("/update", HTTP_POST, image);
  assert(image.status == 200 && Update.installs == 1 && reboots == 1);
  assert(ota.busy() && clock.maintenance_ready());
  image.disconnect();
  assert(reboots == 1);
}
