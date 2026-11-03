// Minimal stand-ins for the Arduino-ESP32 API surface used by the glue code.
// Only for `make syntax`; reflects the real signatures as documented, not a behavioural model.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define PROGMEM
#define SERIAL_8N1 0x800001c
typedef uint8_t byte;
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
void delay(uint32_t);
uint32_t millis();
class String {
 public:
  String(const char* s = "") : s_(s) {}
  const char* c_str() const { return s_.c_str(); }
  unsigned length() const { return s_.size(); }
  void trim();
  char operator[](size_t i) const { return s_[i]; }
  bool operator==(const char* o) const { return s_ == o; }
 private:
  std::string s_;
};
class IPAddress {
 public:
  IPAddress() {}
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  IPAddress(uint32_t) {}
  bool fromString(const char*) { return true; }
  bool fromString(const String&) { return true; }
  String toString() const { return String(); }
};
class Print {
 public:
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  size_t println(const char*);
  size_t print(char);
  size_t print(const char*);
  size_t write(const uint8_t*, size_t);
};
class Stream : public Print {
 public:
  int available();
  int read();
  size_t readBytes(uint8_t*, size_t);
};
class HardwareSerial : public Stream {
 public:
  void begin(unsigned long baud, uint32_t cfg = 0, int8_t rx = -1, int8_t tx = -1);
  size_t setRxBufferSize(size_t);
};
extern HardwareSerial Serial, Serial2;
class EspClass {
 public:
  uint32_t getFreeHeap();
  uint32_t getMinFreeHeap();
  void restart();
  uint32_t getCycleCount();
};
extern EspClass ESP;
#define pdMS_TO_TICKS(x) (x)

// --- additions for the hardware 3.0 (ESP32-S3) glue
#define IRAM_ATTR
#define CHANGE 3
void noInterrupts();
void interrupts();
uint32_t getCpuFrequencyMhz();
void attachInterrupt(uint8_t, void (*)(), int);
void ledcAttach(uint8_t, uint32_t, uint8_t);
void ledcWrite(uint8_t, uint32_t);
void ledcSetup(uint8_t, uint32_t, uint8_t);
void ledcAttachPin(uint8_t, uint8_t);
