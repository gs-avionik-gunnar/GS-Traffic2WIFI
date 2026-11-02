// Navigation / traffic state built from the NMEA stream.
// Pure C++ (no Arduino dependencies). Times are millis()-style uint32_t and
// compared with wrap-safe subtraction.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "Nmea.h"

enum {
  kPosTimeoutMs = 3000,     // position older than this is "not valid" for GDL90
  kBaroTimeoutMs = 5000,
  kAltTimeoutMs = 5000,
  kAccTimeoutMs = 5000,
  kTargetTimeoutMs = 6000,  // traffic dropped after this without PFLAA update
};

struct Target {
  bool used;
  uint32_t id;      // 24 bit
  uint8_t idType;   // FLARM idType from PFLAA
  uint8_t acType;   // FLARM aircraft type
  uint8_t alarm;    // 0..3
  double lat, lon;
  bool altValid;
  int32_t altFt;
  bool trackValid;
  float trackDeg;
  bool speedValid;
  float speedKt;
  bool climbValid;
  float climbFpm;
  float distM;      // distance to ownship at last update
  float bearingDeg; // true bearing from ownship
  uint32_t lastMs;
};

class TrafficTable {
 public:
  enum { kMax = 40 };
  TrafficTable() { clear(); }
  void clear();
  void update(const Target& t);  // insert or replace by id, evicts the oldest if full
  void expire(uint32_t now, uint32_t timeoutMs);
  int count() const;
  const Target& slot(int i) const { return slots_[i]; }

 private:
  Target slots_[kMax];
};

// What the GDL90 publisher last put on the air (for the WebUI "GDL90 as sent" panel).
struct Gdl90Info {
  bool valid;  // at least one 1 Hz cycle has been emitted
  bool hbGps, hbUtc;
  bool ownPos;
  double lat, lon;
  bool altValid;
  int32_t altFt;
  uint8_t nic, nacp;
  bool airborne;
  bool geoValid;
  int32_t geoFt;
  uint16_t vfom;
  uint32_t hbCount, ownCount, trafficCount;
};

struct NavStats {
  uint32_t lines;
  uint32_t badChecksum;
  uint32_t noChecksum;
  uint32_t unknown;
  uint32_t parseErrors;
};

class NavState {
 public:
  NavState() { reset(); }
  void reset();

  // Validates, parses and applies one complete '$' line. Returns the sentence type
  // (Type::Unknown if rejected or not interesting).
  nmea::Type ingestLine(const char* line, uint32_t now);

  void applyRmc(const nmea::Rmc& r, uint32_t now);
  void applyGga(const nmea::Gga& g, uint32_t now);
  void applyGsa(const nmea::Gsa& g, uint32_t now);
  void applyPgrmz(const nmea::Pgrmz& z, uint32_t now);
  void applyPflau(const nmea::Pflau& u, uint32_t now);
  void applyPflaa(const nmea::Pflaa& a, uint32_t now);

  bool posFresh(uint32_t now) const { return rmcValid && age(now, posMs) < kPosTimeoutMs; }
  bool timeFresh(uint32_t now) const { return timeValid && age(now, timeMs) < 5000; }
  bool baroFresh(uint32_t now) const { return baroValid && age(now, baroMs) < kBaroTimeoutMs; }
  bool altFresh(uint32_t now) const { return altValid && age(now, altMs) < kAltTimeoutMs; }
  bool accFresh(uint32_t now) const { return hdopValid && age(now, accMs) < kAccTimeoutMs; }

  // Reference altitude used for ownship (pressure altitude preferred, GPS MSL as fallback).
  bool refAltitudeFt(uint32_t now, int32_t& ft) const;
  // Geometric (WGS-84 ellipsoid) altitude in feet.
  bool geoAltitudeFt(uint32_t now, int32_t& ft) const;

  static uint32_t age(uint32_t now, uint32_t then) { return (uint32_t)(now - then); }

  // --- ownship / GPS
  bool rmcValid;
  uint32_t posMs;
  double lat, lon;
  bool sogValid;
  float sogKt;
  bool trackValid;
  float trackDeg;
  bool timeValid;
  uint32_t secOfDay, timeMs;
  bool altValid;
  float altMslM;
  bool geoidValid;
  float geoidM;
  uint32_t altMs;
  bool baroValid;
  int32_t baroAltFt;
  uint32_t baroMs;
  bool hdopValid;
  float hdop;
  uint32_t accMs;
  uint8_t sats;
  uint8_t fixQuality;
  uint8_t fixMode;

  // --- FLARM
  bool flarmSeen;
  uint8_t flarmRx, flarmTx, flarmGps;
  uint32_t flarmMs;
  uint8_t alarmLevel;  // level of the latest alarm-bearing message
  uint32_t alarmMs;    // time of that message (0 = never)
  bool alarmEver;

  uint32_t lastRxMs;  // last accepted sentence
  NavStats stats;
  // Strict mode: sentences without checksum are rejected. Default: accepted (bad checksums are always rejected).
  bool requireChecksum = false;
  // Last rejected lines (printable, JSON-safe) for diagnosis in the web UI.
  static const int kRejKeep = 4;
  char rej[kRejKeep][80];
  uint8_t rejNext = 0;
  void noteRejected(const char* line, char why);
  Gdl90Info gdl;
  TrafficTable traffic;
};

// Combined LED state: 0 off, 1 green, 2 red (traffic alarm), 3 orange (no position).
uint8_t computeLedState(const NavState& nav, uint32_t now, uint8_t trafficLevel, bool positionLed,
                        uint32_t trafficHoldMs, uint32_t positionHoldMs);
