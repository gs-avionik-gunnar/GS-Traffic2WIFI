// GS-Traffic2WIFI 4.1BETA1 - serial/NMEA to WiFi bridge with GDL90 output for ESP32.
//
// Architecture (see README.md):
//   Nmea.*       NMEA line assembly, checksum, parsers            (pure C++, unit-tested)
//   State.*      ownship + traffic state, LED logic               (pure C++, unit-tested)
//   Gdl90.*      GDL90 encoder and 1 Hz publisher                 (pure C++, unit-tested)
//   Simulator.*  synthetic NMEA for testing                       (pure C++)
//   Settings.*   NVS persistence + migration from 3.x
//   Bridge.*     serial/TCP/UDP data path (runs in loop())
//   WebUi.*      HTTP UI + OTA (own FreeRTOS task)
//   Gfx/DialUi/DialHw/DialFont  hardware 3.0 (M5 Dial, ESP32-S3): round display, wheel, LED ring
//
// Build with the Arduino IDE: board "ESP32 Dev Module", Arduino-ESP32 core 2.0.x or 3.x.
// No additional libraries are needed.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include "App.h"
#include "Bridge.h"
#include "Config.h"
#include "DialHw.h"
#include "FlightStore.h"
#include "CallsignStore.h"
#include "Regions.h"
#include "Settings.h"
#include "StatusLed.h"
#include "WebUi.h"

static Bridge bridge;

static void detectHardware() {
  if (dialhw::supported()) {  // ESP32-S3 build = hardware 3.0 (M5 Dial)
    g_hw3 = true;
    g_hwName = "3.0 (M5 Dial)";
    return;
  }
  pinMode(PIN_HWTYPE_A, INPUT_PULLUP);
  pinMode(PIN_HWTYPE_B, INPUT_PULLUP);
  delay(5);
  if (digitalRead(PIN_HWTYPE_A) == LOW && digitalRead(PIN_HWTYPE_B) == LOW) {
    g_hw2 = true;
    g_hwName = "2.0";
  }
}

// Hold the reset pin to GND while powering up: settings are reset, release the pin and the unit reboots.
static void checkFactoryReset() {
  if (g_hw3) return;  // GPIO 19 is USB on the ESP32-S3: the Dial resets via its menu
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  delay(50);
  if (digitalRead(PIN_FACTORY_RESET) != LOW) return;
  Serial.println("Factory reset requested");
  statusled::set(3);
  settingsFactoryReset();
  uint32_t t0 = millis();
  while (digitalRead(PIN_FACTORY_RESET) == LOW && millis() - t0 < 10000) delay(50);
  ESP.restart();
}

// Must run after WiFi.mode() (radio initialised) and before the AP/STA is started,
// otherwise the channel check uses the wrong region.
static void applyRegion() {
  const Region* r = findRegion(g_settings.country);
  if (!r) r = &defaultRegion();
  wifi_country_t c;
  memset(&c, 0, sizeof c);
  c.cc[0] = r->cc[0];
  c.cc[1] = r->cc[1];
  c.cc[2] = '\0';
  c.schan = r->firstCh;
  c.nchan = (uint8_t)(r->lastCh - r->firstCh + 1);
  c.max_tx_power = 20;
  c.policy = WIFI_COUNTRY_POLICY_MANUAL;
  esp_err_t err = esp_wifi_set_country(&c);  // 3.x declared this but never called it
  if (err != ESP_OK) Serial.printf("esp_wifi_set_country(%s) failed: %d\n", r->cc, (int)err);
}

static void tuneWifi() {
  if (g_settings.reducePower) WiFi.setTxPower(WIFI_POWER_11dBm);
}

static void startAccessPoint(const char* ssid, const char* key, uint8_t channel) {
  IPAddress ip(AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
  IPAddress mask(255, 255, 255, 0);
  WiFi.mode(WIFI_AP);
  applyRegion();
  if (!WiFi.softAPConfig(ip, ip, mask)) Serial.println("softAPConfig failed");
  if (!WiFi.softAP(ssid, key, channel, 0, AP_MAX_STATIONS)) Serial.println("softAP failed");

  if (!g_settings.defaultGw) {
    // Do not announce ourselves as router, so phones keep using cellular data for the internet.
    esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap) {
      uint8_t offer = 0;
      esp_netif_dhcps_stop(ap);
      esp_netif_dhcps_option(ap, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &offer, sizeof offer);
      esp_netif_dhcps_start(ap);
    }
  }
  tuneWifi();
  Serial.printf("WiFi AP '%s' started, IP %s\n", ssid, ip.toString().c_str());
}

static void startWifi() {
  const Settings& s = g_settings;
  if (!s.clientMode) {
    startAccessPoint(s.ssid, s.wpakey, s.channel);
    return;
  }

  WiFi.mode(WIFI_STA);
  applyRegion();
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(s.ssid, s.wpakey);
  Serial.printf("Joining '%s' ", s.ssid);
  uint32_t t0 = millis();
  uint8_t blink = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < CLIENT_MODE_CONNECT_TIMEOUT_MS) {
    statusled::set((blink++ & 1) ? 3 : 0);
    delay(250);
    Serial.print('.');
  }
  statusled::set(0);
  if (WiFi.status() == WL_CONNECTED) {
    tuneWifi();
    Serial.printf("\nconnected, IP %s\n", WiFi.localIP().toString().c_str());
    return;
  }

  // Never leave the unit unreachable: fall back to an AP with the factory credentials.
  Serial.println("\njoin failed, starting fallback AP with default credentials");
  g_apFallback = true;
  WiFi.disconnect(true);
  char ssid[33], key[64];
  settingsDefaultCredentials(ssid, sizeof ssid, key, sizeof key);
  startAccessPoint(ssid, key, s.channel);
}

void setup() {
  Serial.begin(115200);
  appInit();
  detectHardware();
  statusled::begin();
  statusled::bootSequence();

  settingsLoad(g_settings);
  checkFactoryReset();
  if (g_hw3) dialhw::begin();
  Serial.printf("\nGS-Traffic2WIFI %s (HW %s), built %s %s\n", GS_FW_VERSION, g_hwName, __DATE__, __TIME__);

  flights::begin();
  csstore::begin();
  startWifi();
  bridge.begin();
  webBegin();
}

void loop() {
  bridge.poll();
  delay(1);  // yields to WiFi/lwIP; the 2 KB UART buffer easily covers this at 115200 baud
}
