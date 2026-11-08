#include "DialHw.h"

#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include <SPI.h>
#include <initializer_list>
#include <WiFi.h>
#include <soc/gpio_reg.h>

#include "App.h"
#include "Ble.h"
#include "Config.h"
#include "DialUi.h"
#include "FlightStore.h"
#include "Settings.h"

namespace dialhw {

namespace {

// ---- LEDC wrappers (API differs between Arduino-ESP32 2.x and 3.x) ------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void pwmAttach(uint8_t pin, uint8_t, uint32_t freq, uint8_t bits) { ledcAttach(pin, freq, bits); }
void pwmWrite(uint8_t pin, uint8_t, uint32_t duty) { ledcWrite(pin, duty); }
#else
void pwmAttach(uint8_t pin, uint8_t ch, uint32_t freq, uint8_t bits) {
  ledcSetup(ch, freq, bits);
  ledcAttachPin(pin, ch);
}
void pwmWrite(uint8_t, uint8_t ch, uint32_t duty) { ledcWrite(ch, duty); }
#endif

const uint8_t kChBacklight = 0, kChBuzzer = 1;

// ---- GC9A01 --------------------------------------------------------------------------------------
void lcdCmd(uint8_t c) {
  digitalWrite(PIN_DIAL_LCD_DC, LOW);
  digitalWrite(PIN_DIAL_LCD_CS, LOW);
  SPI.transfer(c);
  digitalWrite(PIN_DIAL_LCD_CS, HIGH);
}

void lcdData(const uint8_t* d, size_t n) {
  digitalWrite(PIN_DIAL_LCD_DC, HIGH);
  digitalWrite(PIN_DIAL_LCD_CS, LOW);
  SPI.writeBytes(d, (uint32_t)n);
  digitalWrite(PIN_DIAL_LCD_CS, HIGH);
}

void lcdCmdData(uint8_t c, std::initializer_list<uint8_t> d) {
  lcdCmd(c);
  if (d.size()) {
    uint8_t tmp[16];
    size_t n = 0;
    for (uint8_t v : d) tmp[n++] = v;
    lcdData(tmp, n);
  }
}

void lcdInit() {
  pinMode(PIN_DIAL_LCD_CS, OUTPUT);
  pinMode(PIN_DIAL_LCD_DC, OUTPUT);
  pinMode(PIN_DIAL_LCD_RST, OUTPUT);
  digitalWrite(PIN_DIAL_LCD_CS, HIGH);
  SPI.begin(PIN_DIAL_LCD_SCLK, -1, PIN_DIAL_LCD_MOSI, -1);
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));  // kept for the lifetime of the task

  digitalWrite(PIN_DIAL_LCD_RST, HIGH);
  delay(10);
  digitalWrite(PIN_DIAL_LCD_RST, LOW);
  delay(10);
  digitalWrite(PIN_DIAL_LCD_RST, HIGH);
  delay(120);

  lcdCmd(0xEF);
  lcdCmdData(0xEB, {0x14});
  lcdCmd(0xFE);
  lcdCmd(0xEF);
  lcdCmdData(0xEB, {0x14});
  lcdCmdData(0x84, {0x40});
  lcdCmdData(0x85, {0xFF});
  lcdCmdData(0x86, {0xFF});
  lcdCmdData(0x87, {0xFF});
  lcdCmdData(0x88, {0x0A});
  lcdCmdData(0x89, {0x21});
  lcdCmdData(0x8A, {0x00});
  lcdCmdData(0x8B, {0x80});
  lcdCmdData(0x8C, {0x01});
  lcdCmdData(0x8D, {0x01});
  lcdCmdData(0x8E, {0xFF});
  lcdCmdData(0x8F, {0xFF});
  lcdCmdData(0xB6, {0x00, 0x00});
  lcdCmdData(0x36, {DIAL_LCD_MADCTL});
  lcdCmdData(0x3A, {0x05});  // 16 bit colour
  lcdCmdData(0x90, {0x08, 0x08, 0x08, 0x08});
  lcdCmdData(0xBD, {0x06});
  lcdCmdData(0xBC, {0x00});
  lcdCmdData(0xFF, {0x60, 0x01, 0x04});
  lcdCmdData(0xC3, {0x13});
  lcdCmdData(0xC4, {0x13});
  lcdCmdData(0xC9, {0x22});
  lcdCmdData(0xBE, {0x11});
  lcdCmdData(0xE1, {0x10, 0x0E});
  lcdCmdData(0xDF, {0x21, 0x0C, 0x02});
  lcdCmdData(0xF0, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A});
  lcdCmdData(0xF1, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F});
  lcdCmdData(0xF2, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A});
  lcdCmdData(0xF3, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F});
  lcdCmdData(0xED, {0x1B, 0x0B});
  lcdCmdData(0xAE, {0x77});
  lcdCmdData(0xCD, {0x63});
  lcdCmdData(0x70, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03});
  lcdCmdData(0xE8, {0x34});
  lcdCmdData(0x62, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70});
  lcdCmdData(0x63, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70});
  lcdCmdData(0x64, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07});
  lcdCmdData(0x66, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00});
  lcdCmdData(0x67, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98});
  lcdCmdData(0x74, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00});
  lcdCmdData(0x98, {0x3E, 0x07});
  lcdCmd(0x35);
  lcdCmd(DIAL_LCD_INVERT ? 0x21 : 0x20);
  lcdCmd(0x11);
  delay(120);
  lcdCmd(0x29);
  delay(20);
}

