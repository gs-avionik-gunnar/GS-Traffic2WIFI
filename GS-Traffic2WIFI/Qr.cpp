#include "Qr.h"

#include <string.h>

namespace qr {

namespace {

// Versions 1..6, level L: total codewords, EC codewords per block, number of blocks.
struct Ver { uint8_t total, ec, blocks; uint8_t cap; };
const Ver kVer[6] = {{26, 7, 1, 17}, {44, 10, 1, 32}, {70, 15, 1, 53}, {100, 20, 1, 78}, {134, 26, 1, 106}, {172, 18, 2, 134}};
const uint8_t kAlign[6] = {0, 18, 22, 26, 30, 34};  // centre of the single alignment pattern (v2+)

uint8_t gfExp[512], gfLog[256];
bool gfReady = false;

void gfInit() {
  if (gfReady) return;
  int x = 1;
  for (int i = 0; i < 255; ++i) {
    gfExp[i] = (uint8_t)x;
    gfLog[x] = (uint8_t)i;
    x <<= 1;
    if (x & 0x100) x ^= 0x11D;
  }
  for (int i = 255; i < 512; ++i) gfExp[i] = gfExp[i - 255];
  gfReady = true;
}

uint8_t gfMul(uint8_t a, uint8_t b) { return (a && b) ? gfExp[gfLog[a] + gfLog[b]] : 0; }

// Reed-Solomon: ec bytes for data[0..n)
void rs(const uint8_t* data, int n, int ec, uint8_t* out) {
  uint8_t gen[32];
  memset(gen, 0, sizeof gen);
  gen[0] = 1;
  int deg = 0;
  for (int i = 0; i < ec; ++i) {  // multiply by (x - a^i)
    for (int j = deg + 1; j > 0; --j) gen[j] = (uint8_t)(gen[j - 1] ^ gfMul(gen[j], gfExp[i]));
    gen[0] = gfMul(gen[0], gfExp[i]);
    ++deg;
  }
  // gen[k] = coefficient of x^k (gen[deg] == 1). Polynomial division of data * x^ec.
  uint8_t rem[32];
  memset(rem, 0, sizeof rem);
  for (int i = 0; i < n; ++i) {
    uint8_t f = (uint8_t)(data[i] ^ rem[0]);
    memmove(rem, rem + 1, (size_t)ec - 1);
    rem[ec - 1] = 0;
    for (int k = 0; k < ec; ++k) rem[k] ^= gfMul(gen[ec - 1 - k], f);
  }
  memcpy(out, rem, (size_t)ec);
}

struct Builder {
  int n;
  uint8_t mod[kMaxSize][kMaxSize];
  uint8_t fn[kMaxSize][kMaxSize];  // function pattern (not data)

  void set(int x, int y, int v) {
    if (x < 0 || y < 0 || x >= n || y >= n) return;
    mod[y][x] = (uint8_t)v;
    fn[y][x] = 1;
  }
  void finder(int cx, int cy) {  // centre of the 7x7 pattern, plus separator
    for (int dy = -4; dy <= 4; ++dy)
      for (int dx = -4; dx <= 4; ++dx) {
        int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        set(cx + dx, cy + dy, d != 2 && d != 4);
      }
  }
  static int abs(int v) { return v < 0 ? -v : v; }
  void align(int cx, int cy) {
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) {
        int d = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        set(cx + dx, cy + dy, d != 1);
      }
  }
  void formatBits(int mask) {
    int data = (1 << 3) | mask;  // EC level L = 01, then mask
    int rem = data;
    for (int i = 0; i < 10; ++i) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;
    for (int i = 0; i <= 5; ++i) set(8, i, (bits >> i) & 1);
    set(8, 7, (bits >> 6) & 1);
    set(8, 8, (bits >> 7) & 1);
    set(7, 8, (bits >> 8) & 1);
    for (int i = 9; i < 15; ++i) set(14 - i, 8, (bits >> i) & 1);
    for (int i = 0; i < 8; ++i) set(n - 1 - i, 8, (bits >> i) & 1);
    for (int i = 8; i < 15; ++i) set(8, n - 15 + i, (bits >> i) & 1);
    set(8, n - 8, 1);  // dark module
  }
  void functionPatterns(int version) {
    memset(mod, 0, sizeof mod);
    memset(fn, 0, sizeof fn);
    for (int i = 0; i < n; ++i) {
      set(6, i, i % 2 == 0);
      set(i, 6, i % 2 == 0);
    }
    finder(3, 3);
    finder(n - 4, 3);
    finder(3, n - 4);
    if (version >= 2) align(kAlign[version - 1], kAlign[version - 1]);
    formatBits(0);  // reserves the area
  }
  void place(const uint8_t* cw, int count) {
    int bit = 0, total = count * 8;
    for (int right = n - 1; right >= 1; right -= 2) {
      if (right == 6) right = 5;
      for (int v = 0; v < n; ++v)
        for (int j = 0; j < 2; ++j) {
          int x = right - j;
          bool up = ((right + 1) & 2) == 0;
          int y = up ? n - 1 - v : v;
          if (fn[y][x] || bit >= total) continue;
          mod[y][x] = (cw[bit >> 3] >> (7 - (bit & 7))) & 1;
          ++bit;
        }
    }
  }
  static bool maskBit(int m, int x, int y) {
    switch (m) {
      case 0: return (x + y) % 2 == 0;
      case 1: return y % 2 == 0;
      case 2: return x % 3 == 0;
      case 3: return (x + y) % 3 == 0;
      case 4: return (x / 3 + y / 2) % 2 == 0;
      case 5: return (x * y) % 2 + (x * y) % 3 == 0;
      case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
      default: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
    }
  }
  void applyMask(int m) {
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x)
        if (!fn[y][x] && maskBit(m, x, y)) mod[y][x] ^= 1;
  }
  int penalty() const {
    int p = 0;
    for (int pass = 0; pass < 2; ++pass)
      for (int a = 0; a < n; ++a) {
        int run = 1;
        for (int b = 1; b < n; ++b) {
          int cur = pass ? mod[b][a] : mod[a][b], prev = pass ? mod[b - 1][a] : mod[a][b - 1];
          if (cur == prev) {
            ++run;
            if (run == 5) p += 3;
            else if (run > 5) ++p;
          } else {
            run = 1;
          }
        }
      }
    for (int y = 0; y + 1 < n; ++y)
      for (int x = 0; x + 1 < n; ++x)
        if (mod[y][x] == mod[y][x + 1] && mod[y][x] == mod[y + 1][x] && mod[y][x] == mod[y + 1][x + 1]) p += 3;
    static const uint8_t pat[11] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
    for (int pass = 0; pass < 2; ++pass)
      for (int a = 0; a < n; ++a)
        for (int b = 0; b + 11 <= n; ++b) {
          bool f = true, r = true;
          for (int k = 0; k < 11; ++k) {
            int v = pass ? mod[b + k][a] : mod[a][b + k];
            if (v != pat[k]) f = false;
            if (v != pat[10 - k]) r = false;
          }
          if (f || r) p += 40;
        }
    int dark = 0;
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) dark += mod[y][x];
    int pct = dark * 100 / (n * n);
    int dev = pct > 50 ? pct - 50 : 50 - pct;
    p += (dev / 5) * 10;
    return p;
  }
};

}  // namespace

