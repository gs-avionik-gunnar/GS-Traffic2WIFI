#include "Gfx.h"

#include <math.h>

namespace gfx {

static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

// Blend fg over bg with 4-bit coverage (both in wire order).
static uint16_t blend(uint16_t bgW, uint16_t fgW, int a4) {
  uint16_t bg = swap16(bgW), fg = swap16(fgW);
  int a = a4 + (a4 >> 3);  // 0..16
  int r = (((bg >> 11) & 31) * (16 - a) + ((fg >> 11) & 31) * a) >> 4;
  int g = (((bg >> 5) & 63) * (16 - a) + ((fg >> 5) & 63) * a) >> 4;
  int b = ((bg & 31) * (16 - a) + (fg & 31) * a) >> 4;
  return swap16((uint16_t)((r << 11) | (g << 5) | b));
}

void Canvas::clear(uint16_t c) {
  for (int i = 0; i < kW * h_; ++i) buf_[i] = c;
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t c) {
  int x0 = x < 0 ? 0 : x, x1 = x + w > kW ? kW : x + w;
  int ya = y < y0_ ? y0_ : y, yb = y + h > y0_ + h_ ? y0_ + h_ : y + h;
  for (int yy = ya; yy < yb; ++yy) {
    uint16_t* p = buf_ + (yy - y0_) * kW;
    for (int xx = x0; xx < x1; ++xx) p[xx] = c;
  }
}

void Canvas::fillCircle(int cx, int cy, int r, uint16_t c) {
  int ya = cy - r < y0_ ? y0_ : cy - r, yb = cy + r + 1 > y0_ + h_ ? y0_ + h_ : cy + r + 1;
  for (int y = ya; y < yb; ++y) {
    int dy = y - cy;
    int dx = (int)sqrtf((float)(r * r - dy * dy) + 0.5f);
    fillRect(cx - dx, y, 2 * dx + 1, 1, c);
  }
}

void Canvas::ring(int cx, int cy, int r, int thick, uint16_t c) {
  int ro = r, ri = r - thick;
  int ya = cy - ro < y0_ ? y0_ : cy - ro, yb = cy + ro + 1 > y0_ + h_ ? y0_ + h_ : cy + ro + 1;
  for (int y = ya; y < yb; ++y) {
    int dy = y - cy;
    int xo = (int)sqrtf((float)(ro * ro - dy * dy) + 0.5f);
    if (abs(dy) >= ri) {
      fillRect(cx - xo, y, 2 * xo + 1, 1, c);
    } else {
      int xi = (int)sqrtf((float)(ri * ri - dy * dy) + 0.5f);
      fillRect(cx - xo, y, xo - xi + 1, 1, c);
      fillRect(cx + xi, y, xo - xi + 1, 1, c);
    }
  }
}

void Canvas::arc(int cx, int cy, int r, int thick, int deg0, int deg1, uint16_t c) {
  // Per-pixel test inside the ring's bounding rows; used only for small gauges.
  int ro = r, ri = r - thick;
  int ya = cy - ro < y0_ ? y0_ : cy - ro, yb = cy + ro + 1 > y0_ + h_ ? y0_ + h_ : cy + ro + 1;
  for (int y = ya; y < yb; ++y) {
    for (int x = cx - ro; x <= cx + ro; ++x) {
      int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
      if (d2 > ro * ro || d2 < ri * ri) continue;
      float a = atan2f((float)dx, (float)-dy) * 57.29578f;
      if (a < 0) a += 360.0f;
      if (a >= deg0 && a <= deg1) put(x, y, c);
    }
  }
}

void Canvas::line(int x0, int y0, int x1, int y1, int thick, uint16_t c) {
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int steps = dx > dy ? dx : dy;
  if (steps == 0) steps = 1;
  int r = thick / 2;
  for (int i = 0; i <= steps; ++i) {
    int x = x0 + (x1 - x0) * i / steps, y = y0 + (y1 - y0) * i / steps;
    if (y + r < y0_ || y - r >= y0_ + h_) continue;
    fillRect(x - r, y - r, thick, thick, c);
  }
}

void Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  int ymin = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int ymax = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  if (ymin < y0_) ymin = y0_;
  if (ymax >= y0_ + h_) ymax = y0_ + h_ - 1;
  const int xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  for (int y = ymin; y <= ymax; ++y) {
    int lo = 100000, hi = -100000;
    for (int e = 0; e < 3; ++e) {
      int ax = xs[e], ay = ys[e], bx = xs[(e + 1) % 3], by = ys[(e + 1) % 3];
      if (ay == by) {
        if (ay == y) { lo = ax < lo ? ax : lo; hi = ax > hi ? ax : hi; lo = bx < lo ? bx : lo; hi = bx > hi ? bx : hi; }
        continue;
      }
      if ((y < ay && y < by) || (y > ay && y > by)) continue;
      int x = ax + (bx - ax) * (y - ay) / (by - ay);
      lo = x < lo ? x : lo;
      hi = x > hi ? x : hi;
    }
    if (hi >= lo) fillRect(lo, y, hi - lo + 1, 1, c);
  }
}

