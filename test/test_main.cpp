// Host-side unit tests for the hardware independent core (no Arduino needed).
// Build & run:  make -C test
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "DialUi.h"
#include "Gdl90.h"
#include "Nmea.h"
#include "Qr.h"
#include "Regions.h"
#include "Simulator.h"
#include "State.h"

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                      \
  do {                                                                   \
    ++g_checks;                                                          \
    if (!(cond)) {                                                       \
      ++g_fail;                                                          \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
    }                                                                    \
  } while (0)
#define CHECK_NEAR(a, b, eps) CHECK(fabs((double)(a) - (double)(b)) <= (eps))

// ------------------------------------------------------------- GDL90 decode
struct Decoded {
  bool crcOk;
  std::vector<uint8_t> payload;
};
static Decoded decode(const uint8_t* f, size_t n) {
  Decoded d;
  d.crcOk = false;
  if (n < 5 || f[0] != 0x7E || f[n - 1] != 0x7E) return d;
  std::vector<uint8_t> raw;
  for (size_t i = 1; i + 1 < n; ++i) {
    if (f[i] == 0x7D) {
      ++i;
      raw.push_back(f[i] ^ 0x20);
    } else {
      CHECK(f[i] != 0x7E);
      raw.push_back(f[i]);
    }
  }
  if (raw.size() < 3) return d;
  size_t len = raw.size() - 2;
  uint16_t crc = (uint16_t)(raw[len] | (raw[len + 1] << 8));
  d.crcOk = (gdl90::crc16(raw.data(), len) == crc);
  d.payload.assign(raw.begin(), raw.begin() + len);
  return d;
}
static double decLat24(const uint8_t* p) {
  int32_t v = (p[0] << 16) | (p[1] << 8) | p[2];
  if (v & 0x800000) v -= 0x1000000;
  return v * (180.0 / 8388608.0);
}

static std::vector<std::vector<uint8_t> > g_frames;
static void collect(const uint8_t* f, size_t n, void*) { g_frames.push_back(std::vector<uint8_t>(f, f + n)); }

// ------------------------------------------------------------------- tests
static void testNmea() {
  using namespace nmea;
  const char* rmc = "$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*68";
  CHECK(verifyChecksum(rmc) == Checksum::Ok);
  CHECK(verifyChecksum("$GPRMC,225446,A,4916.45,N,12311.12,W,000.5,054.7,191194,020.3,E*69") == Checksum::Bad);
  CHECK(verifyChecksum("$GPRMC,1") == Checksum::Missing);
  CHECK(verifyChecksum("$GPRMC,1*G1") == Checksum::Bad);
  CHECK(verifyChecksum("$GPRMC,1*") == Checksum::Bad);

  char buf[100];
  strcpy(buf, "$PGRMZ,3300,f,3");
  appendChecksum(buf, sizeof buf);
  CHECK(verifyChecksum(buf) == Checksum::Ok);

  char tmp[128];
  strcpy(tmp, rmc);
  char* f[kMaxFields];
  int n = split(tmp, f, kMaxFields);
  CHECK(n == 12);
  CHECK(strcmp(f[0], "GPRMC") == 0);
  CHECK(classify(f[0]) == Type::Rmc);
  Rmc r;
  CHECK(parseRmc(f, n, r));
  CHECK(r.valid && r.hasTime && r.secOfDay == 22 * 3600 + 54 * 60 + 46);
  CHECK_NEAR(r.lat, 49.0 + 16.45 / 60.0, 1e-9);
  CHECK_NEAR(r.lon, -(123.0 + 11.12 / 60.0), 1e-9);
  CHECK_NEAR(r.sogKt, 0.5, 1e-6);
  CHECK_NEAR(r.trackDeg, 54.7, 1e-4);

  // void RMC
  strcpy(tmp, "$GPRMC,225446,V,,,,,,,191194,,,N*00");
  n = split(tmp, f, kMaxFields);
  CHECK(parseRmc(f, n, r) && !r.valid && !r.hasPos);

  strcpy(tmp, "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47");
  n = split(tmp, f, kMaxFields);
  CHECK(classify(f[0]) == Type::Gga);
  Gga g;
  CHECK(parseGga(f, n, g));
  CHECK(g.quality == 1 && g.sats == 8 && g.hasHdop && g.hasAlt && g.hasGeoid);
  CHECK_NEAR(g.altMslM, 545.4, 1e-3);
  CHECK_NEAR(g.geoidM, 46.9, 1e-3);

  strcpy(tmp, "$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39");
  n = split(tmp, f, kMaxFields);
  Gsa s;
  CHECK(parseGsa(f, n, s) && s.fixMode == 3 && s.hasHdop);
  CHECK_NEAR(s.hdop, 1.3, 1e-4);

  strcpy(tmp, "$PGRMZ,2282,f,3*00");
  n = split(tmp, f, kMaxFields);
  Pgrmz z;
  CHECK(parsePgrmz(f, n, z) && z.altFt == 2282);

  strcpy(tmp, "$PFLAU,3,1,2,1,2,-30,2,-32,755,1A2B3C*00");
  n = split(tmp, f, kMaxFields);
  Pflau u;
  CHECK(parsePflau(f, n, u) && u.alarmLevel == 2 && u.rx == 3 && u.gps == 2);

  strcpy(tmp, "$PFLAA,1,-1234,4321,220,2,DD8F1B,180,0,30,-1.4,1*00");
  n = split(tmp, f, kMaxFields);
  Pflaa a;
  CHECK(parsePflaa(f, n, a));
  CHECK(a.alarmLevel == 1 && a.relNorthM == -1234 && a.relEastM == 4321 && a.relVertM == 220);
  CHECK(a.id == 0xDD8F1B && a.idType == 2 && a.hasTrack && a.hasSpeed && a.hasClimb && a.acType == 1);

  // Assembler: garbage, resync, CRLF, overflow
  LineAssembler as;
  const char* in = "xx\r\n$GPRMC,partial$PGRMZ,3300,f,3*";
  int lines = 0;
  for (const char* p = in; *p; ++p) lines += as.push((uint8_t)*p) ? 1 : 0;
  CHECK(lines == 0);
  CHECK(!as.push('1') && !as.push('2'));
  CHECK(as.push('\r'));
  CHECK(strcmp(as.line(), "$PGRMZ,3300,f,3*12") == 0);
  CHECK(!as.push('\n'));
  as.push('$');
  for (int i = 0; i < 300; ++i) as.push('A');
  CHECK(!as.push('\n'));
  CHECK(as.overflows() == 1);
  as.push('$');
  as.push('X');
  CHECK(as.push('\n') && strcmp(as.line(), "$X") == 0);
}

