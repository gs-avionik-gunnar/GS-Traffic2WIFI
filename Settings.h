#pragma once
#include <stddef.h>
#include <stdint.h>

struct Settings {
  // WiFi
  char ssid[33];
  char wpakey[64];
  char country[3];       // WiFi regulatory region (see Regions.h)
  uint8_t channel;       // AP channel, must be allowed in the region
  bool clientMode;       // join an existing network instead of opening an AP
  char clientHost[16];   // TCP server to connect to in client mode
  bool defaultGw;        // offer the module as default gateway via DHCP
  bool reducePower;
  // Serial / NMEA over TCP
  bool nmeaStrict;       // reject NMEA sentences that have no checksum (bad checksums are always rejected)
  uint32_t baud;
  uint16_t tcpPort;
  bool nmeaTcp;          // forward the raw NMEA stream to TCP clients
  bool ble;              // NMEA over Bluetooth LE (HM-10 / CC2541 compatible)
  char bleName[21];      // BLE device name
  // GDL90 over UDP
  bool gdl90;
  uint16_t gdl90Port;
  bool gdl90Traffic;     // send FLARM traffic (PFLAA) as GDL90 traffic reports
  bool stratuxHb;        // also send the Stratux heartbeat (0xCC), used by SkyDemon & co. to detect the device
  uint8_t g90Cast;       // delivery: 0 broadcast, 1 unicast to every station (Stratux style), 2 both
  bool ffId;             // ForeFlight ID message (0x65)
  uint32_t ownAddr;      // ownship 24 bit address
  uint8_t ownAddrType;   // 0 ICAO, 1 self-assigned
  char callsign[9];
  uint8_t emitter;       // GDL90 emitter category
  // LED
  uint8_t trafficLed;    // 0 off, else minimum PFLAU/PFLAA alarm level for the red LED
  bool positionLed;      // orange when there is no valid position
  uint16_t trafficHoldS;
  uint16_t positionHoldS;
  bool nmeaReg;          // append the registration to PFLAA IDs in the NMEA stream (ID!REG), needs the aircraft database
  // Flight log
  bool flightLog;        // record flights (start/end time, duration, start/end position)
  uint16_t flightKmh;    // ground speed threshold for start/stop, km/h
  // Hardware 3.0 (M5 Dial): display + LED ring
  uint8_t dialBright;    // display backlight, 10..100 %
  uint8_t ringBright;    // Trafficmod LED brightness, 5..100 %
  bool ring;             // Trafficmod LED ring connected / enabled
  // Debug / test
  uint8_t debug;         // 0..3
  bool simWifi;          // simulator -> TCP (disables serial bridging)
  bool simSerial;        // simulator -> serial output (disables serial bridging)
};

void settingsDefaults(Settings& s);
void settingsSanitize(Settings& s);
void settingsLoad(Settings& s);  // loads from NVS, migrates 3.x settings on first boot
bool settingsSave(const Settings& s);
void settingsFactoryReset();
// Default SSID / key derived from the MAC address (same scheme as 3.x).
void settingsDefaultCredentials(char* ssid, size_t ssidCap, char* key, size_t keyCap);
