#include "Simulator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Nmea.h"

static const double kPi = 3.14159265358979323846;

static void emitLine(Simulator::LineSink sink, void* ctx, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void emitLine(Simulator::LineSink sink, void* ctx, const char* fmt, ...) {
  char buf[nmea::kMaxLine + 1];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf - 4, fmt, ap);
  va_end(ap);
  nmea::appendChecksum(buf, sizeof buf);
  sink(buf, ctx);
}

// ddmm.mmmm / dddmm.mmmm formatting
static void fmtLat(double deg, char* out, size_t cap) {
  char h = deg >= 0 ? 'N' : 'S';
  deg = fabs(deg);
  int d = (int)deg;
  snprintf(out, cap, "%02d%07.4f,%c", d, (deg - d) * 60.0, h);
}
static void fmtLon(double deg, char* out, size_t cap) {
  char h = deg >= 0 ? 'E' : 'W';
  deg = fabs(deg);
  int d = (int)deg;
  snprintf(out, cap, "%03d%07.4f,%c", d, (deg - d) * 60.0, h);
}

void Simulator::tick(uint32_t now, LineSink sink, void* ctx) {
  if (!started_) {
    started_ = true;
    last_ = now;
    emitSet(sink, ctx);
    return;
  }
  if ((uint32_t)(now - last_) >= 1000u) {
    last_ += 1000u;
    ++t_;
    emitSet(sink, ctx);
  }
}

void Simulator::emitSet(LineSink sink, void* ctx) {
  // Ownship flies a circle (R = 3 km, 50 m/s = 97 kt) near Schleswig-Holstein.
  const double centerLat = 54.2166, centerLon = 9.6001, R = 3000.0, v = 50.0;
  double th = (v / R) * t_;
  double north = R * cos(th), east = R * sin(th);
  double lat = centerLat + north / 111132.0;
  double lon = centerLon + east / (111320.0 * cos(centerLat * kPi / 180.0));
  double trk = atan2(cos(th), -sin(th)) * 180.0 / kPi;
  if (trk < 0) trk += 360.0;

  uint32_t sod = 12 * 3600 + t_;
  int hh = (sod / 3600) % 24, mm = (sod / 60) % 60, ss = sod % 60;
  char la[24], lo[24];
  fmtLat(lat, la, sizeof la);
  fmtLon(lon, lo, sizeof lo);

  emitLine(sink, ctx, "$GPRMC,%02d%02d%02d.00,A,%s,%s,%.1f,%.1f,191026,,,A", hh, mm, ss, la, lo, v * 1.943844, trk);
  emitLine(sink, ctx, "$GPGGA,%02d%02d%02d.00,%s,%s,1,09,0.9,1000.0,M,46.0,M,,", hh, mm, ss, la, lo);
  emitLine(sink, ctx, "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,,,,1.5,0.9,1.2");
  emitLine(sink, ctx, "$PGRMZ,3300,f,3");

  // Six aircraft. A orbits the ownship, B is ahead and raises an alarm (level 2) for 6 s every 40 s.
  // Four more fly circles around the same centre; three of them carry a permanent FLARM alarm of
  // level 1, 2 and 3 (to see every alarm colour on radar, list and LED ring), one is far away.
  double ph = 0.05 * t_;
  int aN = (int)(800 * cos(ph)), aE = (int)(800 * sin(ph));
  bool alarm = (t_ % 40) >= 30 && (t_ % 40) < 36;
  int bN = 600 + (int)(100 * sin(0.1 * t_)), bE = -300;

  struct Orbit { double r, v, phase; int dir; double relVertM, climb; const char* id; int type; int alarm; };
  static const Orbit kOrbits[4] = {
      {1500.0, 40.0, 0.0, 1, 300.0, 1.5, "DD0C01", 1, 1},    // alarm level 1
      {2600.0, 45.0, 2.0, -1, -200.0, 0.0, "DD0D02", 8, 2},  // alarm level 2
      {2000.0, 35.0, 4.0, -1, 150.0, -2.0, "DD0E03", 9, 3},  // alarm level 3
      {9000.0, 70.0, 3.0, 1, 600.0, 0.5, "DD0F04", 1, 0},    // far away, no alarm
  };
  int top = alarm ? 2 : 0;
  for (int i = 0; i < 4; ++i)
    if (kOrbits[i].alarm > top) top = kOrbits[i].alarm;

  emitLine(sink, ctx, "$PFLAU,2,1,2,1,%d,0,2,%d,%d,DD5678", top, -50, (int)sqrt((double)bN * bN + (double)bE * bE));
  emitLine(sink, ctx, "$PFLAA,0,%d,%d,100,1,3FEBA7,%d,0,25,0.5,1", aN, aE, (int)fmod(ph * 180.0 / kPi + 90.0, 360.0));
  emitLine(sink, ctx, "$PFLAA,%d,%d,%d,-50,2,DD5678,90,0,30,-1.0,8", alarm ? 2 : 0, bN, bE);
  for (int i = 0; i < 4; ++i) {
    const Orbit& o = kOrbits[i];
    double phi = o.phase + o.dir * (o.v / o.r) * t_;
    double tn = o.r * cos(phi), te = o.r * sin(phi);
    double vn = -o.dir * o.v * sin(phi), ve = o.dir * o.v * cos(phi);
    double tr = atan2(ve, vn) * 180.0 / kPi;
    if (tr < 0) tr += 360.0;
    emitLine(sink, ctx, "$PFLAA,%d,%d,%d,%d,1,%s,%d,0,%d,%.1f,%d", o.alarm, (int)(tn - north), (int)(te - east), (int)o.relVertM, o.id,
             (int)tr, (int)o.v, o.climb, o.type);
  }
}