static void testGdl90Basics() {
  using namespace gdl90;
  const uint8_t* s = (const uint8_t*)"123456789";
  (void)s;
  // The GDL90 CRC is NOT CRC-16/XMODEM (that would give 50 DA for the heartbeat below).
  // Reference: example heartbeat of the GDL90 ICD, CRC 0x8BB3 (sent lsb first: B3 8B).
  const uint8_t hb[] = {0x00, 0x81, 0x41, 0xDB, 0xD0, 0x08, 0x02};
  uint8_t out[kMaxFrame];
  size_t n = frame(hb, sizeof hb, out, sizeof out);
  const uint8_t expect[] = {0x7E, 0x00, 0x81, 0x41, 0xDB, 0xD0, 0x08, 0x02, 0xB3, 0x8B, 0x7E};
  CHECK(n == sizeof expect && memcmp(out, expect, n) == 0);

  // byte stuffing of 0x7E / 0x7D
  const uint8_t st[] = {0x00, 0x7E, 0x7D, 0x01};
  n = frame(st, sizeof st, out, sizeof out);
  Decoded d = decode(out, n);
  CHECK(d.crcOk && d.payload.size() == 4 && d.payload[1] == 0x7E && d.payload[2] == 0x7D);
  CHECK(frame(st, sizeof st, out, 5) == 0);  // too small

  uint8_t p[40];
  size_t len = encodeHeartbeat(p, true, true, 86399);
  CHECK(len == 7 && p[1] == 0x91 && p[2] == 0x81);  // GPS valid + addr talkback + UAT init; 86399 s needs timestamp bit 16
  CHECK(p[1] == 0x91);
  uint32_t ts = p[3] | (p[4] << 8) | ((p[2] >> 7) << 16);
  CHECK(ts == 86399);
  len = encodeHeartbeat(p, false, false, 70000);  // > 65535 uses timestamp bit 16
  ts = p[3] | (p[4] << 8) | ((p[2] >> 7) << 16);
  CHECK(ts == 70000 && p[1] == 0x11 && (p[2] & 1) == 0);

  Report r;
  memset(&r, 0, sizeof r);
  r.alert = true;
  r.addrType = 1;
  r.addr = 0xABCDEF;
  r.posValid = true;
  r.lat = -33.8688;
  r.lon = 151.2093;
  r.altValid = true;
  r.altFt = 4500;
  r.airborne = true;
  r.trackValid = true;
  r.trackDeg = 90.0f;
  r.nic = 9;
  r.nacp = 10;
  r.hvValid = true;
  r.hvKt = 85.4f;
  r.vvValid = true;
  r.vvFpm = -640.0f;
  r.emitter = 1;
  strcpy(r.callsign, "d-abcd");
  uint8_t rep[kReportLen];
  CHECK(encodeReport(kIdTraffic, r, rep) == kReportLen);
  CHECK(rep[0] == 0x14 && rep[1] == 0x11 && rep[2] == 0xAB && rep[3] == 0xCD && rep[4] == 0xEF);
  CHECK_NEAR(decLat24(rep + 5), -33.8688, 180.0 / 8388608.0);
  CHECK_NEAR(decLat24(rep + 8), 151.2093, 180.0 / 8388608.0);
  uint16_t alt = (rep[11] << 4) | (rep[12] >> 4);
  CHECK(alt == (4500 + 1000) / 25);
  CHECK((rep[12] & 0xF) == (8 | 1));
  CHECK(rep[13] == 0x9A);
  uint16_t hv = (rep[14] << 4) | (rep[15] >> 4);
  int vv = ((rep[15] & 0xF) << 8) | rep[16];
  if (vv & 0x800) vv -= 0x1000;
  CHECK(hv == 85 && vv == -10);
  CHECK(rep[17] == 64);
  CHECK(rep[18] == 1 && memcmp(rep + 19, "D-ABCD  ", 8) == 0 && rep[27] == 0);

  // invalid markers
  memset(&r, 0, sizeof r);
  encodeReport(kIdOwnship, r, rep);
  CHECK(rep[11] == 0xFF && (rep[12] >> 4) == 0xF);                        // altitude invalid
  CHECK(rep[14] == 0xFF && (rep[15] >> 4) == 0xF);                        // speed invalid
  CHECK(((rep[15] & 0xF) << 8 | rep[16]) == 0x800);                        // vertical rate invalid
  CHECK(rep[5] == 0 && rep[10] == 0 && rep[13] == 0);                      // pos invalid, NIC 0

  // geometric altitude and ForeFlight ID
  len = encodeOwnshipGeoAlt(p, 5000, 10);
  CHECK(len == 5 && p[0] == 0x0B && ((p[1] << 8) | p[2]) == 1000 && p[3] == 0x00 && p[4] == 0x0A);
  len = encodeOwnshipGeoAlt(p, -100, 0x7FFF);
  CHECK((int16_t)((p[1] << 8) | p[2]) == -20 && p[3] == 0x7F && p[4] == 0xFF);
  len = encodeOwnshipGeoAlt(p, 0, 0xFFFF);  // clamped, bit 15 (vertical warning) never set
  CHECK((p[3] & 0x80) == 0);

  // Stratux heartbeat: 0xCC + flags (bit1 = GPS valid)
  len = encodeStratuxHeartbeat(p, true);
  CHECK(len == 2 && p[0] == 0xCC && p[1] == 0x06);  // protocol version 1 (0x04) + GPS valid (0x02), as Stratux
  len = encodeStratuxHeartbeat(p, false);
  CHECK(len == 2 && p[0] == 0xCC && p[1] == 0x04);
  n = frame(p, len, out, sizeof out);
  CHECK(n == 6 && out[0] == 0x7E && out[1] == 0xCC && out[n - 1] == 0x7E && decode(out, n).crcOk);
  len = encodeForeFlightId(p, "GS-TRAF", "GS-Traffic2WIFI");
  CHECK(len == 39 && p[0] == 0x65 && p[2] == 1 && memcmp(p + 11, "GS-TRAF", 7) == 0 && p[38] == 1);
  n = frame(p, len, out, sizeof out);
  CHECK(n > 0 && decode(out, n).crcOk);
}