bool encode(const char* text, Code& out) {
  gfInit();
  memset(&out, 0, sizeof out);
  size_t len = strlen(text);
  int v = 0;
  for (int i = 0; i < 6; ++i)
    if (len <= kVer[i].cap) { v = i + 1; break; }
  if (!v) return false;
  const Ver& vi = kVer[v - 1];
  int dataCw = vi.total - vi.ec * vi.blocks;

  uint8_t data[140];
  memset(data, 0, sizeof data);
  int bitPos = 0;
  auto put = [&](int val, int bits) {
    for (int i = bits - 1; i >= 0; --i, ++bitPos)
      if ((val >> i) & 1) data[bitPos >> 3] |= (uint8_t)(0x80 >> (bitPos & 7));
  };
  put(4, 4);
  put((int)len, 8);
  for (size_t i = 0; i < len; ++i) put((uint8_t)text[i], 8);
  int cap = dataCw * 8;
  int term = cap - bitPos < 4 ? cap - bitPos : 4;
  put(0, term);
  bitPos = (bitPos + 7) & ~7;
  for (int pad = 0; bitPos < cap; pad ^= 1) put(pad ? 0x11 : 0xEC, 8);

  // blocks + error correction, interleaved
  uint8_t all[200];
  int per = dataCw / vi.blocks;  // all blocks have equal size for versions 1..6 level L
  uint8_t ecb[2][32];
  for (int b = 0; b < vi.blocks; ++b) rs(data + b * per, per, vi.ec, ecb[b]);
  int k = 0;
  for (int i = 0; i < per; ++i)
    for (int b = 0; b < vi.blocks; ++b) all[k++] = data[b * per + i];
  for (int i = 0; i < vi.ec; ++i)
    for (int b = 0; b < vi.blocks; ++b) all[k++] = ecb[b][i];

  Builder bld;
  bld.n = 17 + 4 * v;
  bld.functionPatterns(v);
  bld.place(all, k);

  int best = 0, bestP = 1 << 30;
  for (int m = 0; m < 8; ++m) {
    bld.applyMask(m);
    bld.formatBits(m);
    int p = bld.penalty();
    if (p < bestP) { bestP = p; best = m; }
    bld.applyMask(m);  // undo
  }
  bld.applyMask(best);
  bld.formatBits(best);
  out.size = bld.n;
  for (int y = 0; y < bld.n; ++y)
    for (int x = 0; x < bld.n; ++x) out.m[y][x] = bld.mod[y][x];
  return true;
}

bool wifiString(const char* ssid, const char* key, char* out, size_t cap) {
  size_t n = 0;
  auto add = [&](const char* s, bool esc) {
    for (; *s; ++s) {
      if (esc && (*s == '\\' || *s == ';' || *s == ',' || *s == ':' || *s == '"')) {
        if (n + 1 >= cap) return false;
        out[n++] = '\\';
      }
      if (n + 1 >= cap) return false;
      out[n++] = *s;
    }
    return true;
  };
  bool ok = add("WIFI:T:WPA;S:", false) && add(ssid, true) && add(";P:", false) && add(key, true) && add(";;", false);
  out[n < cap ? n : cap - 1] = '\0';
  return ok;
}

}  // namespace qr
