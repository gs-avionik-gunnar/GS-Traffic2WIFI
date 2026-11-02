#pragma once
#include "Arduino.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"
enum { WL_CONNECTED = 3 };
typedef enum { WIFI_AP = 2, WIFI_STA = 1 } wifi_mode_t;
typedef enum { WIFI_POWER_11dBm = 44 } wifi_power_t;
typedef enum { ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED = 1, ARDUINO_EVENT_MAX = 99 } arduino_event_id_t;
typedef struct {
  struct {
    struct { uint32_t addr; } ip;
  } wifi_ap_staipassigned;
} arduino_event_info_t;
typedef void (*WiFiEventCb)(arduino_event_id_t, arduino_event_info_t);
class WiFiClass {
 public:
  int onEvent(WiFiEventCb, arduino_event_id_t = ARDUINO_EVENT_MAX);
  bool mode(wifi_mode_t);
  bool softAPConfig(IPAddress, IPAddress, IPAddress);
  bool softAP(const char*, const char*, int channel = 1, int hidden = 0, int maxConn = 4);
  uint8_t softAPgetStationNum();
  int status();
  void begin(const char*, const char*);
  bool disconnect(bool wifioff = false);
  void setAutoReconnect(bool);
  bool setSleep(bool);
  bool setTxPower(wifi_power_t);
  IPAddress localIP();
  IPAddress broadcastIP();
  int8_t RSSI();
};
extern WiFiClass WiFi;
