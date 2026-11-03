// Data path: serial (or simulator / TCP in client mode) -> NMEA parser -> state,
// raw stream -> TCP clients, state -> GDL90 -> UDP broadcast, state -> LED.
// poll() never blocks for long; call it from loop().
#pragma once
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include "Config.h"
#include "Gdl90.h"
#include "Nmea.h"
#include "Simulator.h"

class Bridge {
 public:
  Bridge() : server_(0), nextConnectTryMs_(0), simulate_(false), bcastCheckMs_(0), canSend_(true) {}
  void begin();
  void poll();

 private:
  void ingest(const uint8_t* data, size_t n, uint32_t now);
  void handleLine(const char* line, uint32_t now);
  void forwardRaw(const uint8_t* data, size_t n);
  void acceptClients();
  void pollSerial(uint32_t now);
  void pollSimulator(uint32_t now);
  void pollClientMode(uint32_t now);
  void pollGdl90(uint32_t now);
  void updateLed(uint32_t now);
  void sendGdl90(const uint8_t* frame, size_t len);
  void sendTo(IPAddress ip, const uint8_t* frame, size_t len);
  void onSimLine(const char* line);

  static void gdl90Thunk(const uint8_t* frame, size_t len, void* ctx) {
    static_cast<Bridge*>(ctx)->sendGdl90(frame, len);
  }
  static void simThunk(const char* line, void* ctx) { static_cast<Bridge*>(ctx)->onSimLine(line); }

  HardwareSerial* serial_ = &Serial2;
  WiFiServer server_;
  WiFiClient clients_[MAX_TCP_CLIENTS];
  uint8_t failCount_[MAX_TCP_CLIENTS] = {0};
  WiFiClient upstream_;  // client mode: connection to the remote NMEA server
  IPAddress upstreamIp_;
  uint32_t nextConnectTryMs_;

  nmea::LineAssembler assembler_;
  gdl90::Publisher publisher_;
  Simulator simulator_;
  bool simulate_;

  IPAddress bcast_;
  uint32_t bcastCheckMs_;
  bool canSend_;
  WiFiUDP udp_;
  uint32_t lastNow_ = 0;
};
