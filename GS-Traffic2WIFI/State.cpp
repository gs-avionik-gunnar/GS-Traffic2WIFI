#include "State.h"

#include <math.h>
#include <string.h>

// ------------------------------------------------------------ traffic table
void TrafficTable::clear() { memset(slots_, 0, sizeof slots_); }

void TrafficTable::update(const Target& t) {
  int freeIdx = -1, oldest = 0;
  uint32_t oldestAge = 0;
  for (int i = 0; i < kMax; ++i) {
    if (slots_[i].used && slots_[i].id == t.id) {
      slots_[i] = t;
      slots_[i].used = true;
      return;
    }
    if (!slots_[i].used) {
      if (freeIdx < 0) freeIdx = i;
    } else {
      uint32_t a = (uint32_t)(t.lastMs - slots_[i].lastMs);
      if (a >= oldestAge) {
        oldestAge = a;
        oldest = i;
      }
    }
  }
  int idx = freeIdx >= 0 ? freeIdx : oldest;
  slots_[idx] = t;
  slots_[idx].used = true;
}

void TrafficTable::expire(uint32_t now, uint32_t timeoutMs) {
  for (int i = 0; i < kMax; ++i)
    if (slots_[i].used && (uint32_t)(now - slots_[i].lastMs) > timeoutMs) slots_[i].used = false;
}

int TrafficTable::count() const {
  int c = 0;
  for (int i = 0; i < kMax; ++i)
    if (slots_[i].used) ++c;
  return c;
}

// ---------------------------------------------------------------- nav state
void NavState::reset() {
  memset((void*)this, 0, sizeof(*this));  // plain data only; traffic.clear() below
  traffic.clear();
}

void NavState::noteRejected(const char* line, char why) {
  char* d = rej[rejNext];
  rejNext = (uint8_t)((rejNext + 1) % kRejKeep);
  size_t o = 0;
  d[o++] = why;
  d[o++] = ':';
  for (size_t i = 0; line[i] && o < sizeof rej[0] - 1; ++i) {
    unsigned char c = (unsigned char)line[i];
    d[o++] = (c >= 0x20 && c < 0x7F && c != '"' && c != '\\' && c != '<' && c != '>' && c != '&') ? (char)c : '.';
  }
  d[o] = '\0';
}

nmea::Type NavState::ingestLine(const char* line, uint32_t now) {
  using namespace nmea;
  ++stats.lines;
  Checksum cs = verifyChecksum(line);
  if (cs == Checksum::Missing) {
    ++stats.noChecksum;
    if (requireChecksum) {
      noteRejected(line, 'N');
      return Type::Unknown;
    }
  } else if (cs == Checksum::Bad) {
    ++stats.badChecksum;
    noteRejected(line, 'B');
    return Type::Unknown;
  }
  char tmp[kMaxLine + 1];
  strncpy(tmp, line, kMaxLine);
  tmp[kMaxLine] = '\0';
  char* f[kMaxFields];
  int n = split(tmp, f, kMaxFields);
  if (n < 1) return Type::Unknown;
  Type t = classify(f[0]);
  bool ok = false;
  switch (t) {
    case Type::Rmc: {
      Rmc r;
      if ((ok = parseRmc(f, n, r))) applyRmc(r, now);
      break;
    }
    case Type::Gga: {
      Gga g;
      if ((ok = parseGga(f, n, g))) applyGga(g, now);
      break;
    }
    case Type::Gsa: {
      Gsa g;
      if ((ok = parseGsa(f, n, g))) applyGsa(g, now);
      break;
    }
    case Type::Pgrmz: {
      Pgrmz z;
      if ((ok = parsePgrmz(f, n, z))) applyPgrmz(z, now);
      break;
    }
    case Type::Pflau: {
      Pflau u;
      if ((ok = parsePflau(f, n, u))) applyPflau(u, now);
      break;
    }
    case Type::Pflaa: {
      Pflaa a;
      if ((ok = parsePflaa(f, n, a))) applyPflaa(a, now);
      break;
    }
    default:
      ++stats.unknown;
      return Type::Unknown;
  }
  if (!ok) {
    ++stats.parseErrors;
    return Type::Unknown;
  }
  lastRxMs = now;
  return t;
}

// Days since 1970-01-01 for a civil date (proleptic Gregorian).
static uint32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int era = y / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (uint32_t)(era * 146097 + doe - 719468);
}

