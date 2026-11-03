#pragma once
#include "Arduino.h"
class WiFiClient : public Stream {
 public:
  operator bool();
  bool connected();
  void stop();
  void setNoDelay(bool);
  size_t write(const uint8_t*, size_t);
  int connect(IPAddress, uint16_t, int32_t timeout);
  int available();
  int read(uint8_t*, size_t);
};