struct SimCtx {
  NavState* nav;
  uint32_t now;
};
static void simLine(const char* line, void* c) {
  SimCtx* s = (SimCtx*)c;
  s->nav->ingestLine(line, s->now);
}

static void testEndToEnd() {
  using namespace gdl90;
  NavState nav;
  Simulator sim;
  Publisher pub;
  PublisherConfig cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.traffic = true;
  cfg.stratuxHb = true;
  cfg.foreFlightId = true;
  cfg.ownAddr = 0xF00000;
  cfg.ownAddrType = 1;
  strcpy(cfg.callsign, "GSTRAFFC");
  cfg.emitter = 1;
  pub.configure(cfg);

  SimCtx ctx = {&nav, 0};
  int hb = 0, own = 0, geo = 0, ff = 0, stx = 0, trafA = 0, trafB = 0, trafAll = 0, alerts = 0;
  bool ledRed = false, ledOrangeAtStart = false;
  uint32_t t0 = 4000000000u;  // close to uint32 wrap on purpose
  for (uint32_t ms = 0; ms < 60000; ms += 10) {
    uint32_t now = t0 + ms;
    ctx.now = now;
    if (ms == 0) ledOrangeAtStart = computeLedState(nav, now, 2, true, 5000, 5000) == 3;  // before any data
    sim.tick(now, simLine, &ctx);
    g_frames.clear();
    pub.tick(now, nav, collect, nullptr);
    if (ms > 1000 && computeLedState(nav, now, 2, true, 5000, 5000) == 2) ledRed = true;
    for (size_t i = 0; i < g_frames.size(); ++i) {
      Decoded d = decode(g_frames[i].data(), g_frames[i].size());
      CHECK(d.crcOk);
      switch (d.payload[0]) {
        case kIdHeartbeat: ++hb; break;
        case kIdOwnship: {
          ++own;
          if (ms > 3000) {
            CHECK_NEAR(decLat24(&d.payload[5]), 54.2166, 0.03);
            CHECK(d.payload[13] >> 4 == 8);  // Stratux profile: NIC fixed 8 with a valid position
          }
          break;
        }
        case kIdOwnshipGeoAlt: ++geo; break;
        case kIdForeFlight: ++ff; break;
        case kIdStratux:
          ++stx;
          if (ms > 3000) CHECK(d.payload.size() == 2 && d.payload[1] == 0x06);  // GPS valid in simulation
          break;
        case kIdTraffic: {
          uint32_t id = (d.payload[2] << 16) | (d.payload[3] << 8) | d.payload[4];
          ++trafAll;
          if (id == 0xDD1234) ++trafA;
          if (id == 0xDD5678) ++trafB;
          if (d.payload[1] >> 4) ++alerts;
          break;
        }
        default: CHECK(false);
      }
    }
  }
  CHECK(hb >= 59 && hb <= 61);
  CHECK(own == hb && ff == hb && stx == hb);
  CHECK(geo >= 58);
  CHECK(trafA >= 55 && trafA <= 62);
  CHECK(trafB >= 55 && trafB <= 62);
  CHECK(alerts >= 4);  // target B alarm lasts ~6 s; three more aircraft carry permanent alarms
  CHECK(ledRed);
  CHECK(ledOrangeAtStart);  // before first sentence: no position yet -> orange
  CHECK(nav.stats.badChecksum == 0 && nav.stats.parseErrors == 0 && nav.stats.noChecksum == 0);
  CHECK(nav.flarmSeen && nav.posFresh(t0 + 59990));
  CHECK(nav.gdl.valid && nav.gdl.hbGps && nav.gdl.hbUtc && nav.gdl.ownPos && nav.gdl.nic == 8 && nav.gdl.geoValid);
  CHECK(nav.gdl.hbCount == (uint32_t)hb && nav.gdl.trafficCount == (uint32_t)trafAll);
  CHECK(trafAll >= 6 * 55);  // six simulated aircraft
  printf("  frames: hb=%d own=%d geo=%d ff=%d stx=%d trafA=%d trafB=%d alerts=%d total=%u\n", hb, own, geo, ff, stx, trafA, trafB,
         alerts, (unsigned)pub.framesSent());

  // data stops: position and traffic must go stale
  g_frames.clear();
  uint32_t later = t0 + 59990 + 10000;
  pub.tick(later, nav, collect, nullptr);
  bool gotHb = false;
  for (size_t i = 0; i < g_frames.size(); ++i) {
    Decoded d = decode(g_frames[i].data(), g_frames[i].size());
    if (d.payload[0] == kIdStratux) CHECK(d.payload[1] == 0x04);  // GPS flag cleared when data stops, version bits stay
    if (d.payload[0] == kIdHeartbeat) {
      gotHb = true;
      CHECK((d.payload[1] & 0x80) == 0);  // GPS invalid flag
      CHECK((d.payload[2] & 0x01) == 0);  // UTC not ok
    }
    CHECK(d.payload[0] != kIdTraffic);
  }
  CHECK(gotHb);
  CHECK(nav.traffic.count() == 0);
}

