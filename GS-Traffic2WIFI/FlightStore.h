// Flight log glue: NVS persistence (own namespace "gs4log", survives a settings reset) and thread-safe access.
#pragma once
#include <stdint.h>

#include "FlightLog.h"
#include "State.h"

namespace flights {

void begin();                                    // loads the log, call once after settingsLoad()
void tick(const NavState& nav, uint32_t now, bool allow);  // loop task
// Copies up to max flights (newest first). Returns the number copied; *active tells whether a flight is in progress.
int snapshot(flightlog::Flight* out, int max, bool* active);
bool removeByStart(uint32_t start);
void clearAll();

}  // namespace flights
