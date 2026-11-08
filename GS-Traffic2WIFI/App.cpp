#include "App.h"

Settings g_settings;
NavState g_nav;
LiveStats g_stats;
SemaphoreHandle_t g_navMutex = nullptr;
const char* g_hwName = "1.x / selfmade";
bool g_hw2 = false;
bool g_hw3 = false;
bool g_apFallback = false;

void appInit() {
  if (!g_navMutex) g_navMutex = xSemaphoreCreateMutex();
}

bool navSnapshot(NavState& out, uint32_t timeoutMs) {
  if (!g_navMutex || xSemaphoreTake(g_navMutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) return false;
  out = g_nav;
  xSemaphoreGive(g_navMutex);
  return true;
}