static void testStratuxOptionOff() {
  gdl90::Publisher pub;
  gdl90::PublisherConfig cfg;
  memset(&cfg, 0, sizeof cfg);
  strcpy(cfg.callsign, "X");
  pub.configure(cfg);  // stratuxHb = false
  NavState nav;
  g_frames.clear();
  pub.tick(1000, nav, collect, nullptr);
  bool anyStx = false, anyHb = false;
  for (size_t i = 0; i < g_frames.size(); ++i) {
    Decoded d = decode(g_frames[i].data(), g_frames[i].size());
    anyStx = anyStx || d.payload[0] == gdl90::kIdStratux;
    anyHb = anyHb || d.payload[0] == gdl90::kIdHeartbeat;
  }
  CHECK(anyHb && !anyStx);
}

static void testStandardProfile() {
  // standard GDL90 behaviour: one cycle per second, NIC 8 like Stratux, ForeFlight ID only when enabled
  for (int ff_on = 0; ff_on < 2; ++ff_on) {
    NavState nav;
    Simulator sim;
    gdl90::Publisher pub;
    gdl90::PublisherConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.stratuxHb = true;
    cfg.foreFlightId = (ff_on == 1);
    strcpy(cfg.callsign, "GSTRFC");
    pub.configure(cfg);
    SimCtx ctx = {&nav, 0};
    int hb = 0, own = 0, ff = 0, badAcc = 0;
    for (uint32_t ms = 0; ms < 30000; ms += 10) {
      ctx.now = ms;
      sim.tick(ms, simLine, &ctx);
      g_frames.clear();
      pub.tick(ms, nav, collect, nullptr);
      for (size_t i = 0; i < g_frames.size(); ++i) {
        Decoded d = decode(g_frames[i].data(), g_frames[i].size());
        CHECK(d.crcOk);
        if (d.payload[0] == gdl90::kIdHeartbeat) ++hb;
        if (d.payload[0] == gdl90::kIdForeFlight) ++ff;
        if (d.payload[0] == gdl90::kIdOwnship) {
          ++own;
          if (ms > 2000 && (d.payload[13] >> 4) != 8) ++badAcc;
        }
      }
    }
    CHECK(hb >= 29 && hb <= 31 && own == hb);  // 30 s, once per second
    CHECK(ff_on ? (ff == hb) : (ff == 0));
    CHECK(badAcc == 0);
  }
}

