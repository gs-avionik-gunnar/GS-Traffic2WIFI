#include "Settings.h"

#include "Regions.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#if __has_include("esp_mac.h")
#include "esp_mac.h"
#endif
#include <stdio.h>
#include <string.h>

// NVS keys are limited to 15 characters.
static const char* kNs = "gs4";
static const uint32_t kCfgVersion = 1;

void settingsDefaultCredentials(char* ssid, size_t ssidCap, char* key, size_t keyCap) {
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);
  snprintf(ssid, ssidCap, "GS-Traffic-%02x%02x", mac[4], mac[5]);
  snprintf(key, keyCap, "%02x%02x%02x%02x", mac[2], mac[3], mac[4], mac[5]);
}

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof s);
  settingsDefaultCredentials(s.ssid, sizeof s.ssid, s.wpakey, sizeof s.wpakey);
  strcpy(s.country, "DE");
  s.channel = 1;
  strcpy(s.clientHost, "192.168.1.1");
  s.baud = 19200;
  s.tcpPort = 2000;
  s.nmeaTcp = true;
  s.ble = false;
  {
    char k[64];
    settingsDefaultCredentials(s.bleName, sizeof s.bleName, k, sizeof k);
  }
  s.gdl90 = true;
  s.gdl90Port = 4000;
  s.gdl90Traffic = true;
  s.stratuxHb = true;
  s.nmeaStrict = false;
  s.g90Cast = 0;
  s.ffId = false;   // the 4.0 alpha did not send it and worked with SkyDemon
  s.ownAddr = 0xF00000;
  s.ownAddrType = 1;
  strcpy(s.callsign, "GSTRAFFC");
  s.emitter = 1;
  s.trafficLed = 0;
  s.positionLed = false;
  s.trafficHoldS = 5;
  s.positionHoldS = 5;
  s.dialBright = 80;
  s.ringBright = 30;
  s.ring = false;
  s.startPage = 0;
  s.nmeaReg = false;
  s.flightLog = true;
  s.flightKmh = 60;
}

static uint32_t clampU(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

void settingsSanitize(Settings& s) {
  s.ssid[sizeof s.ssid - 1] = '\0';
  s.wpakey[sizeof s.wpakey - 1] = '\0';
  s.clientHost[sizeof s.clientHost - 1] = '\0';
  s.callsign[sizeof s.callsign - 1] = '\0';
  s.bleName[sizeof s.bleName - 1] = '\0';
  for (char* c = s.bleName; *c; ++c)
    if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '_' || *c == '.' || *c == ' ')) *c = '_';
  if (s.bleName[0] == '\0') {
    char k[64];
    settingsDefaultCredentials(s.bleName, sizeof s.bleName, k, sizeof k);
  }
  if (strlen(s.ssid) < 1) settingsDefaultCredentials(s.ssid, sizeof s.ssid, s.wpakey, sizeof s.wpakey);
  if (strlen(s.wpakey) < 8) {
    char d1[33], d2[64];
    settingsDefaultCredentials(d1, sizeof d1, d2, sizeof d2);
    strcpy(s.wpakey, d2);
  }
  s.country[2] = '\0';
  const Region* reg = findRegion(s.country);
  if (!reg) reg = &defaultRegion();
  strcpy(s.country, reg->cc);
  s.channel = (uint8_t)clampU(s.channel, reg->firstCh, reg->lastCh);
  s.baud = clampU(s.baud, 1200, 921600);
  if (s.tcpPort == 0) s.tcpPort = 2000;
  if (s.gdl90Port == 0) s.gdl90Port = 4000;
  if (s.g90Cast > 2) s.g90Cast = 0;
  s.ownAddr &= 0xFFFFFF;
  s.ownAddrType = s.ownAddrType ? 1 : 0;
  s.emitter = (uint8_t)clampU(s.emitter, 0, 22);
  s.trafficLed = (uint8_t)clampU(s.trafficLed, 0, 3);
  s.trafficHoldS = (uint16_t)clampU(s.trafficHoldS, 1, 600);
  s.positionHoldS = (uint16_t)clampU(s.positionHoldS, 1, 600);
  s.dialBright = (uint8_t)clampU(s.dialBright, 10, 100);
  s.ringBright = (uint8_t)clampU(s.ringBright, 5, 100);
  s.startPage = (uint8_t)clampU(s.startPage, 0, 7);
  s.flightKmh = (uint16_t)clampU(s.flightKmh, 20, 300);
  s.debug = (uint8_t)clampU(s.debug, 0, 3);
  if (s.clientHost[0] == '\0') strcpy(s.clientHost, "192.168.1.1");
  for (char* c = s.callsign; *c; ++c)
    if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
}

