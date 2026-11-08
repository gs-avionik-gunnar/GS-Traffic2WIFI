#include "BootAnim.h"

#include <math.h>

namespace bootanim {

namespace {

const int kCx = 120, kCy = 120;

// Colour from 8-bit channels scaled by f (0..1).
uint16_t shade(int r, int g, int b, float f) {
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;
  return gfx::rgb((uint8_t)(r * f), (uint8_t)(g * f), (uint8_t)(b * f));
}

float smooth(float x) {  // ease in/out, 0..1
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return 1.0f;
  return x * x * (3.0f - 2.0f * x);
}

float progress(uint32_t t, uint32_t start, uint32_t len) {
  if (t <= start) return 0.0f;
  if (t >= start + len) return 1.0f;
  return (float)(t - start) / (float)len;
}

void polar(float r, float deg, int& x, int& y) {  // 0 = up, clockwise
  float a = deg * 0.01745329f;
  x = kCx + (int)lroundf(r * sinf(a));
  y = kCy - (int)lroundf(r * cosf(a));
}

}  // namespace

void draw(gfx::Canvas& c, uint32_t t, const char* title, const char* product, const char* info) {
  // green of the template palette (40, 220, 80)
  const int gr = 40, gg = 220, gb = 80;

  // 1) scale ring builds up clockwise from the top (0 - 700 ms)
  const float build = smooth(progress(t, 0, 700));
  const int shown = (int)(72.0f * build);  // number of ticks (5 degree steps)
  if (build > 0.0f) c.arc(kCx, kCy, 118, 2, 0, (int)(360.0f * build), shade(gr, gg, gb, 0.35f));
  int x0, y0, x1, y1;
  for (int i = 0; i < shown; ++i) {
    const bool major = i % 6 == 0;
    polar(117, 5.0f * i, x0, y0);
    polar(major ? 104 : 110, 5.0f * i, x1, y1);
    c.line(x0, y0, x1, y1, major ? 3 : 1, shade(gr, gg, gb, major ? 1.0f : 0.6f));
  }

  // 2) radar beam: one revolution (500 - 1700 ms) with a fading trail, the trail dies out afterwards
  if (t >= 500 && t < 2100) {
    const float sweep = progress(t, 500, 1200);
    const float ang = 360.0f * sweep;
    const float fade = 1.0f - smooth(progress(t, 1700, 400));
    const int kTrail = 36;  // 36 lines, 2 degrees apart = 72 degrees of trail
    for (int k = kTrail - 1; k >= 0; --k) {
      float f = 1.0f - (float)k / (float)kTrail;
      f = f * f * fade;
      if (f < 0.03f) continue;
      polar(88, ang - 2.0f * k, x0, y0);
      polar(102, ang - 2.0f * k, x1, y1);
      c.line(x0, y0, x1, y1, 3, shade(gr, gg, gb, f * 0.75f));
    }
    if (fade > 0.0f && t < 1700 + 60) {  // bright leading edge while the beam is running
      polar(84, ang, x0, y0);
      polar(106, ang, x1, y1);
      c.line(x0, y0, x1, y1, 3, shade(gr, gg, gb, 1.0f));
    }
  }

  // 3) texts fade in one after the other, at the template positions
  const float f1 = smooth(progress(t, 800, 350));
  const float f2 = smooth(progress(t, 1100, 350));
  const float f3 = smooth(progress(t, 1400, 350));
  if (f1 > 0.0f) c.text(kFontMedium, 120, 80, gfx::Center, shade(gr, gg, gb, f1), title);
  if (f2 > 0.0f) c.text(kFontSmall, 120, 114, gfx::Center, shade(255, 255, 255, f2), product);
  if (f3 > 0.0f) c.text(kFontSmall, 120, 138, gfx::Center, shade(110, 110, 110, f3), info);
}

}  // namespace bootanim
