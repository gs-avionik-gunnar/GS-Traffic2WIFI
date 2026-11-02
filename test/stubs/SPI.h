#pragma once
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings { SPISettings(uint32_t, uint8_t, uint8_t) {} };
class SPIClass {
 public:
  void begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss);
  void beginTransaction(SPISettings);
  uint8_t transfer(uint8_t);
  void writeBytes(const uint8_t*, uint32_t);
};
extern SPIClass SPI;
