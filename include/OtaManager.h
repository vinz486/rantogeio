#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <ESPAsyncWebServer.h>
#include <atomic>

class ClockManager;

class OtaManager {
public:
  explicit OtaManager(ClockManager& clock) : _clock(clock) {}
  void begin(AsyncWebServer& server, void (*restart)(), bool (*canPrepare)());
  void expire_preparation();
  bool busy() const { return _phase.load() != Phase::IDLE; }

private:
  enum class Phase { IDLE, PREPARING, UPLOADING, REBOOTING, RELEASING };
  ClockManager& _clock;
  std::atomic<Phase> _phase{Phase::IDLE};
  std::atomic<unsigned long> _preparedAt{0};
  AsyncWebServerRequest* _owner = nullptr;
  bool _uploadComplete = false;
  bool _uploadFailed = false;
  size_t _written = 0;

  void release();
  void receive_chunk(AsyncWebServerRequest* request, size_t index, uint8_t* data, size_t len, bool final);
};
#endif
