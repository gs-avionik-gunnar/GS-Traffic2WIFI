#pragma once
#include "WiFiClient.h"
class WiFiServer {
 public:
  WiFiServer(uint16_t port = 80);
  void begin(uint16_t port = 0);
  void setNoDelay(bool);
  bool hasClient();
  WiFiClient available();
};
