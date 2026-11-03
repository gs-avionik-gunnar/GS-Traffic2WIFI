#pragma once
#include "Arduino.h"
class WiFiUDP {
 public:
  uint8_t begin(uint16_t);
  int beginPacket(IPAddress, uint16_t);
  size_t write(const uint8_t*, size_t);
  int endPacket();
};
