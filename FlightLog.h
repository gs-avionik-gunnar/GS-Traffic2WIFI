// Flight log: start/end time (UTC), duration and start/end position of every flight. No track.
// A flight starts when the ground speed stays above the threshold for 10 s and ends when it stays
// below for 60 s; flights shorter than 2 minutes are dropped. While flying the running flight is
// stored every minute (so a power loss right after landing still leaves a log entry).
// Pure C++ (storage is behind the Store interface), host-testable.
#pragma once
#include <stdint.h>

#include "State.h"

namespace flightlog {

struct Flight {
  uint32_t start;  // UTC, seconds since 1970
  uint32_t dur;    // seconds
  int32_t sLat, sLon;  // start position, 1e-5 degrees
  int32_t eLat, eLon;  // end position
};

class Store {
 public:
  virtual ~Store() {}
  virtual void saveCurrent(const Flight& f) = 0;  // flight in progress
  virtual void clearCurrent() = 0;
  virtual void saveAll(const Flight* list, int n) = 0;  // finished flights, oldest first
};

enum {
  kMaxFlights = 50,
  kStartConfirmMs = 10000,
  kStopConfirmMs = 60000,
  kMinFlightS = 120,
  kSaveEveryMs = 60000,
};

class Log {
 public:
  void init(Store* store, const Flight* list, int n, const Flight* current);
  // Call regularly. allow=false (disabled / simulator) ends a running flight.
  void tick(const NavState& nav, uint32_t now, bool allow, uint16_t thresholdKmh);

  int count() const { return n_; }
  const Flight& at(int i) const { return list_[n_ - 1 - i]; }  // 0 = newest
  bool active() const { return state_ == kFlying || state_ == kEnding; }
  uint32_t activeStart() const { return cur_.start; }
  bool removeByStart(uint32_t start);
  void clear();

 private:
  enum State { kIdle, kStarting, kFlying, kEnding };
  struct Point { uint32_t epoch; int32_t lat, lon; };
  void finish();
  void updateCurrent();
  void add(const Flight& f);

  Store* store_ = nullptr;
  Flight list_[kMaxFlights];
  int n_ = 0;
  State state_ = kIdle;
  Flight cur_;
  Point cand_, lastAbove_;
  uint32_t since_ = 0, lastSave_ = 0;
};

}  // namespace flightlog
