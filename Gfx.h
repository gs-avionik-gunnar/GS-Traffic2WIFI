// Minimal band-based 2D renderer for the 240x240 round display (pure C++, host-testable).
// The frame is rendered in horizontal bands into a small buffer; the caller redraws the whole
// page once per band and flushes the band to the panel. Result: no flicker, ~14 KB RAM.
// Pixels are RGB565 already byte-swapped for the SPI wire (high byte first in memory).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "DialFont.h"

namespace gfx {

constexpr uint16_t swap16c(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return swap16c((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}
constexpr uint16_t kBlack = rgb(0, 0, 0);
constexpr uint16_t kWhite = rgb(255, 255, 255);
constexpr uint16_t kGreen = rgb(40, 220, 80);
constexpr uint16_t kRed = rgb(255, 50, 40);
constexpr uint16_t kOrange = rgb(255, 150, 20);
constexpr uint16_t kYellow = rgb(255, 220, 30);
constexpr uint16_t kCyan = rgb(60, 200, 255);
constexpr uint16_t kGrey = rgb(110, 110, 110);
constexpr uint16_t kDark = rgb(40, 40, 40);

enum Align { Left, Center, Right };

class Canvas {
 public:
  static constexpr int kW = 240, kH = 240;
  // Select the band [y0, y0+h) that is rendered into buf (kW*h pixels).
  void setBand(uint16_t* buf, int y0, int h) { buf_ = buf; y0_ = y0; h_ = h; }
  void clear(uint16_t c);
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void hline(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void fillCircle(int cx, int cy, int r, uint16_t c);
  void ring(int cx, int cy, int r, int thick, uint16_t c);          // circle outline
  void arc(int cx, int cy, int r, int thick, int deg0, int deg1, uint16_t c);  // clockwise from 12 o'clock
  void line(int x0, int y0, int x1, int y1, int thick, uint16_t c);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);
  void roundRect(int x, int y, int w, int h, uint16_t c, bool fill, uint16_t bg = kBlack);  // fully rounded ends
  // Arrow-like marker pointing to angle deg (0 = up, clockwise), centre cx,cy, size s.
  void marker(int cx, int cy, int s, float deg, uint16_t c);
  // Draws text; y is the top of the line. Returns the width in pixels.
  int text(const DialFont& f, int x, int y, Align a, uint16_t c, const char* s);
  static int textWidth(const DialFont& f, const char* s);

 private:
  void put(int x, int y, uint16_t c) {
    int yy = y - y0_;
    if ((unsigned)x < (unsigned)kW && (unsigned)yy < (unsigned)h_) buf_[yy * kW + x] = c;
  }
  uint16_t* buf_ = nullptr;
  int y0_ = 0, h_ = 0;
};

}  // namespace gfx
