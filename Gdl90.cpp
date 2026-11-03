#include "Gdl90.h"

#include <math.h>
#include <string.h>

namespace gdl90 {

// CRC-16 as defined by the GDL90 ICD (section 2.2.3): crc = Table[crc >> 8] ^ (crc << 8) ^ byte, polynomial 0x1021.
// NOT the same as CRC-16/XMODEM (which xors the byte before the table lookup): the ICD example heartbeat
// 00 81 41 DB D0 08 02 has the CRC 0x8BB3, XMODEM would give 0x50DA.
uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc = 0;
  while (n--) {
    uint16_t t = (uint16_t)(crc & 0xFF00);  // Table[crc >> 8], computed bitwise
    for (int i = 0; i < 8; ++i) t = (t & 0x8000) ? (uint16_t)((t << 1) ^ 0x1021) : (uint16_t)(t << 1);
    crc = (uint16_t)(t ^ (uint16_t)(crc << 8) ^ *p++);
  }
  return crc;
}

size_t frame(const uint8_t* payload, size_t len, uint8_t* out, size_t cap) {
  if (cap < 2) return 0;
  uint16_t crc = crc16(payload, len);
  size_t o = 0;
  out[o++] = 0x7E;
  for (size_t i = 0; i < len + 2; ++i) {
    uint8_t b = (i < len) ? payload[i] : (i == len ? (uint8_t)(crc & 0xFF) : (uint8_t)(crc >> 8));
    if (b == 0x7E || b == 0x7D) {
      if (o + 2 + 1 > cap) return 0;
      out[o++] = 0x7D;
      out[o++] = b ^ 0x20;
    } else {
      if (o + 1 + 1 > cap) return 0;
      out[o++] = b;
    }
  }
  out[o++] = 0x7E;
  return o;
}

// ---------------------------------------------------------------- encoders
size_t encodeHeartbeat(uint8_t* o, bool gpsValid, bool utcOk, uint32_t secOfDay) {
  secOfDay %= 86400u;
  o[0] = kIdHeartbeat;
  o[1] = (uint8_t)((gpsValid ? 0x80 : 0x00) | 0x10 | 0x01);  // bit4 addr talkback + bit0 UAT initialized, as Stratux
  o[2] = (uint8_t)((((secOfDay >> 16) & 1) << 7) | (utcOk ? 0x01 : 0x00));
  o[3] = (uint8_t)(secOfDay & 0xFF);
  o[4] = (uint8_t)((secOfDay >> 8) & 0xFF);
  o[5] = 0;  // message counts
  o[6] = 0;
  return 7;
}

static uint32_t raw24(double deg) {
  long v = lround(deg * (8388608.0 / 180.0));
  if (v > 8388607) v = 8388607;
  if (v < -8388608) v = -8388608;
  return (uint32_t)(int32_t)v & 0xFFFFFFu;
}

