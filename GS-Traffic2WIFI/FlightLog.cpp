#include "FlightLog.h"

#include <math.h>
#include <string.h>

namespace flightlog {

void Log::init(Store* store, const Flight* list, int n, const Flight* current) {
  store_ = store;
  n_ = 0;
  state_ = kIdle;
  for (int i = 0; i < n && i < kMaxFlights; ++i) list_[n_++] = list[i];
  if (current && store_) {
    // power was lost while flying: keep what was stored last
    if (current->dur >= (uint32_t)kMinFlightS) add(*current);
    store_->clearCurrent();
  }
}

void Log::add(const Flight& f) {
  if (n_ == kMaxFlights) {  // drop the oldest
    memmove(&list_[0], &list_[1], sizeof(Flight) * (kMaxFlights - 1));
    --n_;
  }
  list_[n_++] = f;
  if (store_) store_->saveAll(list_, n_);
}

bool Log::removeByStart(uint32_t start) {
  for (int i = 0; i < n_; ++i) {
    if (list_[i].start != start) continue;
    memmove(&list_[i], &list_[i + 1], sizeof(Flight) * (n_ - i - 1));
    --n_;
    if (store_) store_->saveAll(list_, n_);
    return true;
  }
  return false;
}

void Log::clear() {
  n_ = 0;
  if (store_) store_->saveAll(list_, 0);
}

void Log::updateCurrent() {
  cur_.dur = lastAbove_.epoch > cur_.start ? lastAbove_.epoch - cur_.start : 0;
  cur_.eLat = lastAbove_.lat;
  cur_.eLon = lastAbove_.lon;
}

void Log::finish() {
  updateCurrent();
  if (store_) store_->clearCurrent();
  if (cur_.dur >= (uint32_t)kMinFlightS) add(cur_);
  state_ = kIdle;
}

void Log::tick(const NavState& n, uint32_t now, bool allow, uint16_t thresholdKmh) {
  if (!allow) {
    if (active()) finish();
    state_ = kIdle;
    return;
  }
  uint32_t ep;
  if (!n.posFresh(now) || !n.sogValid || !n.utcEpoch(now, ep)) return;  // no fix: keep the state
  const bool above = n.sogKt * 1.852f >= (float)thresholdKmh;
  Point p = {ep, (int32_t)lround(n.lat * 1e5), (int32_t)lround(n.lon * 1e5)};

  switch (state_) {
    case kIdle:
      if (above) {
        cand_ = p;
        since_ = now;
        state_ = kStarting;
      }
      break;
    case kStarting:
      if (!above) {
        state_ = kIdle;
      } else if ((uint32_t)(now - since_) >= (uint32_t)kStartConfirmMs) {
        memset(&cur_, 0, sizeof cur_);
        cur_.start = cand_.epoch;
        cur_.sLat = cand_.lat;
        cur_.sLon = cand_.lon;
        lastAbove_ = p;
        updateCurrent();
        if (store_) store_->saveCurrent(cur_);
        lastSave_ = now;
        state_ = kFlying;
      }
      break;
    case kFlying:
      if (above) {
        lastAbove_ = p;
        if ((uint32_t)(now - lastSave_) >= (uint32_t)kSaveEveryMs) {
          updateCurrent();
          if (store_) store_->saveCurrent(cur_);
          lastSave_ = now;
        }
      } else {
        state_ = kEnding;
        since_ = now;
      }
      break;
    case kEnding:
      if (above) {
        lastAbove_ = p;
        state_ = kFlying;
      } else if ((uint32_t)(now - since_) >= (uint32_t)kStopConfirmMs) {
        finish();
      }
      break;
  }
}

}  // namespace flightlog
