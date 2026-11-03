#include "Nmea.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nmea {

// ---------------------------------------------------------------- assembler
bool LineAssembler::push(uint8_t b) {
  if (b == '\r' || b == '\n') {
    bool ok = (len_ > 0) && !overflow_;
    if (ok) buf_[len_] = '\0';
    len_ = 0;
    overflow_ = false;
    return ok;
  }
  if (b == '$') {  // a '$' always starts a new sentence
    len_ = 0;
    overflow_ = false;
    buf_[len_++] = '$';
    return false;
  }
  if (len_ == 0) return false;  // garbage between sentences
  if (len_ >= kMaxLine) {
    if (!overflow_) {
      overflow_ = true;
      ++overflows_;
    }
    return false;
  }
  buf_[len_++] = (char)b;
  return false;
}

// ----------------------------------------------------------------- checksum
static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

Checksum verifyChecksum(const char* line) {
  if (!line || line[0] != '$') return Checksum::Bad;
  const char* star = strchr(line, '*');
  if (!star) return Checksum::Missing;
  uint8_t x = 0;
  for (const char* p = line + 1; p < star; ++p) x ^= (uint8_t)*p;
  int h = hexVal(star[1]);
  if (h < 0) return Checksum::Bad;
  int l = hexVal(star[2]);
  if (l < 0) return Checksum::Bad;
  return (x == (uint8_t)(h * 16 + l)) ? Checksum::Ok : Checksum::Bad;
}

size_t appendChecksum(char* line, size_t cap) {
  size_t len = strlen(line);
  if (len + 4 > cap) return len;
  uint8_t x = 0;
  for (size_t i = 1; i < len; ++i) x ^= (uint8_t)line[i];
  snprintf(line + len, cap - len, "*%02X", x);
  return len + 3;
}

// ---------------------------------------------------------------- tokenizer
int split(char* line, char** fields, int maxFields) {
  if (!line || line[0] != '$' || maxFields < 1) return 0;
  char* p = line + 1;
  char* star = strchr(p, '*');
  if (star) *star = '\0';
  int n = 0;
  fields[n++] = p;
  for (; *p; ++p) {
    if (*p == ',') {
      *p = '\0';
      if (n < maxFields) fields[n++] = p + 1;
    }
  }
  return n;
}

Type classify(const char* id) {
  if (!id) return Type::Unknown;
  if (strcmp(id, "PGRMZ") == 0) return Type::Pgrmz;
  if (strcmp(id, "PFLAU") == 0) return Type::Pflau;
  if (strcmp(id, "PFLAA") == 0) return Type::Pflaa;
  if (strlen(id) == 5 && id[0] == 'G') {  // GP/GN/GL/GA... talkers
    const char* t = id + 2;
    if (strcmp(t, "RMC") == 0) return Type::Rmc;
    if (strcmp(t, "GGA") == 0) return Type::Gga;
    if (strcmp(t, "GSA") == 0) return Type::Gsa;
  }
  return Type::Unknown;
}

// ------------------------------------------------------------------ helpers
static bool toDouble(const char* s, double& v) {
  if (!s || !*s) return false;
  char* e = nullptr;
  double d = strtod(s, &e);
  if (e == s || *e != '\0' || !isfinite(d)) return false;
  v = d;
  return true;
}

static bool toLong(const char* s, long& v) {
  if (!s || !*s) return false;
  char* e = nullptr;
  long d = strtol(s, &e, 10);
  if (e == s || *e != '\0') return false;
  v = d;
  return true;
}

static bool toHex(const char* s, uint32_t& v) {
  if (!s || !*s) return false;
  char* e = nullptr;
  unsigned long d = strtoul(s, &e, 16);
  if (e == s || *e != '\0') return false;
  v = (uint32_t)d;
  return true;
}

static bool toCoord(const char* v, const char* h, bool lat, double& out) {
  double raw;
  if (!toDouble(v, raw) || !h || !*h || raw < 0) return false;
  double deg = floor(raw / 100.0);
  double min = raw - deg * 100.0;
  if (min >= 60.0) return false;
  double r = deg + min / 60.0;
  char c = h[0];
  if (lat) {
    if (c == 'S') r = -r;
    else if (c != 'N') return false;
    if (r < -90.0 || r > 90.0) return false;
  } else {
    if (c == 'W') r = -r;
    else if (c != 'E') return false;
    if (r < -180.0 || r > 180.0) return false;
  }
  out = r;
  return true;
}

static bool toTime(const char* s, uint32_t& sec) {
  if (!s) return false;
  for (int i = 0; i < 6; ++i)
    if (s[i] < '0' || s[i] > '9') return false;
  uint32_t hh = (s[0] - '0') * 10 + (s[1] - '0');
  uint32_t mm = (s[2] - '0') * 10 + (s[3] - '0');
  uint32_t ss = (s[4] - '0') * 10 + (s[5] - '0');
  if (hh > 23 || mm > 59 || ss > 60) return false;
  if (ss == 60) ss = 59;
  sec = hh * 3600 + mm * 60 + ss;
  return true;
}

