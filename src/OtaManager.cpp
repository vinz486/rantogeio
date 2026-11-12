#include "OtaManager.h"
#include "ClockManager.h"
#include "NumberParsing.h"
#include <Update.h>

namespace {
constexpr unsigned long PREPARATION_TIMEOUT_MS = 30000;
const char OTA_HTML[] = R"ota(
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rantoge · Update firmware</title>
<style>
body { background:#f5f4f8; color:#242235; font:1rem/1.5 system-ui,sans-serif; margin:0; }
main { max-width:32rem; margin:3rem auto; padding:0 1rem; }
form { background:white; border:1px solid #dedce6; border-radius:.875rem; padding:1.25rem; }
label,input,button { display:block; margin-bottom:.75rem; font:inherit; }
input { max-width:100%; }
button { color:white; background:#4e3d90; border:0; border-radius:.5rem; padding:.65rem .875rem; }
button:disabled { opacity:.7; }
a { color:#4e3d90; }
:focus-visible { outline:3px solid #765abe; outline-offset:3px; }
</style>
<main>
<a href="/">Back to clock settings</a>
<h1>Update firmware</h1>
<form>
<label for="firmware">Firmware file (.bin)</label>
<input id="firmware" type="file" accept=".bin,application/octet-stream" required>
<button type="submit">Install update</button>
<p role="status"></p>
</form>
</main>
<script>
const form = document.querySelector('form');
form.addEventListener('submit', async event => {
  event.preventDefault();
  const button = form.querySelector('button');
  const status = form.querySelector('[role=status]');
  const file = document.getElementById('firmware').files[0];
  button.disabled = true;
  let installed = false;
  try {
    status.textContent = 'Waiting for the clock to finish its current movement…';
    const prepare = await fetch('/update/prepare', {method:'POST'});
    if (!prepare.ok) throw new Error(await prepare.text());
    const deadline = Date.now() + 25000;
    for (;;) {
      const ready = await fetch('/update/status', {cache:'no-store'});
      if (ready.status === 200) break;
      if (ready.status !== 202 && ready.status !== 425) throw new Error(await ready.text());
      if (Date.now() >= deadline) throw new Error('The clock did not become ready in time.');
      await new Promise(resolve => setTimeout(resolve, 300));
    }
    status.textContent = 'Uploading firmware…';
    const body = new FormData();
    body.append('firmware', file);
    const result = await fetch('/update?size=' + file.size, {method:'POST', body});
    if (!result.ok) throw new Error(await result.text());
    installed = true;
    status.textContent = await result.text();
  } catch (error) {
    status.textContent = error.message;
  } finally {
    if (!installed) button.disabled = false;
  }
});
</script>
</html>
)ota";
}

void OtaManager::release() {
  if (Update.isRunning()) Update.abort();
  _owner = nullptr;
  _clock.end_maintenance();
  _phase.store(Phase::IDLE);
}

void OtaManager::expire_preparation() {
  if (_phase.load() != Phase::PREPARING || millis() - _preparedAt.load() < PREPARATION_TIMEOUT_MS) return;
  auto expected = Phase::PREPARING;
  if (_phase.compare_exchange_strong(expected, Phase::RELEASING)) {
    _clock.end_maintenance();
    _phase.store(Phase::IDLE);
  }
}

void OtaManager::receive_chunk(AsyncWebServerRequest* request, size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0 && !_owner) {
    auto expected = Phase::PREPARING;
    if (!_clock.maintenance_ready() || !_phase.compare_exchange_strong(expected, Phase::UPLOADING)) return;

    _owner = request;
    _uploadComplete = false;
    _written = 0;
    int32_t size = 0;
    _uploadFailed = !request->hasParam("size") ||
                    !parseInteger(request->getParam("size")->value().c_str(), size) ||
                    size <= 0 || !Update.begin(size, U_FLASH);
    request->client()->setRxTimeout(15);
    request->onDisconnect([this, request]() {
      if (_owner == request) release();
    });
  }
  if (_owner != request || _uploadFailed) return;
  if (index != _written || _uploadComplete || Update.write(data, len) != len) {
    _uploadFailed = true;
    return;
  }
  _written += len;
  _uploadComplete = final;
}

void OtaManager::begin(AsyncWebServer& server, void (*restart)(), bool (*canPrepare)()) {
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", OTA_HTML);
  });
  server.on("/update/prepare", HTTP_POST, [this, canPrepare](AsyncWebServerRequest* request) {
    if (!canPrepare()) {
      request->send(409, "text/plain", "The clock is restarting.");
      return;
    }
    const Phase phase = _phase.load();
    if (phase == Phase::IDLE) {
      _clock.request_maintenance();
      _preparedAt.store(millis());
      _phase.store(Phase::PREPARING);
    } else if (phase != Phase::PREPARING) {
      request->send(409, "text/plain", "A firmware update is already in progress.");
      return;
    }
    request->send(202, "text/plain", "Waiting for the motor to stop.");
  });
  server.on("/update/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (_phase.load() != Phase::PREPARING) {
      request->send(409, "text/plain", "Prepare the clock for the update again.");
    } else if (!_clock.maintenance_ready()) {
      request->send(425, "text/plain", "Waiting for the motor to stop.");
    } else {
      request->send(200, "text/plain", "Ready.");
    }
  });
  server.on("/update", HTTP_POST, [this, restart](AsyncWebServerRequest* request) {
    if (_owner != request) {
      request->send(409, "text/plain", "Prepare the clock and choose a firmware file before uploading.");
      return;
    }
    if (_uploadFailed || !_uploadComplete || !Update.end()) {
      release();
      request->send(400, "text/plain", "Firmware rejected. Check that the complete ESP32 firmware binary was selected.");
      return;
    }

    _owner = nullptr;
    _phase.store(Phase::REBOOTING);
    request->send(200, "text/plain", "Firmware installed. The clock is restarting. Confirm the displayed digits after reconnecting.");
    restart();
  }, [this](AsyncWebServerRequest* request, String, size_t index, uint8_t* data, size_t len, bool final) {
    receive_chunk(request, index, data, len, final);
  });
}
