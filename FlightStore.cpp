#include "FlightStore.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "App.h"

namespace flights {

namespace {

using flightlog::Flight;

const char* kNs = "gs4log";
const uint8_t kVersion = 1;

class PrefStore : public flightlog::Store {
 public:
  void saveCurrent(const Flight& f) override {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    p.putBytes("cur", &f, sizeof f);
    p.end();
  }
  void clearCurrent() override {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    p.remove("cur");
    p.end();
  }
  void saveAll(const Flight* list, int n) override {
    Preferences p;
    if (!p.begin(kNs, false)) return;
    static uint8_t buf[2 + sizeof(Flight) * flightlog::kMaxFlights];
    buf[0] = kVersion;
    buf[1] = (uint8_t)n;
    memcpy(buf + 2, list, sizeof(Flight) * (size_t)n);
    p.putBytes("log", buf, 2 + sizeof(Flight) * (size_t)n);
    p.end();
  }
};

PrefStore s_store;
flightlog::Log s_log;
SemaphoreHandle_t s_mutex = nullptr;

struct Lock {
  Lock() { if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY); }
  ~Lock() { if (s_mutex) xSemaphoreGive(s_mutex); }
};

}  // namespace

void begin() {
  s_mutex = xSemaphoreCreateMutex();
  Flight list[flightlog::kMaxFlights];
  int n = 0;
  Flight cur;
  bool haveCur = false;
  Preferences p;
  if (p.begin(kNs, false)) {
    static uint8_t buf[2 + sizeof(Flight) * flightlog::kMaxFlights];
    size_t got = p.getBytes("log", buf, sizeof buf);
    if (got >= 2 && buf[0] == kVersion) {
      int cnt = buf[1];
      if (cnt > flightlog::kMaxFlights) cnt = flightlog::kMaxFlights;
      if (got >= 2 + sizeof(Flight) * (size_t)cnt) {
        memcpy(list, buf + 2, sizeof(Flight) * (size_t)cnt);
        n = cnt;
      }
    }
    if (p.getBytes("cur", &cur, sizeof cur) == sizeof cur) haveCur = true;
    p.end();
  }
  Lock l;
  s_log.init(&s_store, list, n, haveCur ? &cur : nullptr);
}

void tick(const NavState& nav, uint32_t now, bool allow) {
  Lock l;
  s_log.tick(nav, now, allow && g_settings.flightLog, g_settings.flightKmh);
}

int snapshot(Flight* out, int max, bool* active) {
  Lock l;
  int n = s_log.count() < max ? s_log.count() : max;
  for (int i = 0; i < n; ++i) out[i] = s_log.at(i);
  if (active) *active = s_log.active();
  return n;
}

bool removeByStart(uint32_t start) {
  Lock l;
  return s_log.removeByStart(start);
}

void clearAll() {
  Lock l;
  s_log.clear();
}

}  // namespace flights
