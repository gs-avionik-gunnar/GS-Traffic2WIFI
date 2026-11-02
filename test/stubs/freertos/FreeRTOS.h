#pragma once
#include <stdint.h>
typedef void* TaskHandle_t;
typedef int BaseType_t;
#define pdTRUE 1
BaseType_t xTaskCreatePinnedToCore(void (*)(void*), const char*, uint32_t, void*, unsigned, TaskHandle_t*, int);
void vTaskDelay(uint32_t);
