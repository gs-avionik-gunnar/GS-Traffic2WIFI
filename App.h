// Shared runtime state. Threading model:
//   * loop() task  : owns g_nav (serial/TCP/GDL90/LED) and writes it under g_navMutex
//   * web task     : reads g_nav only through navSnapshot(); never writes it
//   * g_settings   : read-only after boot (the WebUI edits a copy, saves, reboots)
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "Settings.h"
#include "State.h"

struct LiveStats {
  volatile uint32_t tcpClients;
  volatile uint32_t gdl90Frames;
  volatile uint32_t udpErrors;
  volatile uint32_t rxBytes;
  volatile uint32_t simActive;
  volatile uint8_t led;  // 0 off, 1 green, 2 red, 3 orange
};

extern Settings g_settings;
extern NavState g_nav;
extern LiveStats g_stats;
extern SemaphoreHandle_t g_navMutex;
extern const char* g_hwName;
extern bool g_hw3;         // hardware 3.0 (M5 Dial, ESP32-S3)
extern bool g_hw2;         // hardware 2.0 detected (different serial pins)
extern bool g_apFallback;  // client mode failed at boot, running as AP so the config stays reachable

#define LOG(level, ...)                                  \
  do {                                                   \
    if (g_settings.debug >= (level)) Serial.printf(__VA_ARGS__); \
  } while (0)

void appInit();
// Copies the navigation state under the mutex. Returns false on lock timeout.
bool navSnapshot(NavState& out, uint32_t timeoutMs);
