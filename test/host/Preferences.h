#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <cstring>

namespace storage {
extern bool failWrites;
extern unsigned int writes;
extern std::map<std::string, std::vector<uint8_t>> data;
}
class Preferences {
  std::string prefix;
public:
  bool begin(const char* name, bool = false) { prefix = std::string(name) + "/"; return true; }
  void end() {}
  size_t getBytesLength(const char* key) { return storage::data[prefix + key].size(); }
  size_t getBytes(const char* key, void* out, size_t size) {
    const auto& bytes = storage::data[prefix + key];
    if (size < bytes.size()) return 0;
    if (!bytes.empty()) memcpy(out, bytes.data(), bytes.size());
    return bytes.size();
  }
  size_t putBytes(const char* key, const void* value, size_t size) {
    if (storage::failWrites) return 0;
    const auto* bytes = static_cast<const uint8_t*>(value);
    storage::data[prefix + key] = std::vector<uint8_t>(bytes, bytes + size);
    ++storage::writes;
    return size;
  }
  int32_t getInt(const char* key, int32_t fallback) {
    int32_t value = fallback;
    if (getBytesLength(key) == sizeof(value)) getBytes(key, &value, sizeof(value));
    return value;
  }
  size_t putInt(const char* key, int32_t value) { return putBytes(key, &value, sizeof(value)); }
  bool getBool(const char* key, bool fallback) { return getInt(key, fallback) != 0; }
  size_t putBool(const char* key, bool value) { return putInt(key, value); }
  String getString(const char* key, const char* fallback) {
    const auto& bytes = storage::data[prefix + key];
    return bytes.empty() ? fallback : reinterpret_cast<const char*>(bytes.data());
  }
  size_t putString(const char* key, const String& value) { return putString(key, value.c_str()); }
  size_t putString(const char* key, const char* value) { return putBytes(key, value, strlen(value) + 1); }
};