static void testChecksumPolicy() {
  NavState n;
  const char* noCs = "$GPRMC,120000,A,5412.0000,N,00936.0000,E,54.0,90.0,010125,,,A";
  n.ingestLine(noCs, 1000);
  CHECK(n.stats.noChecksum == 1 && n.rej[0][0] == 0);       // accepted by default
  n.ingestLine("$GPRMC,1*00", 1000);
  CHECK(n.stats.badChecksum == 1 && n.rej[0][0] == 'B');    // bad is always rejected
  n.requireChecksum = true;
  n.ingestLine(noCs, 1000);
  CHECK(n.rej[1][0] == 'N');
  n.ingestLine("$GP<b>\"x*00", 1000);
  bool clean = true;
  for (int i = 0; i < NavState::kRejKeep; ++i)
    for (const char* p = n.rej[i]; *p; ++p) if (*p == '<' || *p == '>' || *p == '"' || *p == '&') clean = false;
  CHECK(clean);
}

static void testHeartbeatBytes() {
  uint8_t o[16];
  size_t n = gdl90::encodeHeartbeat(o, true, true, 43333);
  CHECK(n == 7 && o[1] == 0x91 && o[2] == 0x01 && o[3] == 0x45 && o[4] == 0xA9 && o[5] == 0 && o[6] == 0);
  n = gdl90::encodeHeartbeat(o, false, false, 1);
  CHECK(o[1] == 0x11 && (o[2] & 1) == 0);
  n = gdl90::encodeStratuxHeartbeat(o, true);
  CHECK(n == 2 && o[0] == 0xCC && o[1] == 0x06);
  n = gdl90::encodeStratuxHeartbeat(o, false);
  CHECK(o[1] == 0x04);
}

static void testUtcFlag() {
  for (int inv = 0; inv < 2; ++inv) {
    NavState nav;
    Simulator sim;
    gdl90::Publisher pub;
    gdl90::PublisherConfig cfg;
    memset(&cfg, 0, sizeof cfg);
      cfg.utcInvalid = (inv == 1);
    strcpy(cfg.callsign, "T");
    pub.configure(cfg);
    SimCtx ctx = {&nav, 0};
    int hb = 0, flagOk = 0;
    for (uint32_t ms = 0; ms < 8000; ms += 10) {
      ctx.now = ms;
      sim.tick(ms, simLine, &ctx);
      g_frames.clear();
      pub.tick(ms, nav, collect, nullptr);
      for (size_t i = 0; i < g_frames.size(); ++i) {
        Decoded d = decode(g_frames[i].data(), g_frames[i].size());
        if (d.payload[0] == gdl90::kIdHeartbeat && ms > 3000) {
          ++hb;
          bool utcBit = (d.payload[2] & 1) != 0;
          if (utcBit == (inv == 0)) ++flagOk;
        }
      }
    }
    CHECK(hb > 3 && hb == flagOk);
  }
}

static void testCrcAgainstRealDevice() {
  // Frames captured from the working 4.0 alpha (SkyDemon accepts them): their CRCs must equal ours.
  const uint8_t cc[] = {0xCC, 0x02};
  CHECK(gdl90::crc16(cc, 2) == 0xCC02);
  const uint8_t geo[] = {0x0B, 0x01, 0x90, 0x00, 0x0A};
  CHECK(gdl90::crc16(geo, 5) == 0x4073);
  const uint8_t hb[] = {0x00, 0x81, 0x01, 0x7E, 0xD5, 0x08, 0x02};
  CHECK(gdl90::crc16(hb, 7) == 0x2430);
  const uint8_t own[] = {0x0A, 0x01, 0xF0, 0x00, 0x00, 0x26, 0x8C, 0x17, 0x06, 0xD0, 0xC9, 0x07, 0x89, 0xA9,
                         0x02, 0xB8, 0x00, 0xB8, 0x01, 0x47, 0x53, 0x54, 0x52, 0x46, 0x43, 0x00, 0x00, 0x00};
  CHECK(gdl90::crc16(own, 28) == 0xAB86);
  // ICD example heartbeat
  const uint8_t icd[] = {0x00, 0x81, 0x41, 0xDB, 0xD0, 0x08, 0x02};
  CHECK(gdl90::crc16(icd, 7) == 0x8BB3);
}

static void testRegions() {
  CHECK(regionCount() >= 20);
  const Region* de = findRegion("DE");
  CHECK(de && de->firstCh == 1 && de->lastCh == 13);
  const Region* us = findRegion("us");  // case-insensitive
  CHECK(us && strcmp(us->cc, "US") == 0 && us->lastCh == 11);
  CHECK(findRegion("jp") && findRegion("JP")->lastCh == 14);
  CHECK(findRegion("01") != nullptr);
  CHECK(findRegion("XX") == nullptr && findRegion("") == nullptr && findRegion("D") == nullptr && findRegion("DEU") == nullptr);
  CHECK(findRegion(nullptr) == nullptr);
  CHECK(strcmp(defaultRegion().cc, "DE") == 0);
  for (int i = 0; i < regionCount(); ++i) {
    const Region& r = regionAt(i);
    CHECK(strlen(r.cc) == 2 && r.firstCh >= 1 && r.lastCh >= r.firstCh && r.lastCh <= 14);
    for (int j = i + 1; j < regionCount(); ++j) CHECK(strcmp(r.cc, regionAt(j).cc) != 0);
  }
}

