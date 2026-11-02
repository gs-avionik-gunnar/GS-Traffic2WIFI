// GDL90 (Garmin/FAA "Data Interface Specification" 560-1058-00) encoder and publisher.
// Pure C++ (no Arduino dependencies).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "State.h"

namespace gdl90 {

enum {
  kIdHeartbeat = 0x00,
  kIdOwnship = 0x0A,
  kIdOwnshipGeoAlt = 0x0B,
  kIdTraffic = 0x14,
  kIdForeFlight = 0x65,
  kIdStratux = 0xCC,  // non-standard "Stratux heartbeat", used by apps to recognise a Stratux-style device
  kReportLen = 28,
  kMaxFrame = 96,  // worst case: ForeFlight ID message with every byte escaped
};

// CRC-16 as defined by the GDL90 ICD: crc = Table[crc >> 8] ^ (crc << 8) ^ byte (polynomial 0x1021, init 0).
// Not CRC-16/XMODEM, see Gdl90.cpp.
uint16_t crc16(const uint8_t* data, size_t len);

// Wraps payload into 0x7E | escaped(payload + CRC lsb,msb) | 0x7E.
// Returns the frame length, 0 if `cap` is too small.
size_t frame(const uint8_t* payload, size_t len, uint8_t* out, size_t cap);

struct Report {
  bool alert;
  uint8_t addrType;  // 0 ADS-B ICAO, 1 ADS-B self-assigned, ...
  uint32_t addr;     // 24 bit
  bool posValid;
  double lat, lon;
  bool altValid;
  int32_t altFt;     // pressure altitude
  bool airborne;
  bool extrapolated;
  bool trackValid;
  float trackDeg;
  uint8_t nic, nacp;
  bool hvValid;
  float hvKt;
  bool vvValid;
  float vvFpm;
  uint8_t emitter;
  char callsign[9];
  uint8_t emergency;
};

size_t encodeHeartbeat(uint8_t* out, bool gpsValid, bool utcOk, uint32_t secOfDay);
size_t encodeReport(uint8_t msgId, const Report& r, uint8_t* out);  // writes kReportLen bytes
// vfomM: vertical figure of merit in metres, 0x7FFF = not available
size_t encodeOwnshipGeoAlt(uint8_t* out, int32_t altFt, uint16_t vfomM);
// Stratux heartbeat: 0xCC + flags as in Stratux (protocol version 1 in bits 7..2, bit1 = GPS valid, bit0 = AHRS).
size_t encodeStratuxHeartbeat(uint8_t* out, bool gpsValid);
size_t encodeForeFlightId(uint8_t* out, const char* shortName, const char* longName);

struct PublisherConfig {
  bool traffic;
  bool stratuxHb;  // also send the Stratux heartbeat (0xCC) next to every GDL90 heartbeat
  bool foreFlightId;     // send the ForeFlight ID message (0x65)
  bool utcInvalid;       // heartbeat: UTC-valid flag cleared (set while the simulator runs: its clock is not real time)
  uint32_t ownAddr;
  uint8_t ownAddrType;
  char callsign[9];
  uint8_t emitter;
};

typedef void (*FrameSink)(const uint8_t* frame, size_t len, void* ctx);

// Produces the periodic GDL90 stream: heartbeat, ForeFlight ID, ownship and
// ownship geometric altitude once per second; traffic reports paced evenly so
// each target is sent about once per second.
class Publisher {
 public:
  Publisher() : started_(false), lastSecondMs_(0), lastTrafficMs_(0), cursor_(0), airborne_(false), frames_(0) {
    cfg_ = PublisherConfig();
  }
  void configure(const PublisherConfig& c) { cfg_ = c; }
  void tick(uint32_t now, NavState& nav, FrameSink sink, void* ctx);
  uint32_t framesSent() const { return frames_; }

 private:
  void emit(const uint8_t* payload, size_t len, FrameSink sink, void* ctx);
  void emitSecond(uint32_t now, NavState& nav, FrameSink sink, void* ctx);
  void emitTraffic(uint32_t now, NavState& nav, FrameSink sink, void* ctx);

  PublisherConfig cfg_;
  bool started_;
  uint32_t lastSecondMs_, lastTrafficMs_;
  int cursor_;
  bool airborne_;
  uint32_t frames_;
};

uint8_t emitterFromFlarm(uint8_t flarmAcType);

}  // namespace gdl90
