// Hardware v3 glue: GC9A01 display, wheel, button, buzzer, Trafficmod LED ring.
// No external libraries. Everything runs in its own FreeRTOS task; on boards that are not an
// ESP32-S3 this module compiles to nothing.
#pragma once

namespace dialhw {
bool supported();  // true when compiled for an ESP32-S3
void begin();      // starts the display task (call after settingsLoad())
}  // namespace dialhw
