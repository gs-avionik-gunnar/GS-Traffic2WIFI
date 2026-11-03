// Page logic and drawing for the hardware 3.0 round display (M5 Dial).
// Pure C++ (no Arduino dependencies) so it can be rendered and tested on a PC.
// Input: wheel rotation, short click, long press. Output: pages drawn into a gfx::Canvas
// and a few actions (save settings, reboot, factory reset) for the hardware layer.
#pragma once
#include <stdint.h>

#include "Gfx.h"
#include "Qr.h"
#include "Settings.h"
#include "State.h"

struct DialEnv {
  char ip[16];        // address of the module as seen by the clients
  char ssid[33];
  char key[64];
  uint8_t clients;    // WiFi stations (AP mode) or TCP clients
  bool apMode;
  bool simulating;
  bool bleAvail, bleOn;   // Bluetooth LE built in / switched on
  uint8_t bleConn;        // connected phones
  char bleName[21];
  const char* fw;
  const char* hw;
};

enum DialAction { kDialNone = 0, kDialSave, kDialSaveReboot, kDialFactoryReset };

class DialUi {
 public:
  enum Page { kFlight, kGnss, kRadar, kList, kNet, kBle, kQr, kInfo, kMenu, kPageCount };

  void begin(const Settings& s, uint32_t now);  // the first page is s.startPage
  void onRotate(int steps);  // + clockwise
  void onClick();
  void onLongPress();
  // Draws the whole screen into the current band of c.
  void render(gfx::Canvas& c, const NavState& nav, const DialEnv& env, uint32_t now);

  // Results for the hardware layer
  DialAction takeAction() { DialAction a = action_; action_ = kDialNone; return a; }
  const Settings& settings() const { return set_; }
  bool takeBeep() { bool b = beep_; beep_ = false; return b; }
  bool takeDirty() { bool d = dirty_; dirty_ = false; return d; }
  int page() const { return page_; }
  void gotoPage(int p) { page_ = p; dirty_ = true; }

  // Test hooks
  int menuIndex() const { return menuSel_; }
  bool editing() const { return editing_; }
  bool menuOpen() const { return menuOpen_; }
  int confirmState() const { return confirm_; }

  // Altitude shown on the flight page: GPS altitude above MSL, pressure altitude only as fallback.
  static bool displayAltitudeFt(const NavState& n, uint32_t now, int32_t& ft, bool& pressure);

  static const int kRangesNm = 4;

 private:
  enum MenuItem { kMiDisplay, kMiRing, kMiRingBright, kMiStart, kMiNmea, kMiBle, kMiGdl90, kMiRestart, kMiReset, kMiBack, kMiCount };
  void drawFlight(gfx::Canvas& c, const NavState& n, uint32_t now);
  void drawGnss(gfx::Canvas& c, const NavState& n, uint32_t now);
  void drawRadar(gfx::Canvas& c, const NavState& n, uint32_t now);
  void drawList(gfx::Canvas& c, const NavState& n, uint32_t now);
  void drawNet(gfx::Canvas& c, const DialEnv& e);
  void drawQr(gfx::Canvas& c, const DialEnv& e);
  void drawBle(gfx::Canvas& c, const DialEnv& e);
  void drawInfo(gfx::Canvas& c, const DialEnv& e);
  void drawMenu(gfx::Canvas& c);
  void drawConfirm(gfx::Canvas& c);
  void drawSplash(gfx::Canvas& c, const DialEnv& e);
  void drawDots(gfx::Canvas& c);
  void title(gfx::Canvas& c, const char* t, int y = 12);
  void leaveMenu();

  qr::Code qr_;
  char qrText_[160] = "";  // text the cached code was made from
  bool qrOk_ = false;
  Settings set_;      // working copy of the settings
  Settings orig_;     // as loaded, to detect changes that need a reboot
  uint32_t t0_ = 0;
  int page_ = kFlight;
  int rangeIdx_ = 1;  // 1, 2, 5, 10 NM
  int menuSel_ = 0, menuTop_ = 0;
  bool editing_ = false;
  bool menuOpen_ = false;  // the Settings page is only shown until the wheel is pressed
  int confirm_ = 0;   // 0 off, 1 = Cancel selected, 2 = Reset selected
  DialAction action_ = kDialNone;
  bool beep_ = false;
  bool dirty_ = true;
  bool rebooting_ = false;
};

// Trafficmod LED ring: LED 0 = status, LEDs 1..24 = direction of traffic relative to the track.
struct RingFrame {
  uint8_t rgb[25][3];
};
void computeRing(const NavState& nav, uint32_t now, uint8_t statusLed, RingFrame& out);
