// Test-data generator: emits a realistic NMEA set (GPRMC, GPGGA, GPGSA, PGRMZ,
// PFLAU, PFLAA) once per second. The sentences go through the same pipeline as
// real serial data, so GDL90, LEDs and the WebUI can be tested without a GPS.
// Pure C++ (no Arduino dependencies).
#pragma once
#include <stddef.h>
#include <stdint.h>

class Simulator {
 public:
  typedef void (*LineSink)(const char* line, void* ctx);
  Simulator() : started_(false), last_(0), t_(0) {}
  void tick(uint32_t nowMs, LineSink sink, void* ctx);

 private:
  void emitSet(LineSink sink, void* ctx);
  bool started_;
  uint32_t last_;
  uint32_t t_;  // simulated seconds
};
