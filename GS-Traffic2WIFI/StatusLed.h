#pragma once
#include <stdint.h>

namespace statusled {
void begin();
void bootSequence();      // blocking, ~1.8 s, only call from setup()
void set(uint8_t state);  // 0 off, 1 green, 2 red, 3 orange (both)
}  // namespace statusled