void NavState::applyRmc(const nmea::Rmc& r, uint32_t now) {
  rmcValid = r.valid;
  posMs = now;
  if (!r.valid) return;
  lat = r.lat;
  lon = r.lon;
  sogValid = r.hasSog;
  sogKt = r.hasSog ? r.sogKt : 0.0f;
  trackValid = r.hasTrack;
  trackDeg = r.hasTrack ? r.trackDeg : 0.0f;
  if (r.hasTime) {
    timeValid = true;
    secOfDay = r.secOfDay;
    timeMs = now;
    if (r.hasDate) {
      dateValid = true;
      utcDay = daysFromCivil(r.year, r.month, r.day);
    }
  }
}

void NavState::applyGga(const nmea::Gga& g, uint32_t now) {
  fixQuality = g.quality;
  sats = g.sats;
  if (g.hasHdop) {
    hdopValid = true;
    hdop = g.hdop;
    accMs = now;
  }
  altValid = g.hasAlt && g.quality > 0;
  if (altValid) {
    altMslM = g.altMslM;
    geoidValid = g.hasGeoid;
    geoidM = g.hasGeoid ? g.geoidM : 0.0f;
    altMs = now;
  }
}

void NavState::applyGsa(const nmea::Gsa& g, uint32_t now) {
  fixMode = g.fixMode;
  if (g.hasHdop) {
    hdopValid = true;
    hdop = g.hdop;
    accMs = now;
  }
}

void NavState::applyPgrmz(const nmea::Pgrmz& z, uint32_t now) {
  baroValid = true;
  baroAltFt = z.altFt;
  baroMs = now;
}

static void noteAlarm(NavState& s, uint8_t level, uint32_t now) {
  if (level >= 1) {
    s.alarmLevel = level;
    s.alarmMs = now;
    s.alarmEver = true;
  }
}

void NavState::applyPflau(const nmea::Pflau& u, uint32_t now) {
  flarmSeen = true;
  flarmRx = u.rx;
  flarmTx = u.tx;
  flarmGps = u.gps;
  flarmMs = now;
  noteAlarm(*this, u.alarmLevel, now);
}

bool NavState::refAltitudeFt(uint32_t now, int32_t& ft) const {
  if (baroFresh(now)) {
    ft = baroAltFt;
    return true;
  }
  if (altFresh(now)) {
    ft = (int32_t)lround(altMslM * 3.28084);
    return true;
  }
  return false;
}

bool NavState::geoAltitudeFt(uint32_t now, int32_t& ft) const {
  if (!altFresh(now)) return false;
  float m = altMslM + (geoidValid ? geoidM : 0.0f);
  ft = (int32_t)lround(m * 3.28084);
  return true;
}

void NavState::applyPflaa(const nmea::Pflaa& a, uint32_t now) {
  noteAlarm(*this, a.alarmLevel, now);
  if (!posFresh(now)) return;  // cannot place a relative target without ownship position

  const double kDegToRad = 3.14159265358979323846 / 180.0;
  double cosLat = cos(lat * kDegToRad);
  if (cosLat < 0.01) cosLat = 0.01;

  Target t;
  memset(&t, 0, sizeof t);
  t.id = a.id;
  t.idType = a.idType;
  t.acType = a.acType;
  t.alarm = a.alarmLevel;
  t.lat = lat + a.relNorthM / 111132.0;
  t.lon = lon + a.relEastM / (111320.0 * cosLat);
  int32_t refFt;
  if (refAltitudeFt(now, refFt)) {
    t.altValid = true;
    t.altFt = refFt + (int32_t)lround(a.relVertM * 3.28084);
  }
  t.trackValid = a.hasTrack;
  t.trackDeg = a.trackDeg;
  t.speedValid = a.hasSpeed;
  t.speedKt = a.speedMs * 1.943844f;
  t.climbValid = a.hasClimb;
  t.climbFpm = a.climbMs * 196.8504f;
  t.distM = (float)sqrt((double)a.relNorthM * a.relNorthM + (double)a.relEastM * a.relEastM);
  double brg = atan2((double)a.relEastM, (double)a.relNorthM) / kDegToRad;
  if (brg < 0) brg += 360.0;
  t.bearingDeg = (float)brg;
  t.lastMs = now;
  traffic.update(t);
}

// --------------------------------------------------------------------- LED
uint8_t computeLedState(const NavState& nav, uint32_t now, uint8_t trafficLevel, bool positionLed,
                        uint32_t trafficHoldMs, uint32_t positionHoldMs) {
  if (trafficLevel > 0 && nav.alarmEver && nav.alarmLevel >= trafficLevel &&
      NavState::age(now, nav.alarmMs) < trafficHoldMs)
    return 2;
  if (positionLed) {
    bool havePos = nav.rmcValid && NavState::age(now, nav.posMs) < positionHoldMs;
    if (!havePos) return 3;
  }
  return 0;
}
