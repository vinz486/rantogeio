#pragma once
#include <cstddef>
struct HostQueue;
using QueueHandle_t = HostQueue*;
QueueHandle_t xQueueCreate(unsigned int, size_t);
int xQueueSend(QueueHandle_t, const void*, unsigned int);
int xQueueReceive(QueueHandle_t, void*, unsigned int);