// Reads settings written by firmware 2.x/3.x (namespace "GS-Traffic") on first boot of 4.x.
static void migrateLegacy(Settings& s) {
  Preferences old;
  if (!old.begin("GS-Traffic", true)) return;
  String ssid = old.getString("ssid", "");
  if (ssid.length() > 0) {
    String key = old.getString("wpakey", "");
    strncpy(s.ssid, ssid.c_str(), sizeof s.ssid - 1);
    if (key.length() >= 8) strncpy(s.wpakey, key.c_str(), sizeof s.wpakey - 1);
    int baud = old.getInt("baudrate", 0);
    if (baud >= 1200) s.baud = (uint32_t)baud;
    int tcp = old.getInt("tcpport", 0);
    if (tcp > 0 && tcp < 65536) s.tcpPort = (uint16_t)tcp;
    int udp = old.getInt("udpport", 0);
    if (udp > 0 && udp < 65536) s.gdl90Port = (uint16_t)udp;
    String cc = old.getString("countrycode", "DE");
    if (findRegion(cc.c_str())) strncpy(s.country, cc.c_str(), 2);
    s.defaultGw = old.getInt("defgw", 0) == 1;
    s.reducePower = old.getInt("redpw", 0) == 1;
    s.clientMode = old.getInt("clientmode", 0) == 1;
    s.trafficLed = (uint8_t)old.getInt("trafficwarnled", 0);
    s.positionLed = old.getInt("positionwarnled", 0) == 1;
  }
  old.end();
}

static void readAll(Preferences& p, Settings& s) {
  String v;
  v = p.getString("ssid", s.ssid);
  strncpy(s.ssid, v.c_str(), sizeof s.ssid - 1);
  v = p.getString("wpakey", s.wpakey);
  strncpy(s.wpakey, v.c_str(), sizeof s.wpakey - 1);
  v = p.getString("chost", s.clientHost);
  strncpy(s.clientHost, v.c_str(), sizeof s.clientHost - 1);
  v = p.getString("ocall", s.callsign);
  strncpy(s.callsign, v.c_str(), sizeof s.callsign - 1);
  v = p.getString("country", s.country);
  strncpy(s.country, v.c_str(), 2);
  s.country[2] = '\0';
  s.channel = p.getUChar("chan", s.channel);
  s.clientMode = p.getBool("client", s.clientMode);
  s.defaultGw = p.getBool("defgw", s.defaultGw);
  s.reducePower = p.getBool("redpw", s.reducePower);
  s.baud = p.getUInt("baud", s.baud);
  s.tcpPort = p.getUShort("tcp", s.tcpPort);
  s.nmeaTcp = p.getBool("nmeatcp", s.nmeaTcp);
  s.ble = p.getBool("ble", s.ble);
  v = p.getString("blename", s.bleName);
  strncpy(s.bleName, v.c_str(), sizeof s.bleName - 1);
  s.bleName[sizeof s.bleName - 1] = '\0';
  s.gdl90 = p.getBool("g90", s.gdl90);
  s.gdl90Port = p.getUShort("g90port", s.gdl90Port);
  s.gdl90Traffic = p.getBool("g90trf", s.gdl90Traffic);
  s.stratuxHb = p.getBool("g90stx", s.stratuxHb);
  s.ffId = p.getBool("g90ff", s.ffId);
  s.nmeaStrict = p.getBool("nmeastrict", s.nmeaStrict);
  s.g90Cast = p.getUChar("g90cast", s.g90Cast);
  s.ownAddr = p.getUInt("oaddr", s.ownAddr);
  s.ownAddrType = p.getUChar("oatype", s.ownAddrType);
  s.emitter = p.getUChar("oemit", s.emitter);
  s.trafficLed = p.getUChar("tled", s.trafficLed);
  s.positionLed = p.getBool("pled", s.positionLed);
  s.trafficHoldS = p.getUShort("twhold", s.trafficHoldS);
  s.positionHoldS = p.getUShort("poshold", s.positionHoldS);
  s.dialBright = p.getUChar("dlbr", s.dialBright);
  s.ringBright = p.getUChar("tmbr", s.ringBright);
  s.ring = p.getBool("tmod", s.ring);
  s.startPage = p.getUChar("dlstart", s.startPage);
  s.nmeaReg = p.getBool("nreg", s.nmeaReg);
  s.flightLog = p.getBool("flog", s.flightLog);
  s.flightKmh = p.getUShort("fkmh", s.flightKmh);
  s.debug = p.getUChar("debug", s.debug);
  s.simWifi = p.getBool("simw", s.simWifi);
  s.simSerial = p.getBool("sims", s.simSerial);
}

