// WiFi regulatory regions selectable in the WebUI. Pure C++ (no Arduino dependencies).
#pragma once
#include <stdint.h>

struct Region {
  const char* cc;    // ISO 3166 alpha-2 code ("01" = world safe)
  const char* name;
  uint8_t firstCh;   // allowed 2.4 GHz channels
  uint8_t lastCh;
};

int regionCount();
const Region& regionAt(int index);
// Case-insensitive lookup; returns nullptr for unknown codes.
const Region* findRegion(const char* cc);
// Default region and the channel to use when the configured one is not allowed there.
const Region& defaultRegion();