void Canvas::roundRect(int x, int y, int w, int h, uint16_t c, bool fill, uint16_t bg) {
  auto solid = [&](int xx, int yy, int ww, int hh, uint16_t col) {
    int r = hh / 2;
    fillRect(xx + r, yy, ww - 2 * r, hh, col);
    fillCircle(xx + r, yy + r, r, col);
    fillCircle(xx + ww - r - 1, yy + r, r, col);
  };
  solid(x, y, w, h, c);
  if (!fill && w > 4 && h > 4) solid(x + 2, y + 2, w - 4, h - 4, bg);
}

void Canvas::marker(int cx, int cy, int s, float deg, uint16_t c) {
  float a = deg * 0.01745329f, sa = sinf(a), ca = cosf(a);
  // Arrow head: tip forward, two rear corners, notch in the back.
  const float px[3] = {0.0f, 0.7f, -0.7f}, py[3] = {-1.0f, 0.8f, 0.8f};
  int X[3], Y[3];
  for (int i = 0; i < 3; ++i) {
    X[i] = cx + (int)lroundf((px[i] * ca - py[i] * sa) * s);
    Y[i] = cy + (int)lroundf((px[i] * sa + py[i] * ca) * s);
  }
  fillTriangle(X[0], Y[0], X[1], Y[1], X[2], Y[2], c);
}

int Canvas::textWidth(const DialFont& f, const char* s) {
  int w = 0;
  for (; *s; ++s) {
    unsigned ch = (unsigned char)*s;
    if (ch < f.first || ch > 126) ch = '?';
    w += f.width[ch - f.first];
  }
  return w;
}

int Canvas::text(const DialFont& f, int x, int y, Align a, uint16_t col, const char* s) {
  int tw = textWidth(f, s);
  if (a == Center) x -= tw / 2;
  else if (a == Right) x -= tw;
  if (y + f.height <= y0_ || y >= y0_ + h_) return tw;
  int px = x;
  for (; *s; ++s) {
    unsigned ch = (unsigned char)*s;
    if (ch < f.first || ch > 126) ch = '?';
    int gi = (int)ch - f.first, w = f.width[gi];
    const uint8_t* d = f.data + f.offset[gi];
    int stride = (w + 3) / 4;
    int ra = y < y0_ ? y0_ - y : 0, rb = (y + f.height > y0_ + h_) ? y0_ + h_ - y : f.height;
    for (int row = ra; row < rb; ++row) {
      const uint8_t* rp = d + row * stride;
      int yy = y + row - y0_;
      for (int col2 = 0; col2 < w; ++col2) {
        int xx = px + col2;
        if ((unsigned)xx >= (unsigned)kW) continue;
        int v = (rp[col2 >> 2] >> (6 - 2 * (col2 & 3))) & 3;
        if (!v) continue;
        uint16_t* dst = &buf_[yy * kW + xx];
        *dst = v == 3 ? col : blend(*dst, col, v * 5);
      }
    }
    px += w;
  }
  return tw;
}

}  // namespace gfx
