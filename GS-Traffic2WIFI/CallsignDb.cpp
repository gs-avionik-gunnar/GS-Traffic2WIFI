#include "CallsignDb.h"

#include <stdio.h>
#include <string.h>

namespace csdb {

static ReadFn g_read = nullptr;
namespace {
struct Slot {
  uint32_t base, size, count;
  volatile bool open;
};
Slot g_slot[kSlots];
}  // namespace

void setReader(ReadFn fn) {
  g_read = fn;
  for (int i = 0; i < kSlots; ++i) g_slot[i].open = false;
}

void closeSlot(int i) {
  if (i >= 0 && i < kSlots) g_slot[i].open = false;
}

uint32_t count(int slot) {
  uint32_t n = 0;
  for (int i = 0; i < kSlots; ++i)
    if (g_slot[i].open && (slot < 0 || slot == i)) n += g_slot[i].count;
  return n;
}

bool openSlot(int i, uint32_t base, uint32_t size) {
  if (i < 0 || i >= kSlots) return false;
  Slot& sl = g_slot[i];
  sl.open = false;
  uint8_t h[kHeader];
  if (!g_read || size < kHeader || !g_read(base, h, sizeof h)) return false;
  if (memcmp(h, "GSDB", 4) != 0) return false;
  uint32_t n = (uint32_t)h[4] | ((uint32_t)h[5] << 8) | ((uint32_t)h[6] << 16) | ((uint32_t)h[7] << 24);
  if (n == 0 || n > (size - kHeader) / kRec) return false;
  sl.base = base;
  sl.size = size;
  sl.count = n;
  sl.open = true;
  return true;
}

static bool lookupSlot(const Slot& sl, uint32_t id, char* out) {
  uint32_t lo = 0, hi = sl.count;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo) / 2;
    uint8_t r[kRec];
    if (!g_read(sl.base + kHeader + mid * kRec, r, kRec)) return false;
    uint32_t v = (uint32_t)r[0] | ((uint32_t)r[1] << 8) | ((uint32_t)r[2] << 16) | ((uint32_t)r[3] << 24);
    if (v == id) {
      int k = 0;
      for (int i = 0; i < kRegLen && r[4 + i]; ++i) {
        char c = (char)r[4 + i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-') out[k++] = c;
      }
      out[k] = 0;
      return k > 0;
    }
    if (v < id) lo = mid + 1;
    else hi = mid;
  }
  return false;
}

bool lookup(uint32_t id, char out[kRegLen + 1]) {
  if (!g_read) return false;
  for (int i = 0; i < kSlots; ++i)
    if (g_slot[i].open && lookupSlot(g_slot[i], id, out)) return true;
  return false;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool rewritePflaa(const char* line, char* out, size_t cap) {
  if (strncmp(line, "$PFLAA,", 7) != 0) return false;
  // ID is field 6 (PFLAA,alarm,north,east,vertical,idtype,ID,...)
  const char* p = line + 7;
  for (int f = 1; f < 6; ++f) {
    p = strchr(p, ',');
    if (!p) return false;
    ++p;
  }
  const char* e = p;
  uint32_t id = 0;
  int digits = 0;
  while (hexVal(*e) >= 0 && digits < 6) {
    id = (id << 4) | (uint32_t)hexVal(*e);
    ++e;
    ++digits;
  }
  if (digits != 6 || (*e != ',' && *e != '*' && *e != '\0')) return false;  // also rejects an existing "!alias"
  char reg[kRegLen + 1];
  if (!lookup(id, reg)) return false;
  size_t head = (size_t)(e - line);
  const char* star = strchr(e, '*');
  size_t tail = star ? (size_t)(star - e) : strlen(e);
  size_t rl = strlen(reg);
  size_t need = head + 1 + rl + tail + (star ? 3 : 0) + 1;
  if (need > cap) return false;
  memcpy(out, line, head);
  out[head] = '!';
  memcpy(out + head + 1, reg, rl);
  memcpy(out + head + 1 + rl, e, tail);
  size_t len = head + 1 + rl + tail;
  if (star) {
    uint8_t cs = 0;
    for (size_t i = 1; i < len; ++i) cs ^= (uint8_t)out[i];
    snprintf(out + len, 4, "*%02X", cs);
    len += 3;
  }
  out[len] = 0;
  return true;
}

void LineRewriter::feed(const uint8_t* data, size_t n, Emit emit, void* ctx) {
  for (size_t i = 0; i < n; ++i) {
    uint8_t c = data[i];
    if (overflow_) {  // too long to be NMEA: pass through unchanged until the line ends
      emit(&c, 1, ctx);
      if (c == '\n') overflow_ = false;
      continue;
    }
    if (n_ >= sizeof buf_) {
      emit((const uint8_t*)buf_, n_, ctx);
      n_ = 0;
      overflow_ = true;
      emit(&c, 1, ctx);
      continue;
    }
    buf_[n_++] = (char)c;
    if (c != '\n') continue;
    // complete line: split off the line ending
    size_t body = n_;
    while (body > 0 && (buf_[body - 1] == '\n' || buf_[body - 1] == '\r')) --body;
    char tmp[sizeof buf_ + 16];
    char save = buf_[body];
    buf_[body] = 0;
    bool done = false;
    if (body > 7 && rewritePflaa(buf_, tmp, sizeof tmp)) {
      size_t l = strlen(tmp);
      buf_[body] = save;
      memcpy(tmp + l, buf_ + body, n_ - body);
      emit((const uint8_t*)tmp, l + (n_ - body), ctx);
      done = true;
    }
    if (!done) {
      buf_[body] = save;
      emit((const uint8_t*)buf_, n_, ctx);
    }
    n_ = 0;
  }
}

}  // namespace csdb
