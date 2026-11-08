#pragma once
#include "Arduino.h"
class Preferences {
 public:
  bool begin(const char*, bool readOnly = false);
  void end();
  bool clear();
  size_t putString(const char*, const char*);
  size_t putUChar(const char*, uint8_t);
  size_t putBool(const char*, bool);
  size_t putUInt(const char*, uint32_t);
  size_t putUShort(const char*, uint16_t);
  String getString(const char*, String d = String());
  uint8_t getUChar(const char*, uint8_t d = 0);
  bool getBool(const char*, bool d = false);
  uint32_t getUInt(const char*, uint32_t d = 0);
  uint16_t getUShort(const char*, uint16_t d = 0);
  int getInt(const char*, int d = 0);
  bool remove(const char*);
  size_t putBytes(const char*, const void*, size_t);
  size_t getBytes(const char*, void*, size_t);
};