static void writeAll(Preferences& p, const Settings& s) {
  p.putString("ssid", s.ssid);
  p.putString("wpakey", s.wpakey);
  p.putString("chost", s.clientHost);
  p.putString("ocall", s.callsign);
  p.putString("country", s.country);
  p.putUChar("chan", s.channel);
  p.putBool("client", s.clientMode);
  p.putBool("defgw", s.defaultGw);
  p.putBool("redpw", s.reducePower);
  p.putUInt("baud", s.baud);
  p.putUShort("tcp", s.tcpPort);
  p.putBool("nmeatcp", s.nmeaTcp);
  p.putBool("ble", s.ble);
  p.putString("blename", s.bleName);
  p.putBool("g90", s.gdl90);
  p.putUShort("g90port", s.gdl90Port);
  p.putBool("g90trf", s.gdl90Traffic);
  p.putBool("g90stx", s.stratuxHb);
  p.putBool("g90ff", s.ffId);
  p.putBool("nmeastrict", s.nmeaStrict);
  p.putUChar("g90cast", s.g90Cast);
  p.putUInt("oaddr", s.ownAddr);
  p.putUChar("oatype", s.ownAddrType);
  p.putUChar("oemit", s.emitter);
  p.putUChar("tled", s.trafficLed);
  p.putBool("pled", s.positionLed);
  p.putUShort("twhold", s.trafficHoldS);
  p.putUShort("poshold", s.positionHoldS);
  p.putUChar("dlbr", s.dialBright);
  p.putUChar("tmbr", s.ringBright);
  p.putBool("tmod", s.ring);
  p.putUChar("dlstart", s.startPage);
  p.putBool("nreg", s.nmeaReg);
  p.putBool("flog", s.flightLog);
  p.putUShort("fkmh", s.flightKmh);
  p.putUChar("debug", s.debug);
  p.putBool("simw", s.simWifi);
  p.putBool("sims", s.simSerial);
  p.putUInt("cfgver", kCfgVersion);
}

void settingsLoad(Settings& s) {
  settingsDefaults(s);
  Preferences p;
  if (!p.begin(kNs, false)) {
    settingsSanitize(s);
    return;
  }
  if (p.getUInt("cfgver", 0) == 0) {
    migrateLegacy(s);
    settingsSanitize(s);
    writeAll(p, s);
  } else {
    readAll(p, s);
    settingsSanitize(s);
  }
  p.end();
}

bool settingsSave(const Settings& in) {
  Settings s = in;
  settingsSanitize(s);
  Preferences p;
  if (!p.begin(kNs, false)) return false;
  writeAll(p, s);  // no clear(): a power loss mid-save can no longer wipe every setting
  p.end();
  return true;
}

void settingsFactoryReset() {
  Preferences p;
  if (p.begin(kNs, false)) {
    p.clear();
    p.end();
  }
  Settings s;
  settingsDefaults(s);
  settingsSave(s);
}