static void testState() {
  NavState nav;
  uint32_t now = 1000;
  // PFLAA without a position fix is ignored for the traffic table but still counts as an alarm
  nmea::Pflaa a;
  memset(&a, 0, sizeof a);
  a.id = 1;
  a.alarmLevel = 3;
  nav.applyPflaa(a, now);
  CHECK(nav.traffic.count() == 0 && nav.alarmLevel == 3);

  // table eviction: oldest goes first when full
  nmea::Rmc r;
  memset(&r, 0, sizeof r);
  r.valid = r.hasPos = true;
  r.lat = 50;
  r.lon = 8;
  nav.applyRmc(r, now);
  for (uint32_t i = 0; i < TrafficTable::kMax + 5; ++i) {
    a.id = 100 + i;
    a.alarmLevel = 0;
    nav.applyPflaa(a, now + i);
  }
  CHECK(nav.traffic.count() == TrafficTable::kMax);
  bool has100 = false, has144 = false;
  for (int i = 0; i < TrafficTable::kMax; ++i) {
    if (nav.traffic.slot(i).used && nav.traffic.slot(i).id == 100) has100 = true;
    if (nav.traffic.slot(i).used && nav.traffic.slot(i).id == 144) has144 = true;
  }
  CHECK(!has100 && has144);

  // relative -> absolute position (500 m north, 500 m east of 50N 8E)
  NavState n2;
  n2.applyRmc(r, now);
  nmea::Pflaa b;
  memset(&b, 0, sizeof b);
  b.id = 7;
  b.relNorthM = 500;
  b.relEastM = 500;
  n2.applyPflaa(b, now);
  const Target* t = nullptr;
  for (int i = 0; i < TrafficTable::kMax; ++i)
    if (n2.traffic.slot(i).used) t = &n2.traffic.slot(i);
  CHECK(t != nullptr);
  if (t) {
    CHECK_NEAR(t->lat, 50.0 + 500 / 111132.0, 1e-6);
    CHECK_NEAR(t->lon, 8.0 + 500 / (111320.0 * cos(50 * M_PI / 180)), 1e-6);
    CHECK_NEAR(t->distM, 707.1, 0.2);
    CHECK_NEAR(t->bearingDeg, 45.0, 0.01);
    CHECK(!t->altValid);  // no altitude reference known
  }

  // invalid RMC drops position immediately; LED logic
  r.valid = false;
  n2.applyRmc(r, now + 100);
  CHECK(!n2.posFresh(now + 100));
  CHECK(computeLedState(n2, now + 100, 0, true, 5000, 5000) == 3);
  CHECK(computeLedState(n2, now + 100, 0, false, 5000, 5000) == 0);

  // alarm threshold + hold time
  nmea::Pflau u;
  memset(&u, 0, sizeof u);
  u.alarmLevel = 1;
  n2.applyPflau(u, 2000);
  CHECK(computeLedState(n2, 2100, 2, false, 5000, 5000) == 0);
  CHECK(computeLedState(n2, 2100, 1, false, 5000, 5000) == 2);
  CHECK(computeLedState(n2, 2000 + 5001, 1, false, 5000, 5000) == 0);
}


// Robustness: random and mutated input must never crash (run under ASan/UBSan) and the
// publisher must keep producing CRC-valid frames afterwards.
struct FuzzCtx { std::vector<std::string>* lines; };
static void fuzzCollect(const char* line, void* c) { ((FuzzCtx*)c)->lines->push_back(line); }
static void testFuzz() {
  std::vector<std::string> seeds;
  FuzzCtx fc = {&seeds};
  Simulator sim;
  for (uint32_t t = 0; t < 3000; t += 1000) sim.tick(t, fuzzCollect, &fc);
  srand(12345);
  NavState nav;
  nmea::LineAssembler as;
  gdl90::Publisher pub;
  gdl90::PublisherConfig cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.traffic = true;
  strcpy(cfg.callsign, "FUZZ");
  pub.configure(cfg);
  uint32_t now = 0;
  long accepted = 0;
  for (int iter = 0; iter < 60000; ++iter) {
    std::string in;
    if (iter % 3 == 0) {
      for (int i = 0, n = rand() % 200; i < n; ++i) in += (char)(rand() & 0xFF);
    } else {
      in = seeds[rand() % seeds.size()];
      for (int m = rand() % 6; m > 0 && !in.empty(); --m) {
        size_t p = rand() % in.size();
        switch (rand() % 4) {
          case 0: in[p] = (char)(rand() & 0xFF); break;
          case 1: in.erase(p, 1 + rand() % 4); break;
          case 2: in.insert(p, 1, ",*$"[rand() % 3]); break;
          default: in.resize(p); break;
        }
      }
      // sometimes re-seal with a valid checksum so the parsers really see the garbage
      if (rand() % 2 && in.size() > 2 && in[0] == '$') {
        size_t star = in.find('*');
        if (star != std::string::npos) in.resize(star);
        char buf[256];
        snprintf(buf, sizeof buf, "%s", in.c_str());
        nmea::appendChecksum(buf, sizeof buf);
        in = buf;
      }
    }
    in += "\r\n";
    for (size_t i = 0; i < in.size(); ++i)
      if (as.push((uint8_t)in[i])) accepted += nav.ingestLine(as.line(), now) != nmea::Type::Unknown;
    now += 7;
    g_frames.clear();
    pub.tick(now, nav, collect, nullptr);
    for (size_t i = 0; i < g_frames.size(); ++i) CHECK(decode(g_frames[i].data(), g_frames[i].size()).crcOk);
  }
  CHECK(accepted > 1000);
  printf("  fuzz: %ld sentences accepted, %u overflows\n", accepted, (unsigned)as.overflows());
}

