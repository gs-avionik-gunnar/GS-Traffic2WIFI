// Neutral GS Avionik boot logo: a scale ring builds up, a radar beam sweeps once around it while the
// three text lines fade in. Independent of the product: copy BootAnim.h/.cpp together with Gfx.* and DialFont.*
// into any GS Dial project and pass the product name and the version line.
#pragma once
#include <stdint.h>

#include "Gfx.h"

namespace bootanim {

constexpr uint32_t kDurationMs = 2500;

// Draws the frame at time t (ms since the start) into the current band of c.
// title: "GS Avionik" (top line), product: product name, info: version line, e.g. "v2.0".
void draw(gfx::Canvas& c, uint32_t t, const char* title, const char* product, const char* info);

}  // namespace bootanim