// Sends rows [y0, y0+h) of the 240 pixel wide frame (pixels already in wire byte order).
void lcdFlush(const uint16_t* px, int y0, int h) {
  lcdCmdData(0x2A, {0x00, 0x00, 0x00, 239});
  lcdCmdData(0x2B, {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)((y0 + h - 1) >> 8), (uint8_t)(y0 + h - 1)});
  lcdCmd(0x2C);
  lcdData((const uint8_t*)px, (size_t)240 * h * 2);
}

// ---- wheel (quadrature, interrupt driven) -----------------------------------------------------------
volatile int32_t s_enc = 0;
volatile uint8_t s_prev = 0;

static inline __attribute__((always_inline)) uint8_t IRAM_ATTR readEnc() {
  uint32_t in1 = REG_READ(GPIO_IN1_REG);
  return (uint8_t)((((in1 >> (PIN_DIAL_ENC_A - 32)) & 1) << 1) | ((in1 >> (PIN_DIAL_ENC_B - 32)) & 1));
}

void IRAM_ATTR encIsr() {
  static const int8_t tab[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t cur = readEnc();
  s_enc += tab[(s_prev << 2) | cur];
  s_prev = cur;
}

// ---- Trafficmod LED ring (WS2812, bit-banged, interrupts off for ~0.8 ms per update) ---------------------
void ringWrite(const uint8_t rgb[25][3], uint8_t brightPct) {
  const uint32_t mask = 1u << PIN_DIAL_RING;
  const uint32_t mhz = getCpuFrequencyMhz();
  const uint32_t t0h = mhz * 350 / 1000, t1h = mhz * 700 / 1000, tbit = mhz * 1250 / 1000;
  uint8_t buf[75];
  for (int i = 0; i < 25; ++i) {
    buf[i * 3 + 0] = (uint8_t)(rgb[i][1] * brightPct / 100);  // G
    buf[i * 3 + 1] = (uint8_t)(rgb[i][0] * brightPct / 100);  // R
    buf[i * 3 + 2] = (uint8_t)(rgb[i][2] * brightPct / 100);  // B
  }
  noInterrupts();
  uint32_t start = ESP.getCycleCount();
  for (int i = 0; i < 75; ++i) {
    for (int b = 7; b >= 0; --b) {
      uint32_t th = (buf[i] >> b) & 1 ? t1h : t0h;
      while ((uint32_t)(ESP.getCycleCount() - start) < tbit) {}  // wait for the end of the previous bit
      start = ESP.getCycleCount();
      REG_WRITE(GPIO_OUT_W1TS_REG, mask);
      while ((uint32_t)(ESP.getCycleCount() - start) < th) {}
      REG_WRITE(GPIO_OUT_W1TC_REG, mask);
    }
  }
  interrupts();
}

// ---- task ------------------------------------------------------------------------------------------------
DialUi s_ui;
NavState s_nav;
uint16_t s_band[240 * 30];

void beep() {
#if DIAL_BEEP
  pwmWrite(PIN_DIAL_BUZZER, kChBuzzer, 128);
  delay(8);
  pwmWrite(PIN_DIAL_BUZZER, kChBuzzer, 0);
#endif
}

flightlog::Flight s_fl[flightlog::kMaxFlights];  // flight log snapshot for the display (dial task only)

void fillEnv(DialEnv& e) {
  memset(&e, 0, sizeof e);
  e.nFl = flights::snapshot(s_fl, flightlog::kMaxFlights, &e.flActive);
  e.fl = s_fl;
  e.fw = GS_FW_VERSION;
  e.hw = "3.0";
  e.simulating = g_stats.simActive != 0;
  e.bleAvail = ble::available();
  e.bleOn = g_settings.ble;  // takes effect after a reboot, like the setting itself
  e.bleConn = (uint8_t)ble::connections();
  strncpy(e.bleName, g_settings.bleName, sizeof e.bleName - 1);
  const bool ap = !g_settings.clientMode || g_apFallback;
  e.apMode = ap;
  if (ap) {
    snprintf(e.ip, sizeof e.ip, "%d.%d.%d.%d", AP_IP_A, AP_IP_B, AP_IP_C, AP_IP_D);
    if (g_apFallback) settingsDefaultCredentials(e.ssid, sizeof e.ssid, e.key, sizeof e.key);
    else {
      strncpy(e.ssid, g_settings.ssid, sizeof e.ssid - 1);
      strncpy(e.key, g_settings.wpakey, sizeof e.key - 1);
    }
    e.clients = (uint8_t)WiFi.softAPgetStationNum();
  } else {
    strncpy(e.ssid, g_settings.ssid, sizeof e.ssid - 1);
    strcpy(e.key, "(client mode)");
    String ip = WiFi.localIP().toString();
    strncpy(e.ip, ip.c_str(), sizeof e.ip - 1);
    e.clients = (uint8_t)g_stats.tcpClients;
  }
}

void applyOutputs(const Settings& s) {
  pwmWrite(PIN_DIAL_LCD_BL, kChBacklight, (uint32_t)s.dialBright * 255 / 100);
}

void task(void*) {
  pinMode(PIN_DIAL_HOLD, OUTPUT);
  digitalWrite(PIN_DIAL_HOLD, HIGH);
  pinMode(PIN_DIAL_BUTTON, INPUT_PULLUP);
  pinMode(PIN_DIAL_ENC_A, INPUT_PULLUP);
  pinMode(PIN_DIAL_ENC_B, INPUT_PULLUP);
  pinMode(PIN_DIAL_RING, OUTPUT);
  digitalWrite(PIN_DIAL_RING, LOW);
  pwmAttach(PIN_DIAL_LCD_BL, kChBacklight, 5000, 8);
  pwmAttach(PIN_DIAL_BUZZER, kChBuzzer, 4000, 8);
  pwmWrite(PIN_DIAL_BUZZER, kChBuzzer, 0);
  pwmWrite(PIN_DIAL_LCD_BL, kChBacklight, 0);

  lcdInit();
  s_prev = readEnc();
  attachInterrupt(PIN_DIAL_ENC_A, encIsr, CHANGE);
  attachInterrupt(PIN_DIAL_ENC_B, encIsr, CHANGE);

  s_ui.begin(g_settings, millis());
  applyOutputs(s_ui.settings());

  DialEnv env;
  gfx::Canvas canvas;
  int32_t lastEnc = s_enc;
  bool btn = false, longDone = false;
  uint32_t btnT = 0, lastDraw = 0, lastRing = 0;
  RingFrame lastFrame;
  memset(&lastFrame, 0xFF, sizeof lastFrame);
  bool ringWasOn = false;
  const uint32_t bootT0 = millis();
  uint32_t lastAnim = 0;
  bool animDone = false;

  for (;;) {
    uint32_t now = millis();

    // wheel
    int32_t e = s_enc;
    int32_t d = e - lastEnc;
    int steps = d / DIAL_ENC_TRANSITIONS_PER_DETENT;
    if (steps) {
      lastEnc += steps * DIAL_ENC_TRANSITIONS_PER_DETENT;
#if DIAL_ENC_INVERT
      steps = -steps;
#endif
      s_ui.onRotate(steps);
    }

    // button (active low, 25 ms debounce, long press >= 800 ms)
    bool down = digitalRead(PIN_DIAL_BUTTON) == LOW;
    if (down && !btn) {
      btn = true;
      longDone = false;
      btnT = now;
    } else if (!down && btn && now - btnT > 25) {
      btn = false;
      if (!longDone) s_ui.onClick();
    }
    if (btn && !longDone && now - btnT >= 800) {
      longDone = true;
      s_ui.onLongPress();
    }

    // start-up animation of the LED ring (once, if the ring is on)
    if (!animDone) {
      if (!s_ui.settings().ring || now - bootT0 >= (uint32_t)kBootRingMs) {
        animDone = true;
        memset(&lastFrame, 0xFF, sizeof lastFrame);  // hand the ring back to the normal logic
      } else if (now - lastAnim >= 35) {
        lastAnim = now;
        RingFrame f;
        computeBootRing(now - bootT0, f);
#if DIAL_RING_OFFSET || DIAL_RING_REVERSE
        RingFrame g = f;
        for (int i = 0; i < 24; ++i) {
          int src = DIAL_RING_REVERSE ? (24 - i) % 24 : i;
          memcpy(g.rgb[1 + (i + DIAL_RING_OFFSET) % 24], f.rgb[1 + src], 3);
        }
        f = g;
#endif
        ringWrite(f.rgb, s_ui.settings().ringBright);
        ringWasOn = true;
      }
    }

    if (s_ui.takeBeep()) beep();

    // actions requested by the menu
    DialAction act = s_ui.takeAction();
    if (act == kDialFlightDelete) {
      flights::removeByStart(s_ui.deleteStart());
      act = kDialNone;
    } else if (act == kDialFlightDeleteAll) {
      flights::clearAll();
      act = kDialNone;
    }
    if (act != kDialNone) {
      // draw the "restarting" screen once, then do the work
      if (navSnapshot(s_nav, 50)) {
        fillEnv(env);
        for (int y0 = 0; y0 < 240; y0 += 30) {
          canvas.setBand(s_band, y0, 30);
          s_ui.render(canvas, s_nav, env, now);
          lcdFlush(s_band, y0, 30);
        }
      }
      if (act == kDialFactoryReset) {
        settingsFactoryReset();
        delay(500);
        ESP.restart();
      }
      Settings keep = g_settings;  // save everything the menu may change, nothing else
      Settings ns = s_ui.settings();
      keep.dialBright = ns.dialBright;
      keep.ringBright = ns.ringBright;
      keep.ring = ns.ring;
      keep.nmeaTcp = ns.nmeaTcp;
      keep.gdl90 = ns.gdl90;
      keep.ble = ns.ble;
      keep.simWifi = ns.simWifi;
      keep.startPage = ns.startPage;
      keep.flightLog = ns.flightLog;
      keep.flightKmh = ns.flightKmh;
      settingsSave(keep);
      if (act == kDialSaveReboot) {
        delay(500);
        ESP.restart();
      }
      g_settings.dialBright = keep.dialBright;
      g_settings.ringBright = keep.ringBright;
      g_settings.ring = keep.ring;
      g_settings.flightLog = keep.flightLog;
      g_settings.flightKmh = keep.flightKmh;
    }
    // live preview of the brightness values while the menu edits them
    applyOutputs(s_ui.settings());

    // redraw at 5 Hz or immediately after input
    const uint32_t period = s_ui.bootAnimating() ? 40 : 200;  // boot animation ~25 fps
    if (s_ui.takeDirty() || now - lastDraw >= period) {
      lastDraw = now;
      if (navSnapshot(s_nav, 20)) {
        fillEnv(env);
        for (int y0 = 0; y0 < 240; y0 += 30) {
          canvas.setBand(s_band, y0, 30);
          s_ui.render(canvas, s_nav, env, now);
          lcdFlush(s_band, y0, 30);
          vTaskDelay(1);
        }
        // LED ring
        const Settings& cs = s_ui.settings();
        if (cs.ring && animDone) {
          RingFrame f;
          computeRing(s_nav, now, (uint8_t)g_stats.led, f);
#if DIAL_RING_OFFSET || DIAL_RING_REVERSE
          RingFrame g = f;
          for (int i = 0; i < 24; ++i) {
            int src = DIAL_RING_REVERSE ? (24 - i) % 24 : i;
            memcpy(g.rgb[1 + (i + DIAL_RING_OFFSET) % 24], f.rgb[1 + src], 3);
          }
          f = g;
#endif
          if (memcmp(&f, &lastFrame, sizeof f) != 0 || now - lastRing > 3000) {
            ringWrite(f.rgb, cs.ringBright);
            lastFrame = f;
            lastRing = now;
          }
          ringWasOn = true;
        } else if (ringWasOn && animDone) {
          RingFrame off;
          memset(&off, 0, sizeof off);
          ringWrite(off.rgb, 0);
          memset(&lastFrame, 0xFF, sizeof lastFrame);
          ringWasOn = false;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

bool supported() { return true; }

void begin() { xTaskCreatePinnedToCore(task, "dial", 8192, nullptr, 1, nullptr, 1); }

}  // namespace dialhw

#else  // not an ESP32-S3: hardware 3.0 is not available

namespace dialhw {
bool supported() { return false; }
void begin() {}
}  // namespace dialhw

#endif