static void testQr() {
  char t[160];
  CHECK(qr::wifiString("GS-Traffic-a1b2", "c3d4a1b2", t, sizeof t));
  CHECK(strcmp(t, "WIFI:T:WPA;S:GS-Traffic-a1b2;P:c3d4a1b2;;") == 0);
  CHECK(qr::wifiString("a;b,c:d\\e\"f", "k", t, sizeof t));
  CHECK(strcmp(t, "WIFI:T:WPA;S:a\\;b\\,c\\:d\\\\e\\\"f;P:k;;") == 0);
  CHECK(!qr::wifiString("0123456789012345678901234567890123", "k", t, 20));  // buffer too small
  qr::Code c;
  CHECK(qr::encode("WIFI:T:WPA;S:GS-Traffic-a1b2;P:c3d4a1b2;;", c) && c.size == 29);  // version 3
  CHECK(c.m[0][0] == 1 && c.m[0][6] == 1 && c.m[1][1] == 0 && c.m[2][2] == 1);          // finder pattern
  CHECK(!qr::encode("0123456789012345678901234567890123456789012345678901234567890123456789"
                    "0123456789012345678901234567890123456789012345678901234567890123456789", c));  // > 134 bytes
}

static void testDialAltitude() {
  // Real case from a flight test: 20 ft elevation, QNH far above 1013 hPa -> pressure altitude -543 ft.
  NavState n;
  uint32_t now = 10000;
  n.ingestLine("$GPRMC,123456.00,A,5230.0000,N,01330.0000,E,0.0,0.0,011125,,,A", now);
  n.ingestLine("$GPGGA,123456.00,5230.0000,N,01330.0000,E,1,9,1.1,6.1,M,45.0,M,,", now);
  n.ingestLine("$PGRMZ,-543,f,3", now);
  int32_t ft = 0;
  bool pressure = true;
  CHECK(DialUi::displayAltitudeFt(n, now + 500, ft, pressure));
  CHECK(!pressure && ft >= 19 && ft <= 21);  // the GPS altitude is shown, not the pressure altitude
  // GGA gone stale -> fall back to the pressure altitude
  NavState m;
  m.ingestLine("$PGRMZ,-543,f,3", now);
  CHECK(DialUi::displayAltitudeFt(m, now + 500, ft, pressure));
  CHECK(pressure && ft == -543);
  CHECK(!DialUi::displayAltitudeFt(m, now + 60000, ft, pressure));
}

