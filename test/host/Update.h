#pragma once
#include <stdint.h>
#include <stddef.h>

constexpr int U_FLASH = 0;
struct HostUpdate {
  bool begin(size_t size, int) {
    if (running || !size) return false;
    running = true;
    expected = size;
    written = 0;
    ++begins;
    return true;
  }
  size_t write(const uint8_t*, size_t size) {
    if (!running || size > expected - written) return 0;
    written += size;
    return size;
  }
  bool end() {
    if (!running || written != expected) return false;
    running = false;
    ++installs;
    return true;
  }
  void abort() { running = false; ++aborts; }
  bool isRunning() const { return running; }

  bool running = false;
  size_t expected = 0, written = 0;
  int begins = 0, installs = 0, aborts = 0;
};
extern HostUpdate Update;
