#pragma once
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
inline esp_err_t esp_netif_tcpip_exec(esp_err_t (*fn)(void*), void* argument) {
  return fn(argument);
}
