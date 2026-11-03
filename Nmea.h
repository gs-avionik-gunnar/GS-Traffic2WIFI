// NMEA 0183 handling: line assembly, checksum, tokenizing and sentence parsers.
// Pure C++ (no Arduino dependencies) so it can be unit-tested on a PC.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace nmea {

enum { kMaxLine = 128, kMaxFields = 24 };

// Assembles bytes into '$'-sentences. Bounded, allocation-free, resyncs on '$'.
class LineAssembler {
 public:
  LineAssembler() : len_(0), overflow_(false), overflows_(0) { buf_[0] = '\0'; }
  // Feed one byte. Returns true when a complete line (without CR/LF) is
  // available via line(). The pointer stays valid until the next push().
  bool push(uint8_t b);
  const char* line() const { return buf_; }
  uint32_t overflows() const { return overflows_; }

 private:
  char buf_[kMaxLine + 1];
  size_t len_;
  bool overflow_;
  uint32_t overflows_;
};

enum class Checksum { Ok, Bad, Missing };
Checksum verifyChecksum(const char* line);
// Appends "*hh" to a '$...' string. Returns the new length.
size_t appendChecksum(char* line, size_t cap);

// Splits "$GPRMC,a,b*hh" in place. fields[0] = "GPRMC". Returns field count.
int split(char* line, char** fields, int maxFields);

enum class Type { Unknown, Rmc, Gga, Gsa, Pgrmz, Pflau, Pflaa };
Type classify(const char* sentenceId);

struct Rmc {
  bool valid;  // status 'A' and a parsable position
  bool hasTime;
  uint32_t secOfDay;
  bool hasPos;
  double lat, lon;
  bool hasSog;
  float sogKt;
  bool hasTrack;
  float trackDeg;
};

struct Gga {
  uint8_t quality;
  uint8_t sats;
  bool hasHdop;
  float hdop;
  bool hasAlt;
  float altMslM;
  bool hasGeoid;
  float geoidM;
};

struct Gsa {
  uint8_t fixMode;  // 1 = none, 2 = 2D, 3 = 3D, 0 = unknown
  bool hasHdop;
  float hdop;
};

struct Pgrmz {
  int32_t altFt;  // pressure altitude
};

struct Pflau {
  uint8_t rx, tx, gps;
  uint8_t alarmLevel;  // 0..3
};

struct Pflaa {
  uint8_t alarmLevel;
  int32_t relNorthM, relEastM, relVertM;
  uint8_t idType;  // 0 random, 1 ICAO, 2 FLARM
  uint32_t id;
  bool hasTrack;
  float trackDeg;
  bool hasSpeed;
  float speedMs;
  bool hasClimb;
  float climbMs;
  uint8_t acType;
};

// Parsers take the output of split(). They return false when mandatory
// fields are missing or malformed.
bool parseRmc(char** f, int n, Rmc& out);
bool parseGga(char** f, int n, Gga& out);
bool parseGsa(char** f, int n, Gsa& out);
bool parsePgrmz(char** f, int n, Pgrmz& out);
bool parsePflau(char** f, int n, Pflau& out);
bool parsePflaa(char** f, int n, Pflaa& out);

}  // namespace nmea
