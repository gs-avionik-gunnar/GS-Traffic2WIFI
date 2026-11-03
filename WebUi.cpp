#include "WebUi.h"

#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "App.h"
#include "Ble.h"
#include "Config.h"
#include "Regions.h"
#include "WebPage.h"

static WebServer server(80);
static char g_json[4096];
static NavState g_snap;  // web task only
static bool g_otaOk = false;

static void jf(char* b, size_t cap, size_t& len, const char* fmt, ...) __attribute__((format(printf, 4, 5)));
static void jf(char* b, size_t cap, size_t& len, const char* fmt, ...) {
  if (len >= cap) return;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(b + len, cap - len, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  len += (size_t)n;
  if (len >= cap) len = cap - 1;
}

static void restartSoon() {
  delay(600);  // let the HTTP response leave the socket
  ESP.restart();
}

// ------------------------------------------------------------------ status
static void handleStatus() {
  uint32_t now = millis();
  if (!navSnapshot(g_snap, 100)) {
    server.send(503, "text/plain", "busy");
    return;
  }
  const NavState& n = g_snap;
  size_t len = 0;
  char* b = g_json;
  const size_t cap = sizeof g_json;
  const bool pos = n.posFresh(now);

  jf(b, cap, len, "{\"fw\":\"%s\",\"hw\":\"%s\",\"up\":%lu,\"heap\":%u,\"minHeap\":%u,\"led\":%u,\"sim\":%s,", GS_FW_VERSION, g_hwName,
     (unsigned long)(now / 1000), (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)g_stats.led,
     g_stats.simActive ? "true" : "false");

  const bool apActive = !g_settings.clientMode || g_apFallback;
  jf(b, cap, len, "\"wifi\":{\"mode\":\"%s\",", apActive ? (g_apFallback ? "AP (fallback)" : "AP") : "client");
  if (apActive) jf(b, cap, len, "\"stations\":%d,\"rssi\":0},", WiFi.softAPgetStationNum());
  else jf(b, cap, len, "\"stations\":null,\"rssi\":%d},", (int)WiFi.RSSI());

  jf(b, cap, len, "\"ble\":{\"avail\":%d,\"on\":%d,\"conn\":%d,\"name\":\"%s\"},", ble::available(), g_settings.ble && ble::available(),
     ble::connections(), g_settings.bleName);
  jf(b, cap, len, "\"gps\":{\"valid\":%s,\"lat\":%.6f,\"lon\":%.6f,\"sog\":%.1f,\"trk\":%.1f,", pos ? "true" : "false", n.lat, n.lon,
     n.sogKt, n.trackDeg);
  if (n.timeFresh(now)) {
    uint32_t s = (n.secOfDay + NavState::age(now, n.timeMs) / 1000u) % 86400u;
    jf(b, cap, len, "\"time\":\"%02u:%02u:%02u\",", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
  } else {
    jf(b, cap, len, "\"time\":null,");
  }
  if (n.baroFresh(now)) jf(b, cap, len, "\"baro\":%ld,", (long)n.baroAltFt);
  else jf(b, cap, len, "\"baro\":null,");
  if (n.altFresh(now)) jf(b, cap, len, "\"alt\":%.1f,", n.altMslM);
  else jf(b, cap, len, "\"alt\":null,");
  jf(b, cap, len, "\"sats\":%u,", (unsigned)n.sats);
  if (n.accFresh(now)) jf(b, cap, len, "\"hdop\":%.1f},", n.hdop);
  else jf(b, cap, len, "\"hdop\":null},");

  jf(b, cap, len, "\"flarm\":{\"seen\":%s,\"rx\":%u,\"tx\":%u,\"gps\":%u,\"alarm\":%u},", n.flarmSeen ? "true" : "false",
     (unsigned)n.flarmRx, (unsigned)n.flarmTx, (unsigned)n.flarmGps,
     (n.alarmEver && NavState::age(now, n.alarmMs) < 5000) ? (unsigned)n.alarmLevel : 0u);

  int shown = 0, count = 0;
  jf(b, cap, len, "\"traffic\":{\"list\":[");
  for (int i = 0; i < TrafficTable::kMax; ++i) {
    const Target& t = n.traffic.slot(i);
    if (!t.used || NavState::age(now, t.lastMs) > kTargetTimeoutMs) continue;
    ++count;
    if (shown >= 12) continue;
    jf(b, cap, len, "%s{\"id\":\"%06lX\",\"d\":%d,\"b\":%d,\"a\":", shown ? "," : "", (unsigned long)t.id, (int)t.distM,
       (int)t.bearingDeg);
    if (t.altValid) jf(b, cap, len, "%ld", (long)t.altFt);
    else jf(b, cap, len, "null");
    jf(b, cap, len, ",\"al\":%u}", (unsigned)t.alarm);
    ++shown;
  }
  jf(b, cap, len, "],\"count\":%d},", count);

  int age = n.lastRxMs ? (int)NavState::age(now, n.lastRxMs) : -1;
  jf(b, cap, len, "\"nmea\":{\"lines\":%lu,\"bad\":%lu,\"nocs\":%lu,\"err\":%lu,\"age\":%d,\"rej\":[", (unsigned long)n.stats.lines,
     (unsigned long)n.stats.badChecksum, (unsigned long)n.stats.noChecksum, (unsigned long)n.stats.parseErrors, age);
  for (int i = 0, first = 1; i < NavState::kRejKeep; ++i) {
    const char* r = n.rej[(n.rejNext + i) % NavState::kRejKeep];
    if (!r[0]) continue;
    jf(b, cap, len, "%s\"%s\"", first ? "" : ",", r);
    first = 0;
  }
  jf(b, cap, len, "]},");
  jf(b, cap, len, "\"tcp\":{\"clients\":%lu},\"gdl90\":{\"enabled\":%s,\"frames\":%lu,\"errors\":%lu},",
     (unsigned long)g_stats.tcpClients, g_settings.gdl90 ? "true" : "false", (unsigned long)g_stats.gdl90Frames,
     (unsigned long)g_stats.udpErrors);

  // What the GDL90 publisher last sent (helps to see why an app reports "no GPS")
  const Gdl90Info& gi = n.gdl;
  jf(b, cap, len, "\"g90\":{\"on\":%s,\"gps\":%s,\"utc\":%s,\"lat\":%.6f,\"lon\":%.6f,\"alt\":", gi.valid ? "true" : "false",
     gi.hbGps ? "true" : "false", gi.hbUtc ? "true" : "false", gi.lat, gi.lon);
  if (gi.altValid) jf(b, cap, len, "%ld", (long)gi.altFt);
  else jf(b, cap, len, "null");
  jf(b, cap, len, ",\"nic\":%u,\"nacp\":%u,\"air\":%s,\"geo\":", (unsigned)gi.nic, (unsigned)gi.nacp, gi.airborne ? "true" : "false");
  if (gi.geoValid) jf(b, cap, len, "%ld", (long)gi.geoFt);
  else jf(b, cap, len, "null");
  jf(b, cap, len, ",\"vfom\":%u,\"hb\":%lu,\"own\":%lu,\"trf\":%lu}}", (unsigned)gi.vfom, (unsigned long)gi.hbCount,
     (unsigned long)gi.ownCount, (unsigned long)gi.trafficCount);

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", g_json);
}

// ------------------------------------------------------------------ config
static void handleGetConfig() {
  const Settings& s = g_settings;
  size_t len = 0;
  char* b = g_json;
  const size_t cap = sizeof g_json;
  // SSID and callsign are restricted to safe characters on save; escape quotes/backslashes anyway.
  char ssid[70], cs[20];
  size_t j = 0;
  for (const char* p = s.ssid; *p && j < sizeof ssid - 3; ++p) {
    if (*p == '"' || *p == '\\') ssid[j++] = '\\';
    ssid[j++] = *p;
  }
  ssid[j] = 0;
  j = 0;
  for (const char* p = s.callsign; *p && j < sizeof cs - 3; ++p) {
    if (*p == '"' || *p == '\\') cs[j++] = '\\';
    cs[j++] = *p;
  }
  cs[j] = 0;
  jf(b, cap, len,
     "{\"ssid\":\"%s\",\"country\":\"%s\",\"channel\":%u,\"clientMode\":%d,\"clientHost\":\"%s\",\"defaultGw\":%d,\"reducePower\":%d,"
     "\"baud\":%lu,\"tcpPort\":%u,\"nmeaTcp\":%d,\"gdl90\":%d,\"gdl90Port\":%u,\"gdl90Traffic\":%d,\"stratuxHb\":%d,\"g90Cast\":%u,\"ffId\":%d,\"nmeaStrict\":%d,"
     "\"ownAddr\":\"%06lX\",\"ownAddrType\":%u,\"callsign\":\"%s\",\"emitter\":%u,\"trafficLed\":%u,\"positionLed\":%d,"
     "\"trafficHoldS\":%u,\"positionHoldS\":%u,\"ring\":%d,\"ringBright\":%u,\"ble\":%d,\"bleName\":\"%s\",\"bleAvail\":%d,\"ringAvail\":%d,\"startPage\":%u,\"debug\":%u,\"simWifi\":%d,\"simSerial\":%d}",
     ssid, s.country, s.channel, s.clientMode, s.clientHost, s.defaultGw, s.reducePower, (unsigned long)s.baud, s.tcpPort, s.nmeaTcp,
     s.gdl90, s.gdl90Port, s.gdl90Traffic, s.stratuxHb, (unsigned)s.g90Cast, s.ffId, s.nmeaStrict, (unsigned long)s.ownAddr, s.ownAddrType, cs, s.emitter, s.trafficLed,
     s.positionLed, s.trafficHoldS, s.positionHoldS, s.ring, s.ringBright, s.ble, s.bleName, ble::available(), g_hw3, (unsigned)s.startPage, s.debug, s.simWifi, s.simSerial);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", g_json);
}

static bool argBool(const char* name) { return server.arg(name) == "on"; }

// Strict integer field. Absent field: keeps `val`. Present but empty, non-numeric or out of range:
// returns false and fills `err` (the old code silently kept the previous value in that case).
static bool argInt(const char* name, const char* label, long lo, long hi, long& val, String& err) {
  if (!server.hasArg(name)) return true;
  String v = server.arg(name);
  v.trim();
  bool ok = v.length() > 0 && v.length() <= 10;
  for (unsigned i = 0; ok && i < v.length(); ++i)
    if (!isdigit((unsigned char)v[i]) && !(i == 0 && v[i] == '-' && v.length() > 1)) ok = false;
  long r = 0;
  if (ok) {
    r = strtol(v.c_str(), nullptr, 10);
    ok = (r >= lo && r <= hi);
  }
  if (!ok) {
    char buf[96];
    snprintf(buf, sizeof buf, "%s must be a whole number from %ld to %ld", label, lo, hi);
    err = buf;
    return false;
  }
  val = r;
  return true;
}

#define FAIL(msg) return server.send(400, "text/plain", (msg))
#define NUM(var, name, label, lo, hi)                             \
  do {                                                            \
    long _v = (long)(var);                                        \
    if (!argInt(name, label, lo, hi, _v, err)) FAIL(err.c_str()); \
    (var) = (decltype(var))_v;                                    \
  } while (0)

static void handlePostConfig() {
  Settings s = g_settings;  // start from the running config so missing fields keep their value
  String err;

  String ssid = server.arg("ssid");
  if (ssid.length() < 1 || ssid.length() > 32) FAIL("SSID must be 1-32 characters");
  strncpy(s.ssid, ssid.c_str(), sizeof s.ssid - 1);
  s.ssid[sizeof s.ssid - 1] = 0;

  String key = server.arg("wpakey");
  if (key.length() > 0) {
    if (key.length() < 8 || key.length() > 63) FAIL("WPA key must be 8-63 characters");
    strncpy(s.wpakey, key.c_str(), sizeof s.wpakey - 1);
    s.wpakey[sizeof s.wpakey - 1] = 0;
  }

  // Region first: the channel is validated against it.
  String cc = server.arg("country");
  if (cc.length() > 0) {
    const Region* r = findRegion(cc.c_str());
    if (!r) FAIL("Unknown region code");
    strcpy(s.country, r->cc);
  }
  const Region* reg = findRegion(s.country);
  if (!reg) reg = &defaultRegion();
  {
    long ch = s.channel;
    char label[48];
    snprintf(label, sizeof label, "AP channel (region %s)", reg->cc);
    if (!argInt("channel", label, reg->firstCh, reg->lastCh, ch, err)) FAIL(err.c_str());
    s.channel = (uint8_t)ch;
  }

  NUM(s.tcpPort, "tcpPort", "TCP port", 1, 65535);
  NUM(s.gdl90Port, "gdl90Port", "UDP port", 1, 65535);
  NUM(s.trafficHoldS, "trafficHoldS", "Red LED hold time", 1, 600);
  NUM(s.positionHoldS, "positionHoldS", "Orange LED delay", 1, 600);
  NUM(s.trafficLed, "trafficLed", "Red LED alarm level", 0, 3);
  NUM(s.ringBright, "ringBright", "LED ring brightness", 5, 100);
  NUM(s.startPage, "startPage", "Start page", 0, 7);
  NUM(s.debug, "debug", "Debug level", 0, 3);
  NUM(s.ownAddrType, "ownAddrType", "Address type", 0, 1);
  NUM(s.emitter, "emitter", "Emitter category", 0, 22);
  NUM(s.g90Cast, "g90Cast", "GDL90 delivery", 0, 2);
  {
    long baud = s.baud;
    if (!argInt("baud", "Baud rate", 1200, 921600, baud, err)) FAIL(err.c_str());
    static const long allowed[] = {4800, 9600, 19200, 28800, 38400, 57600, 115200, 230400};
    bool known = false;
    for (size_t i = 0; i < sizeof allowed / sizeof allowed[0]; ++i) known = known || allowed[i] == baud;
    if (!known) FAIL("Baud rate must be one of 4800, 9600, 19200, 28800, 38400, 57600, 115200, 230400");
    s.baud = (uint32_t)baud;
  }

  String host = server.arg("clientHost");
  IPAddress ip;
  if (host.length() > 0) {
    if (host.length() > 15 || !ip.fromString(host)) FAIL("Server address must be an IPv4 address like 192.168.1.1");
    strncpy(s.clientHost, host.c_str(), sizeof s.clientHost - 1);
    s.clientHost[sizeof s.clientHost - 1] = 0;
  }

  String addr = server.arg("ownAddr");
  if (server.hasArg("ownAddr")) {
    bool ok = addr.length() >= 1 && addr.length() <= 6;
    for (unsigned i = 0; ok && i < addr.length(); ++i) ok = isxdigit((unsigned char)addr[i]);
    if (!ok) FAIL("Ownship address must be 1-6 hex digits (0-9, A-F)");
    s.ownAddr = (uint32_t)strtoul(addr.c_str(), nullptr, 16);
  }

  String cs = server.arg("callsign");
  if (cs.length() > 8) FAIL("Callsign: max. 8 characters");
  for (size_t i = 0; i < cs.length(); ++i)
    if (!isalnum((unsigned char)cs[i])) FAIL("Callsign: letters and digits only (no hyphen or space)");
  strncpy(s.callsign, cs.c_str(), sizeof s.callsign - 1);
  s.callsign[sizeof s.callsign - 1] = 0;

  // checkboxes are absent from the form when unchecked
  s.defaultGw = argBool("defaultGw");
  s.reducePower = argBool("reducePower");
  s.clientMode = argBool("clientMode");
  s.nmeaTcp = argBool("nmeaTcp");
  s.gdl90 = argBool("gdl90");
  s.gdl90Traffic = argBool("gdl90Traffic");
  s.stratuxHb = argBool("stratuxHb");
  s.ffId = argBool("ffId");
  s.nmeaStrict = argBool("nmeaStrict");
  s.positionLed = argBool("positionLed");
  s.ring = g_hw3 && argBool("ring");  // the LED ring exists on hardware 3.0 only
  s.ble = argBool("ble");
  if (server.hasArg("bleName")) {
    String bn = server.arg("bleName");
    if (bn.length() < 1 || bn.length() > 20) FAIL("Bluetooth name must be 1-20 characters");
    strncpy(s.bleName, bn.c_str(), sizeof s.bleName - 1);
    s.bleName[sizeof s.bleName - 1] = 0;
  }
  s.simWifi = argBool("simWifi");
  s.simSerial = argBool("simSerial");

  settingsSanitize(s);
  if (!settingsSave(s)) FAIL("Could not write settings");
  server.send(200, "text/plain", "OK");
  restartSoon();
}

static void handleRegions() {
  size_t len = 0;
  jf(g_json, sizeof g_json, len, "[");
  for (int i = 0; i < regionCount(); ++i) {
    const Region& r = regionAt(i);
    jf(g_json, sizeof g_json, len, "%s{\"cc\":\"%s\",\"n\":\"%s\",\"a\":%u,\"b\":%u}", i ? "," : "", r.cc, r.name,
       (unsigned)r.firstCh, (unsigned)r.lastCh);
  }
  jf(g_json, sizeof g_json, len, "]");
  server.send(200, "application/json", g_json);
}

static void handleReset() {
  settingsFactoryReset();
  server.send(200, "text/plain", "OK");
  restartSoon();
}

// --------------------------------------------------------------------- OTA
static void handleUpdateDone() {
  server.sendHeader("Connection", "close");
  if (g_otaOk) {
    server.send(200, "text/plain", "OK");
    restartSoon();
  } else {
    server.send(500, "text/plain", Update.hasError() ? Update.errorString() : "no file received");
  }
}

static void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    g_otaOk = false;
    LOG(1, "OTA: %s\n", up.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (Update.write(up.buf, up.currentSize) != up.currentSize) Update.printError(Serial);
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      g_otaOk = true;
      LOG(1, "OTA ok: %u bytes\n", (unsigned)up.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

// -------------------------------------------------------------------- task
static void webTask(void*) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void webBegin() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", kIndexHtml); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/regions", HTTP_GET, handleRegions);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  xTaskCreatePinnedToCore(webTask, "web", 10240, nullptr, 1, nullptr, 0);
}
