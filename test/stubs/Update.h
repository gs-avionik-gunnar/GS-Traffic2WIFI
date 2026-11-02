#pragma once
#include "Arduino.h"
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
class UpdateClass {
 public:
  bool begin(size_t size);
  size_t write(uint8_t*, size_t);
  bool end(bool evenIfRemaining = false);
  void abort();
  bool hasError();
  const char* errorString();
  void printError(Print&);
};
extern UpdateClass Update;