static float norm360(double v) {
  v = fmod(v, 360.0);
  if (v < 0) v += 360.0;
  return (float)v;
}

// ------------------------------------------------------------------ parsers
bool parseRmc(char** f, int n, Rmc& o) {
  memset(&o, 0, sizeof o);
  if (n < 9) return false;
  uint32_t t;
  if (toTime(f[1], t)) {
    o.hasTime = true;
    o.secOfDay = t;
  }
  bool ok = (f[2][0] == 'A');
  if (n > 12 && f[12][0] == 'N') ok = false;  // mode indicator: not valid
  if (!ok) return true;
  if (!toCoord(f[3], f[4], true, o.lat) || !toCoord(f[5], f[6], false, o.lon)) return true;
  o.hasPos = true;
  o.valid = true;
  double v;
  if (toDouble(f[7], v) && v >= 0) {
    o.hasSog = true;
    o.sogKt = (float)v;
  }
  if (toDouble(f[8], v)) {
    o.hasTrack = true;
    o.trackDeg = norm360(v);
  }
  return true;
}

bool parseGga(char** f, int n, Gga& o) {
  memset(&o, 0, sizeof o);
  if (n < 10) return false;
  long q, s;
  double d;
  if (!toLong(f[6], q)) return false;
  o.quality = (uint8_t)(q < 0 ? 0 : q > 255 ? 255 : q);
  if (toLong(f[7], s)) o.sats = (uint8_t)(s < 0 ? 0 : s > 255 ? 255 : s);
  if (toDouble(f[8], d) && d >= 0) {
    o.hasHdop = true;
    o.hdop = (float)d;
  }
  if (toDouble(f[9], d)) {
    o.hasAlt = true;
    o.altMslM = (float)d;
  }
  if (n > 11 && toDouble(f[11], d)) {
    o.hasGeoid = true;
    o.geoidM = (float)d;
  }
  return true;
}

bool parseGsa(char** f, int n, Gsa& o) {
  memset(&o, 0, sizeof o);
  if (n < 3) return false;
  long m;
  if (toLong(f[2], m) && m >= 1 && m <= 3) o.fixMode = (uint8_t)m;
  double d;
  if (n > 16 && toDouble(f[16], d) && d >= 0) {
    o.hasHdop = true;
    o.hdop = (float)d;
  }
  return true;
}

bool parsePgrmz(char** f, int n, Pgrmz& o) {
  memset(&o, 0, sizeof o);
  if (n < 2) return false;
  double a;
  if (!toDouble(f[1], a)) return false;
  if (n > 2 && (f[2][0] == 'm' || f[2][0] == 'M')) a *= 3.28084;
  o.altFt = (int32_t)lround(a);
  return true;
}

bool parsePflau(char** f, int n, Pflau& o) {
  memset(&o, 0, sizeof o);
  if (n < 6) return false;
  long v;
  if (!toLong(f[5], v) || v < 0 || v > 3) return false;
  o.alarmLevel = (uint8_t)v;
  if (toLong(f[1], v) && v >= 0 && v < 256) o.rx = (uint8_t)v;
  if (toLong(f[2], v) && v >= 0 && v < 256) o.tx = (uint8_t)v;
  if (toLong(f[3], v) && v >= 0 && v < 256) o.gps = (uint8_t)v;
  return true;
}

bool parsePflaa(char** f, int n, Pflaa& o) {
  memset(&o, 0, sizeof o);
  if (n < 7) return false;
  long l;
  double d;
  uint32_t h;
  if (toLong(f[1], l) && l >= 0 && l <= 3) o.alarmLevel = (uint8_t)l;
  if (!toDouble(f[2], d)) return false;
  o.relNorthM = (int32_t)lround(d);
  if (!toDouble(f[3], d)) return false;
  o.relEastM = (int32_t)lround(d);
  if (!toDouble(f[4], d)) return false;
  o.relVertM = (int32_t)lround(d);
  if (toLong(f[5], l) && l >= 0 && l < 256) o.idType = (uint8_t)l;
  if (!toHex(f[6], h)) return false;
  o.id = h & 0xFFFFFF;
  bool noTrack = (n > 12 && f[12][0] == '1');
  if (n > 7 && !noTrack && toDouble(f[7], d)) {
    o.hasTrack = true;
    o.trackDeg = norm360(d);
  }
  if (n > 9 && toDouble(f[9], d) && d >= 0) {
    o.hasSpeed = true;
    o.speedMs = (float)d;
  }
  if (n > 10 && toDouble(f[10], d)) {
    o.hasClimb = true;
    o.climbMs = (float)d;
  }
  if (n > 11 && toHex(f[11], h)) o.acType = (uint8_t)(h & 0xFF);
  return true;
}

}  // namespace nmea