static void testDial() {
  Settings st;
  memset(&st, 0, sizeof st);
  st.dialBright = 80; st.ringBright = 30; st.nmeaTcp = true; st.gdl90 = true;
  DialUi ui;
  ui.begin(st, 0);
  // wheel changes pages and is clamped at both ends
  CHECK(ui.page() == DialUi::kFlight);
  ui.onRotate(-1);
  CHECK(ui.page() == DialUi::kFlight);
  for (int i = 0; i < DialUi::kMenu; ++i) ui.onRotate(1);
  CHECK(ui.page() == DialUi::kMenu);
  // the wheel must be pressed once to open the menu; turning only leaves the page
  CHECK(!ui.menuOpen());
  ui.onClick();
  CHECK(ui.menuOpen() && !ui.editing());
  // menu: Display brightness edit
  ui.onClick();
  CHECK(ui.editing());
  ui.onRotate(-1);
  ui.onClick();
  CHECK(!ui.editing());
  CHECK(ui.settings().dialBright == 70);
  // toggling an output requires a reboot, brightness alone only a save
  ui.onRotate(1);  // LED ring
  ui.onClick();
  CHECK(ui.settings().ring);
  ui.onRotate(1); ui.onRotate(1); ui.onRotate(1);  // NMEA TCP
  ui.onClick();
  CHECK(!ui.settings().nmeaTcp);
  ui.onLongPress();  // leaves the menu
  CHECK(ui.takeAction() == kDialSaveReboot);
  // brightness only
  DialUi u2;
  u2.begin(st, 0);
  u2.gotoPage(DialUi::kMenu);
  u2.onClick(); u2.onClick(); u2.onRotate(1); u2.onClick();
  u2.onLongPress();
  CHECK(u2.takeAction() == kDialSave);
  CHECK(u2.settings().dialBright == 90);
  // start page: the UI begins on it, the menu changes it and saves without a reboot
  DialUi u5;
  Settings sp = st;
  sp.startPage = 2;
  u5.begin(sp, 0);
  CHECK(u5.page() == DialUi::kRadar);
  u5.gotoPage(DialUi::kMenu);
  u5.onClick();                                   // open
  for (int i = 0; i < 3; ++i) u5.onRotate(1);     // Start page
  u5.onClick();                                   // edit
  u5.onRotate(1); u5.onRotate(1);
  u5.onClick();
  CHECK(u5.settings().startPage == 4);
  u5.onLongPress();
  CHECK(u5.takeAction() == kDialSave);
  // restart item: saves and reboots without a confirmation
  DialUi u6;
  u6.begin(st, 0);
  u6.gotoPage(DialUi::kMenu);
  u6.onClick();
  for (int i = 0; i < 7; ++i) u6.onRotate(1);
  u6.onClick();
  CHECK(u6.takeAction() == kDialSaveReboot);
  // factory reset needs the confirmation and defaults to Cancel
  DialUi u3;
  u3.begin(st, 0);
  u3.gotoPage(DialUi::kMenu);
  u3.onClick();
  for (int i = 0; i < 8; ++i) u3.onRotate(1);
  u3.onClick();
  CHECK(u3.confirmState() == 1);
  u3.onClick();
  CHECK(u3.takeAction() == kDialNone && u3.confirmState() == 0);
  u3.onClick();
  u3.onRotate(1);
  CHECK(u3.confirmState() == 2);
  u3.onClick();
  CHECK(u3.takeAction() == kDialFactoryReset);
  // rendering never writes outside the band buffer (guard words around it)
  static uint16_t mem[240 * 30 + 64];
  for (int i = 0; i < 64; ++i) mem[i] = mem[240 * 30 + i] = 0xABCD;
  NavState n;
  uint32_t now = 10000;
  n.ingestLine("$GPRMC,123456.00,A,5230.0000,N,01330.0000,E,95.0,87.0,011125,,,A", now);
  n.ingestLine("$PFLAA,2,-800,900,-120,1,AB12CD,270,,95,2.1,8", now);
  n.ingestLine("$PFLAA,0,90000,60000,1000,1,778899,200,,60,0,1", now);
  DialEnv env;
  memset(&env, 0, sizeof env);
  strcpy(env.ssid, "S"); strcpy(env.key, "K"); strcpy(env.ip, "1.2.3.4"); env.fw = "x"; env.hw = "3.0";
  DialUi u4;
  u4.begin(st, 0);
  gfx::Canvas c;
  for (int pg = 0; pg < DialUi::kPageCount; ++pg) {
    u4.gotoPage(pg);
    for (int y0 = 0; y0 < 240; y0 += 30) {
      c.setBand(mem + 32, y0, 30);
      u4.render(c, n, env, now + 3000);
    }
  }
  bool guard = true;
  for (int i = 0; i < 32; ++i) guard = guard && mem[i] == 0xABCD && mem[32 + 240 * 30 + i] == 0xABCD;
  CHECK(guard);
  // LED ring: alarm target dead ahead lights LED 1 in red, status LED 0 mirrors the status
  RingFrame rf;
  NavState m;
  m.ingestLine("$GPRMC,123456.00,A,5230.0000,N,01330.0000,E,95.0,0.0,011125,,,A", now);
  m.ingestLine("$PFLAA,3,1000,0,0,1,AB12CD,0,,95,0,8", now);
  computeRing(m, now, 2, rf);
  CHECK(rf.rgb[0][0] == 255 && rf.rgb[0][1] == 0);
  CHECK(rf.rgb[1][0] == 255 && rf.rgb[1][1] == 0);
  // levels 1 and 2 east/south, a target without alarm must not light anything
  NavState q;
  q.ingestLine("$GPRMC,123456.00,A,5230.0000,N,01330.0000,E,95.0,0.0,011125,,,A", now);
  q.ingestLine("$PFLAA,1,0,1000,0,1,000001,0,,95,0,8", now);   // east  = LED 7, level 1 yellow
  q.ingestLine("$PFLAA,2,-1000,0,0,1,000002,0,,95,0,8", now);  // south = LED 13, level 2 orange
  q.ingestLine("$PFLAA,0,500,0,0,1,000003,0,,95,0,8", now);    // no alarm
  computeRing(q, now, 0, rf);
  CHECK(rf.rgb[7][0] == 200 && rf.rgb[7][1] == 200 && rf.rgb[7][2] == 0);
  CHECK(rf.rgb[13][0] == 255 && rf.rgb[13][1] == 110);
  int lit = 0;
  for (int i = 1; i <= 24; ++i) lit += (rf.rgb[i][0] | rf.rgb[i][1] | rf.rgb[i][2]) ? 1 : 0;
  CHECK(lit == 2);
}

int main() {
  testDialAltitude();
  testQr();
  testDial();
  testNmea();
  testGdl90Basics();
  testRegions();
  testStandardProfile();
  testChecksumPolicy();
  testHeartbeatBytes();
  testUtcFlag();
  testCrcAgainstRealDevice();
  testStratuxOptionOff();
  testState();
  testEndToEnd();
  testFuzz();
  printf("%d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