size_t encodeReport(uint8_t msgId, const Report& r, uint8_t* o) {
  memset(o, 0, kReportLen);
  o[0] = msgId;
  o[1] = (uint8_t)(((r.alert ? 1 : 0) << 4) | (r.addrType & 0x0F));
  o[2] = (uint8_t)(r.addr >> 16);
  o[3] = (uint8_t)(r.addr >> 8);
  o[4] = (uint8_t)(r.addr);
  uint32_t la = r.posValid ? raw24(r.lat) : 0, lo = r.posValid ? raw24(r.lon) : 0;
  o[5] = (uint8_t)(la >> 16);
  o[6] = (uint8_t)(la >> 8);
  o[7] = (uint8_t)la;
  o[8] = (uint8_t)(lo >> 16);
  o[9] = (uint8_t)(lo >> 8);
  o[10] = (uint8_t)lo;

  uint16_t alt = 0xFFF;
  if (r.altValid) {
    long a = lround((r.altFt + 1000) / 25.0);
    if (a < 0) a = 0;
    if (a > 0xFFE) a = 0xFFE;
    alt = (uint16_t)a;
  }
  uint8_t misc = (uint8_t)((r.airborne ? 8 : 0) | (r.extrapolated ? 4 : 0) | (r.trackValid ? 1 : 0));
  o[11] = (uint8_t)(alt >> 4);
  o[12] = (uint8_t)(((alt & 0xF) << 4) | misc);
  o[13] = (uint8_t)(((r.nic & 0xF) << 4) | (r.nacp & 0xF));

  uint16_t hv = 0xFFF;
  if (r.hvValid) {
    long v = lround(r.hvKt);
    if (v < 0) v = 0;
    if (v > 0xFFE) v = 0xFFE;
    hv = (uint16_t)v;
  }
  uint16_t vv = 0x800;
  if (r.vvValid) {
    long v = lround(r.vvFpm / 64.0);
    if (v > 510) v = 510;
    if (v < -510) v = -510;
    vv = (uint16_t)((int32_t)v & 0xFFF);
  }
  o[14] = (uint8_t)(hv >> 4);
  o[15] = (uint8_t)(((hv & 0xF) << 4) | ((vv >> 8) & 0xF));
  o[16] = (uint8_t)(vv & 0xFF);
  o[17] = r.trackValid ? (uint8_t)(lround(r.trackDeg * 256.0 / 360.0) & 0xFF) : 0;
  o[18] = r.emitter;
  size_t i = 0;
  for (; i < 8 && r.callsign[i]; ++i) {
    char c = r.callsign[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    o[19 + i] = (uint8_t)c;
  }
  for (; i < 8; ++i) o[19 + i] = ' ';
  o[27] = (uint8_t)((r.emergency & 0xF) << 4);
  return kReportLen;
}

size_t encodeStratuxHeartbeat(uint8_t* o, bool gpsValid) {
  o[0] = kIdStratux;
  o[1] = (uint8_t)((1 << 2) | (gpsValid ? 0x02 : 0x00));  // protocol version 1, like Stratux
  return 2;
}

size_t encodeOwnshipGeoAlt(uint8_t* o, int32_t altFt, uint16_t vfomM) {
  long v = lround(altFt / 5.0);
  if (v > 32767) v = 32767;
  if (v < -32768) v = -32768;
  o[0] = kIdOwnshipGeoAlt;
  o[1] = (uint8_t)(((int32_t)v >> 8) & 0xFF);
  o[2] = (uint8_t)((int32_t)v & 0xFF);
  if (vfomM > 0x7FFF) vfomM = 0x7FFF;
  o[3] = (uint8_t)(vfomM >> 8);  // bit 7 (vertical warning) stays 0
  o[4] = (uint8_t)(vfomM & 0xFF);
  return 5;
}

size_t encodeForeFlightId(uint8_t* o, const char* shortName, const char* longName) {
  memset(o, 0, 39);
  o[0] = kIdForeFlight;
  o[1] = 0;  // sub id: device ID
  o[2] = 1;  // version
  for (int i = 3; i < 11; ++i) o[i] = 0xFF;  // serial number: invalid
  strncpy((char*)o + 11, shortName, 8);
  strncpy((char*)o + 19, longName, 16);
  o[38] = 0x01;  // capabilities: geometric altitude is WGS-84 ellipsoid height
  return 39;
}

uint8_t emitterFromFlarm(uint8_t t) {
  switch (t) {
    case 1: return 9;    // glider / motor glider
    case 2: return 1;    // tow plane
    case 3: return 7;    // helicopter
    case 4: return 11;   // skydiver
    case 5: return 1;    // drop plane
    case 6: return 12;   // hang glider
    case 7: return 12;   // paraglider
    case 8: return 1;    // powered aircraft
    case 9: return 3;    // jet
    case 11: return 10;  // balloon
    case 12: return 10;  // airship
    case 13: return 14;  // UAV
    case 15: return 19;  // static object
    default: return 0;
  }
}

// --------------------------------------------------------------- publisher
void Publisher::emit(const uint8_t* payload, size_t len, FrameSink sink, void* ctx) {
  uint8_t buf[kMaxFrame];
  size_t n = frame(payload, len, buf, sizeof buf);
  if (n && sink) {
    sink(buf, n, ctx);
    ++frames_;
  }
}

static uint8_t nacpFromHdop(bool valid, float hdop) {
  if (!valid) return 8;
  float m = hdop * 5.0f;  // rough 1-sigma-ish horizontal error estimate
  if (m < 3) return 11;
  if (m < 10) return 10;
  if (m < 30) return 9;
  if (m < 92.6f) return 8;
  if (m < 185.2f) return 7;
  if (m < 555.6f) return 6;
  return 0;
}

void Publisher::emitSecond(uint32_t now, NavState& nav, FrameSink sink, void* ctx) {
  uint8_t p[40];
  bool pos = nav.posFresh(now);
  bool utc = nav.timeFresh(now) && !cfg_.utcInvalid;
  uint32_t sec = utc ? nav.secOfDay + NavState::age(now, nav.timeMs) / 1000u : now / 1000u;

  emit(p, encodeHeartbeat(p, pos, utc, sec), sink, ctx);
  if (cfg_.stratuxHb) emit(p, encodeStratuxHeartbeat(p, pos), sink, ctx);
  if (cfg_.foreFlightId) emit(p, encodeForeFlightId(p, "GS-TRAF", "GS-Traffic2WIFI"), sink, ctx);

  // Airborne with hysteresis so the flag does not chatter around the threshold.
  float sog = (pos && nav.sogValid) ? nav.sogKt : 0.0f;
  if (sog >= 30.0f) airborne_ = true;
  else if (sog < 15.0f) airborne_ = false;

  Report r;
  memset(&r, 0, sizeof r);
  r.addrType = cfg_.ownAddrType;
  r.addr = cfg_.ownAddr;
  r.posValid = pos;
  r.lat = nav.lat;
  r.lon = nav.lon;
  int32_t ft;
  r.altValid = nav.refAltitudeFt(now, ft);
  r.altFt = ft;
  r.airborne = pos && airborne_;
  r.trackValid = pos && nav.trackValid && sog >= 1.0f;
  r.trackDeg = nav.trackDeg;
  bool acc = nav.accFresh(now);
  r.nic = pos ? 8 : 0;  // like Stratux: fixed NIC 8, NACp from HDOP
  r.nacp = pos ? nacpFromHdop(acc, nav.hdop) : 0;
  r.hvValid = pos && nav.sogValid;
  r.hvKt = nav.sogKt;
  r.vvValid = false;
  r.emitter = cfg_.emitter;
  strncpy(r.callsign, cfg_.callsign, 8);
  emit(p, encodeReport(kIdOwnship, r, p), sink, ctx);

  Gdl90Info& gi = nav.gdl;
  gi.valid = true;
  gi.hbGps = pos;
  gi.hbUtc = utc;
  gi.ownPos = pos;
  gi.lat = r.lat;
  gi.lon = r.lon;
  gi.altValid = r.altValid;
  gi.altFt = r.altFt;
  gi.nic = r.nic;
  gi.nacp = r.nacp;
  gi.airborne = r.airborne;
  ++gi.hbCount;
  ++gi.ownCount;
  gi.geoValid = false;

  int32_t geo;
  if (nav.geoAltitudeFt(now, geo)) {
    // Estimated vertical accuracy (metres) from HDOP; apps may treat 0x7FFF ("n/a") as unusable altitude.
    uint16_t vfom = 15;
    if (acc) {
      float v = nav.hdop * 10.0f;
      vfom = (uint16_t)(v < 5.0f ? 5 : (v > 100.0f ? 100 : v));
    }
    emit(p, encodeOwnshipGeoAlt(p, geo, vfom), sink, ctx);
    gi.geoValid = true;
    gi.geoFt = geo;
    gi.vfom = vfom;
  }
}

void Publisher::emitTraffic(uint32_t now, NavState& nav, FrameSink sink, void* ctx) {
  nav.traffic.expire(now, kTargetTimeoutMs);
  int cnt = nav.traffic.count();
  if (cnt <= 0) return;
  uint32_t pace = 1000u / (uint32_t)cnt;
  if (pace < 10) pace = 10;
  if (NavState::age(now, lastTrafficMs_) < pace) return;
  for (int k = 0; k < TrafficTable::kMax; ++k) {
    int idx = (cursor_ + k) % TrafficTable::kMax;
    const Target& t = nav.traffic.slot(idx);
    if (!t.used) continue;
    Report r;
    memset(&r, 0, sizeof r);
    r.alert = t.alarm >= 1;
    r.addrType = (t.idType == 1) ? 0 : 1;
    r.addr = t.id;
    r.posValid = true;
    r.lat = t.lat;
    r.lon = t.lon;
    r.altValid = t.altValid;
    r.altFt = t.altFt;
    r.airborne = true;
    r.trackValid = t.trackValid;
    r.trackDeg = t.trackDeg;
    r.nic = 8;
    r.nacp = 8;
    r.hvValid = t.speedValid;
    r.hvKt = t.speedKt;
    r.vvValid = t.climbValid;
    r.vvFpm = t.climbFpm;
    r.emitter = emitterFromFlarm(t.acType);
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 6; ++i) r.callsign[i] = hex[(t.id >> (20 - 4 * i)) & 0xF];  // callsign = FLARM/ICAO id
    uint8_t p[kReportLen];
    emit(p, encodeReport(kIdTraffic, r, p), sink, ctx);
    cursor_ = (idx + 1) % TrafficTable::kMax;
    lastTrafficMs_ = now;
    ++nav.gdl.trafficCount;
    return;
  }
}

void Publisher::tick(uint32_t now, NavState& nav, FrameSink sink, void* ctx) {
  const uint32_t interval = 1000u;  // GDL90: heartbeat, ownship and altitude once per second
  if (!started_ || NavState::age(now, lastSecondMs_) >= interval) {
    started_ = true;
    lastSecondMs_ = now;
    emitSecond(now, nav, sink, ctx);
  }
  if (cfg_.traffic) emitTraffic(now, nav, sink, ctx);
}

}  // namespace gdl90
