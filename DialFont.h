// Anti-aliased bitmap fonts for the round display (generated, see tools/genfont.py).
#pragma once
#include <stdint.h>

struct DialFont {
  uint8_t height;         // line height in pixels
  uint8_t ascent;         // baseline position from the top
  uint8_t first;          // first character (32)
  const uint8_t* width;   // advance/cell width per character
  const uint32_t* offset; // byte offset of each glyph in data
  const uint8_t* data;    // rows of 2 bpp coverage (4 pixels per byte, MSB first), (width+3)/4 bytes per row; width 0 = glyph not included
};

extern const DialFont kFontSmall;   // ~17 px
extern const DialFont kFontMedium;  // ~25 px
extern const DialFont kFontLarge;   // ~44 px
