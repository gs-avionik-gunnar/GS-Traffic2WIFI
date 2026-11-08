#include "DialUi.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace gfx;

static const float kRangeTab[DialUi::kRangesNm] = {1.0f, 2.0f, 5.0f, 10.0f};
static const float kNm = 1852.0f;

void DialUi::begin(const Settings& s, uint32_t now) {
  set_ = s;
  orig_ = s;
  t0_ = now;
  // the stored start page numbers stay as in 4.0: 0 flight, 1 GNSS, 2 radar, 3 list, 4 WiFi, 5 Bluetooth, 6 QR, 7 info
  static const uint8_t kStartMap[8] = {kFlight, kGnss, kRadar, kList, kNet, kBle, kQr, kInfo};
  page_ = s.startPage < 8 ? kStartMap[s.startPage] : kFlight;
  dirty_ = true;
}

const uint8_t* DialUi::catItems(int cat, int& len) {
  static const uint8_t kDisplayItems[] = {kMiDisplay, kMiStart, kMiBack};
  static const uint8_t kRingItems[] = {kMiRing, kMiRingBright, kMiBack};
  static const uint8_t kOutItems[] = {kMiNmea, kMiBle, kMiGdl90, kMiBack};
  static const uint8_t kFlightItems[] = {kMiFlightLog, kMiFlightKmh, kMiBack};
  static const uint8_t kSysItems[] = {kMiSim, kMiRestart, kMiReset, kMiBack};
  switch (cat) {
    case 0: len = 3; return kDisplayItems;
    case 1: len = 3; return kRingItems;
    case 2: len = 4; return kOutItems;
    case 3: len = 3; return kFlightItems;
    default: len = 4; return kSysItems;
  }
}

int DialUi::listLen() const {
  if (menuCat_ < 0) return kCats + 1;  // categories + Back
  int n;
  catItems(menuCat_, n);
  return n;
}

int DialUi::curItem() const {
  if (menuCat_ < 0) return -1;
  int n;
  const uint8_t* it = catItems(menuCat_, n);
  return menuSel_ < n ? it[menuSel_] : kMiBack;
}

void DialUi::onRotate(int steps) {
  if (steps == 0 || rebooting_) return;
  beep_ = true;
  dirty_ = true;
  if (confirm_) {
    confirm_ = (steps > 0) ? 2 : 1;
    return;
  }
  if (page_ == kFlights && fview_) {
    fsel_ = (fsel_ + (steps > 0 ? 1 : fEntries() - 1)) % fEntries();  // endless
    return;
  }
  if (page_ == kMenu && menuOpen_) {
    if (editing_) {
      if (curItem() == kMiDisplay) {
        int v = (int)set_.dialBright + steps * 10;
        set_.dialBright = (uint8_t)(v < 10 ? 10 : (v > 100 ? 100 : v));
      } else if (curItem() == kMiStart) {
        int v = (int)set_.startPage + (steps > 0 ? 1 : -1);
        set_.startPage = (uint8_t)(v < 0 ? 0 : (v > 7 ? 7 : v));
      } else if (curItem() == kMiFlightKmh) {
        int v = (int)set_.flightKmh + steps * 5;
        set_.flightKmh = (uint16_t)(v < 20 ? 20 : (v > 300 ? 300 : v));
      } else if (curItem() == kMiRingBright) {
        int v = (int)set_.ringBright + steps * 5;
        set_.ringBright = (uint8_t)(v < 5 ? 5 : (v > 100 ? 100 : v));
      }
      return;
    }
    const int len = listLen();  // endless: after the last row comes the first one and vice versa
    menuSel_ = (menuSel_ + (steps > 0 ? 1 : len - 1)) % len;
    if (menuSel_ < menuTop_) menuTop_ = menuSel_;
    if (menuSel_ > menuTop_ + 4) menuTop_ = menuSel_ - 4;
    return;
  }
  page_ = (page_ + (steps > 0 ? 1 : kPageCount - 1)) % kPageCount;  // endless: after Info comes Flight data
}

void DialUi::leaveMenu() {
  editing_ = false;
  fview_ = 0;
  menuOpen_ = false;  // back on the closed Settings page
  menuCat_ = -1;
  bool needReboot = set_.nmeaTcp != orig_.nmeaTcp || set_.gdl90 != orig_.gdl90 || set_.ble != orig_.ble || set_.simWifi != orig_.simWifi;
  bool changed = needReboot || set_.dialBright != orig_.dialBright || set_.ringBright != orig_.ringBright ||
                 set_.ring != orig_.ring || set_.startPage != orig_.startPage || set_.flightLog != orig_.flightLog ||
                 set_.flightKmh != orig_.flightKmh;
  if (needReboot) {
    action_ = kDialSaveReboot;
    rebooting_ = true;
  } else if (changed) {
    action_ = kDialSave;
    orig_ = set_;
  }
}

void DialUi::onClick() {
  if (rebooting_) return;
  beep_ = true;
  dirty_ = true;
  if (confirm_) {
    if (confirm_ == 2) {
      if (confirmKind_ == 1) {
        action_ = kDialFlightDelete;
      } else if (confirmKind_ == 2) {
        action_ = kDialFlightDeleteAll;
        fsel_ = 0;
      } else {
        action_ = kDialFactoryReset;
        rebooting_ = true;
      }
    }
    confirm_ = 0;
    return;
  }
  if (page_ == kRadar) {
    rangeIdx_ = (rangeIdx_ + 1) % kRangesNm;
    return;
  }
  if (page_ == kFlights) {
    if (!fview_) {
      fview_ = 1;
      fsel_ = 0;
      return;
    }
    if (nFl_ == 0 || fsel_ > nFl_) {
      fview_ = 0;  // Back
    } else if (fsel_ == nFl_) {
      confirmKind_ = 2;
      confirm_ = 1;
    } else {
      delStart_ = flStart_[fsel_];
      confirmKind_ = 1;
      confirm_ = 1;
    }
    return;
  }
  if (page_ != kMenu) return;
  if (!menuOpen_) {
    menuOpen_ = true;
    menuCat_ = -1;
    menuSel_ = 0;
    menuTop_ = 0;
    return;
  }
  if (editing_) {
    editing_ = false;
    return;
  }
  if (menuCat_ < 0) {  // top level: open a category, or leave
    if (menuSel_ >= kCats) {
      leaveMenu();
    } else {
      menuCat_ = menuSel_;
      menuSel_ = 0;
      menuTop_ = 0;
    }
    return;
  }
  switch (curItem()) {
    case kMiDisplay:
    case kMiStart:
    case kMiRingBright:
    case kMiFlightKmh: editing_ = true; break;
    case kMiFlightLog: set_.flightLog = !set_.flightLog; break;
    case kMiRing: set_.ring = !set_.ring; break;
    case kMiNmea: set_.nmeaTcp = !set_.nmeaTcp; break;
    case kMiBle: set_.ble = !set_.ble; break;
    case kMiGdl90: set_.gdl90 = !set_.gdl90; break;
    case kMiSim: set_.simWifi = !set_.simWifi; break;
    case kMiRestart:  // saves pending changes and restarts
      action_ = kDialSaveReboot;
      rebooting_ = true;
      break;
    case kMiReset: confirmKind_ = 0; confirm_ = 1; break;
    case kMiBack:  // back to the category list
      menuSel_ = menuCat_;
      menuCat_ = -1;
      menuTop_ = 0;
      break;
  }
}

void DialUi::onLongPress() {
  if (rebooting_) return;
  beep_ = true;
  dirty_ = true;
  confirm_ = 0;
  fview_ = 0;
  if (page_ == kMenu && menuOpen_) leaveMenu();
  menuOpen_ = false;
  menuCat_ = -1;
  page_ = kFlight;
  editing_ = false;
}

// ---------------------------------------------------------------------------------------------

static void fmtTime(char* b, size_t n, uint32_t s) {
  snprintf(b, n, "%02u:%02u:%02u", (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60), (unsigned)(s % 60));
}

void DialUi::title(Canvas& c, const char* t, int y) { c.text(kFontMedium, 120, y, Center, kGreen, t); }

void DialUi::drawDots(Canvas& c) {
  const int n = kPageCount, gap = 11;
  int x0 = 120 - (n - 1) * gap / 2;
  for (int i = 0; i < n; ++i) c.fillCircle(x0 + i * gap, 228, i == page_ ? 4 : 2, i == page_ ? kGreen : kGrey);
}

bool DialUi::displayAltitudeFt(const NavState& n, uint32_t now, int32_t& ft, bool& pressure) {
  if (n.altFresh(now)) {
    ft = (int32_t)lroundf(n.altMslM * 3.28084f);
    pressure = false;
    return true;
  }
  if (n.baroFresh(now)) {
    ft = n.baroAltFt;
    pressure = true;
    return true;
  }
  return false;
}

static bool dataOk(const NavState& n, uint32_t now) { return n.lastRxMs != 0 && NavState::age(now, n.lastRxMs) < 5000; }

void DialUi::drawFlight(Canvas& c, const NavState& n, uint32_t now) {
  bool pos = n.posFresh(now);
  char v[16];
  struct Row { const char* unit; int y; };
  const Row rows[3] = {{"kt", 22}, {"ft", 76}, {"deg", 130}};
  for (int i = 0; i < 3; ++i) {
    bool ok = false;
    if (i == 0 && pos && n.sogValid) { snprintf(v, sizeof v, "%d", (int)lroundf(n.sogKt)); ok = true; }
    // Altitude: GPS altitude above mean sea level (GGA), like the "real" altitude in the EFB. The pressure
    // altitude (PGRMZ, 1013 hPa) differs from it by the QNH deviation, e.g. -543 ft at 20 ft elevation.
    int32_t altFt;
    bool altPressure;
    if (i == 1 && displayAltitudeFt(n, now, altFt, altPressure)) { snprintf(v, sizeof v, "%ld", (long)altFt); ok = true; }
    if (i == 2 && pos && n.trackValid) { snprintf(v, sizeof v, "%03d", ((int)lroundf(n.trackDeg)) % 360); ok = true; }
    if (!ok) strcpy(v, "---");
    uint16_t col = ok ? kWhite : kGrey;
    int wv = Canvas::textWidth(kFontLarge, v), wu = Canvas::textWidth(kFontSmall, rows[i].unit);
    int x = 120 - (wv + 6 + wu) / 2;
    c.text(kFontLarge, x, rows[i].y, Left, col, v);
    c.text(kFontSmall, x + wv + 6, rows[i].y + 25, Left, kGreen, rows[i].unit);
  }
  char lab[40];
  if (n.altFresh(now)) {
    if (n.baroFresh(now)) snprintf(lab, sizeof lab, "GPS MSL  PA %ld", (long)n.baroAltFt);  // pressure altitude as reference
    else strcpy(lab, "GPS MSL");
  } else {
    strcpy(lab, n.baroFresh(now) ? "pressure altitude" : "");
  }
  c.text(kFontTiny, 120, 188, Center, kGrey, lab);
  drawDots(c);
}

void DialUi::drawGnss(Canvas& c, const NavState& n, uint32_t now) {
  title(c, "GNSS", 14);
  bool data = dataOk(n, now), pos = n.posFresh(now);
  char b[32];
  snprintf(b, sizeof b, "%u", (unsigned)n.sats);
  c.text(kFontLarge, 120, 44, Center, pos ? kWhite : kRed, b);
  c.text(kFontSmall, 120, 96, Center, kGrey, "satellites");
  const char* st = !data ? "No data" : (pos ? "Position OK" : "No position");
  c.text(kFontMedium, 120, 124, Center, pos ? kGreen : kRed, st);
  if (n.timeFresh(now)) {
    char t[16];
    fmtTime(t, sizeof t, n.secOfDay + NavState::age(now, n.timeMs) / 1000);
    snprintf(b, sizeof b, "%s UTC", t);
    c.text(kFontMedium, 120, 156, Center, kWhite, b);
  } else {
    c.text(kFontMedium, 120, 156, Center, kGrey, "--:--:--");
  }
  if (n.accFresh(now)) {
    snprintf(b, sizeof b, "HDOP %.1f", (double)n.hdop);
    c.text(kFontSmall, 120, 190, Center, kGrey, b);
  }
  drawDots(c);
}

static uint16_t alarmColor(int alarm) { return alarm >= 3 ? kRed : (alarm == 2 ? kOrange : (alarm == 1 ? kYellow : kCyan)); }

// Altitude of target relative to ownship in 100 ft, false when unknown.
static bool relAlt100(const NavState& n, const Target& t, uint32_t now, int& out) {
  int32_t own;
  if (!t.altValid || !n.refAltitudeFt(now, own)) return false;
  out = (int)lroundf((float)(t.altFt - own) / 100.0f);
  return true;
}

void DialUi::drawRadar(Canvas& c, const NavState& n, uint32_t now) {
  const int cx = 120, cy = 126, R = 86;
  float range = kRangeTab[rangeIdx_] * kNm;
  bool trackUp = n.posFresh(now) && n.trackValid && n.sogValid && n.sogKt > 5.0f;
  float rot = trackUp ? n.trackDeg : 0.0f;
  char b[24];
  snprintf(b, sizeof b, "%g NM  %s", (double)kRangeTab[rangeIdx_], trackUp ? "TRK" : "N");
  c.text(kFontSmall, 120, 12, Center, kGreen, b);
  c.ring(cx, cy, R, 2, kGrey);
  c.ring(cx, cy, R / 2, 1, kDark);
  c.line(cx - R, cy, cx + R, cy, 1, kDark);
  c.line(cx, cy - R, cx, cy + R, 1, kDark);
  // north marker on the rim
  float na = -rot * 0.01745329f;
  c.text(kFontSmall, cx + (int)(sinf(na) * (R + 0)) , cy - (int)(cosf(na) * (R + 0)) - 8, Center, kYellow, "N");

  // targets, farthest first so near ones are on top
  int idx[TrafficTable::kMax], cnt = 0;
  for (int i = 0; i < TrafficTable::kMax; ++i)
    if (n.traffic.slot(i).used) idx[cnt++] = i;
  for (int a = 1; a < cnt; ++a)  // insertion sort by distance descending
    for (int k = a; k > 0 && n.traffic.slot(idx[k]).distM > n.traffic.slot(idx[k - 1]).distM; --k) {
      int t = idx[k]; idx[k] = idx[k - 1]; idx[k - 1] = t;
    }
  for (int q = 0; q < cnt; ++q) {
    const Target& t = n.traffic.slot(idx[q]);
    float d = t.distM / range;
    bool clipped = d > 1.0f;
    if (clipped) d = 1.0f;
    float a = (t.bearingDeg - rot) * 0.01745329f;
    int x = cx + (int)lroundf(sinf(a) * d * (R - 6)), y = cy - (int)lroundf(cosf(a) * d * (R - 6));
    uint16_t col = alarmColor(t.alarm);
    if (clipped) col = kGrey;
    float hdg = t.trackValid ? (t.trackDeg - rot) : (t.bearingDeg - rot);
    if (t.trackValid) c.marker(x, y, 8, hdg, col);
    else c.fillCircle(x, y, 4, col);
    int rel;
    if (!clipped && relAlt100(n, t, now, rel)) {
      snprintf(b, sizeof b, "%+d", rel);
      c.text(kFontSmall, x + 8, y - 8, Left, col, b);
    }
  }
  // ownship
  c.marker(cx, cy, 6, 0, kWhite);
  if (cnt == 0) c.text(kFontSmall, 120, 182, Center, kGrey, "no traffic");
}

void DialUi::drawList(Canvas& c, const NavState& n, uint32_t now) {
  title(c, "Traffic", 8);
  int idx[TrafficTable::kMax], cnt = 0;
  for (int i = 0; i < TrafficTable::kMax; ++i)
    if (n.traffic.slot(i).used) idx[cnt++] = i;
  // alarm level first, then distance
  for (int a = 1; a < cnt; ++a)
    for (int k = a; k > 0; --k) {
      const Target &x = n.traffic.slot(idx[k]), &y = n.traffic.slot(idx[k - 1]);
      bool before = x.alarm > y.alarm || (x.alarm == y.alarm && x.distM < y.distM);
      if (!before) break;
      int t = idx[k]; idx[k] = idx[k - 1]; idx[k - 1] = t;
    }
  if (cnt == 0) {
    c.text(kFontMedium, 120, 100, Center, kGrey, "no traffic");
    drawDots(c);
    return;
  }
  for (int r = 0; r < 5 && r < cnt; ++r) {
    const Target& t = n.traffic.slot(idx[r]);
    int y = 38 + r * 34;
    uint16_t col = t.alarm ? alarmColor(t.alarm) : kWhite;
    char b[24];
    float nm = t.distM / kNm;
    if (nm < 10) snprintf(b, sizeof b, "%.1f", (double)nm); else snprintf(b, sizeof b, "%d", (int)lroundf(nm));
    int w = c.text(kFontMedium, 92, y, Right, col, b);
    c.text(kFontSmall, 95, y + 7, Left, kGrey, "NM");
    (void)w;
    int rel;
    if (relAlt100(n, t, now, rel)) {
      snprintf(b, sizeof b, "%+d", rel);
      c.text(kFontMedium, 186, y, Right, col, b);
    } else {
      c.text(kFontMedium, 186, y, Right, kGrey, "--");
    }
    if (t.climbValid && fabsf(t.climbFpm) > 100.0f) {
      if (t.climbFpm > 0) c.fillTriangle(196, y + 20, 204, y + 20, 200, y + 8, col);
      else c.fillTriangle(196, y + 8, 204, y + 8, 200, y + 20, col);
    }
  }
  drawDots(c);
}

void DialUi::drawNet(Canvas& c, const DialEnv& e) {
  title(c, "WiFi", 14);
  char b[40];
  c.text(kFontSmall, 120, 40, Center, kGrey, "SSID");
  c.text(kFontSmall, 120, 58, Center, kWhite, e.ssid);
  c.text(kFontSmall, 120, 86, Center, kGrey, "Key");
  c.text(kFontSmall, 120, 104, Center, kWhite, e.key);
  c.text(kFontSmall, 120, 132, Center, kGrey, "IP");
  c.text(kFontSmall, 120, 150, Center, kWhite, e.ip);
  snprintf(b, sizeof b, "%u client%s%s", (unsigned)e.clients, e.clients == 1 ? "" : "s", e.simulating ? "  SIM" : "");
  c.text(kFontSmall, 120, 182, Center, e.clients ? kGreen : kGrey, b);
  drawDots(c);
}

void DialUi::drawBle(Canvas& c, const DialEnv& e) {
  title(c, "Bluetooth", 14);
  if (!e.bleAvail || !e.bleOn) {
    c.text(kFontSmall, 120, 96, Center, kGrey, "Bluetooth LE is off");
    c.text(kFontTiny, 120, 124, Center, kGrey, "enable it in Settings");
    drawDots(c);
    return;
  }
  char b[40];
  c.text(kFontSmall, 120, 44, Center, kGrey, "Name");
  c.text(kFontSmall, 120, 62, Center, kWhite, e.bleName);
  c.text(kFontSmall, 120, 94, Center, kGrey, "Connected");
  snprintf(b, sizeof b, "%u device%s", (unsigned)e.bleConn, e.bleConn == 1 ? "" : "s");
  c.text(kFontMedium, 120, 112, Center, e.bleConn ? kGreen : kGrey, b);
  c.text(kFontTiny, 120, 154, Center, kGrey, "NMEA, HM-10 compatible");
  drawDots(c);
}

void DialUi::drawQr(Canvas& c, const DialEnv& e) {
  if (!e.apMode) {
    title(c, "WiFi QR", 14);
    c.text(kFontSmall, 120, 100, Center, kGrey, "Client mode:");
    c.text(kFontSmall, 120, 122, Center, kGrey, "no QR code");
    drawDots(c);
    return;
  }
  c.text(kFontSmall, 120, 8, Center, kGreen, "WiFi");
  char text[160];
  if (!qr::wifiString(e.ssid, e.key, text, sizeof text)) text[0] = '\0';
  if (strcmp(text, qrText_) != 0 || (!qrOk_ && text[0])) {  // cache: build the code only when SSID/key changed
    strcpy(qrText_, text);
    qrOk_ = text[0] && qr::encode(text, qr_);
  }
  if (!qrOk_) {
    c.text(kFontSmall, 120, 100, Center, kRed, "QR: SSID/key");
    c.text(kFontSmall, 120, 122, Center, kRed, "too long");
    drawDots(c);
    return;
  }
  // The display is round: the white square (code + 2 module quiet zone) must fit into a 169 px square.
  int mods = qr_.size + 4;
  int px = 169 / mods;
  if (px < 2) px = 2;
  int side = mods * px, x0 = 120 - side / 2, y0 = 118 - side / 2;
  c.fillRect(x0, y0, side, side, kWhite);
  for (int y = 0; y < qr_.size; ++y)
    for (int x = 0; x < qr_.size; ++x)
      if (qr_.m[y][x]) c.fillRect(x0 + (x + 2) * px, y0 + (y + 2) * px, px, px, kBlack);
  drawDots(c);
}

void DialUi::drawInfo(Canvas& c, const DialEnv& e) {
  char b[40];
  c.text(kFontMedium, 120, 36, Center, kGreen, "GS Avionik");
  c.text(kFontSmall, 120, 74, Center, kWhite, "GS-Traffic2WIFI");
  snprintf(b, sizeof b, "Firmware %s", e.fw);
  c.text(kFontSmall, 120, 106, Center, kGrey, b);
  snprintf(b, sizeof b, "Hardware v%c", e.hw[0]);
  c.text(kFontSmall, 120, 128, Center, kGrey, b);
  c.text(kFontSmall, 120, 168, Center, kGreen, "www.gs-avionik.de");
  drawDots(c);
}

void DialUi::drawMenu(Canvas& c) {
  static const char* const catNames[kCats] = {"Display", "LED ring", "Outputs", "Flight log", "System"};
  title(c, menuOpen_ && menuCat_ >= 0 ? catNames[menuCat_] : "Settings", 12);
  if (!menuOpen_) {
    c.text(kFontSmall, 120, 96, Center, kGrey, "Press the wheel");
    c.text(kFontSmall, 120, 118, Center, kGrey, "to open");
    drawDots(c);
    return;
  }
  static const char* const labels[] = {"Brightness", "LED ring", "LED bright.", "Start page", "NMEA TCP", "NMEA BLE", "GDL90 UDP", "Flight log", "Log km/h", "Simulator", "Restart", "Factory reset", "Back"};
  const int total = listLen();
  for (int r = 0; r < 5; ++r) {
    int i = menuTop_ + r;
    if (i >= total) break;
    int y = 42 + r * 32;
    bool sel = i == menuSel_;
    if (sel) c.roundRect(24, y - 2, 192, 28, editing_ ? kGreen : kGrey, false);
    uint16_t col = sel ? kWhite : kGrey;
    if (menuCat_ < 0) {  // category list
      c.text(kFontSmall, 38, y + 2, Left, col, i < kCats ? catNames[i] : "Back");
      if (i < kCats) c.text(kFontSmall, 202, y + 2, Right, col, ">");
      continue;
    }
    int n;
    int item = catItems(menuCat_, n)[i];
    c.text(kFontSmall, 38, y + 2, Left, col, labels[item]);
    char b[16] = "";
    const char* v = b;
    uint16_t vc = col;
    switch (item) {
      case kMiDisplay: snprintf(b, sizeof b, "%u%%", set_.dialBright); break;
      case kMiRingBright: snprintf(b, sizeof b, "%u%%", set_.ringBright); break;
      case kMiFlightKmh: snprintf(b, sizeof b, "%u", (unsigned)set_.flightKmh); break;
      case kMiFlightLog: v = set_.flightLog ? "ON" : "OFF"; vc = set_.flightLog ? kGreen : kGrey; break;
      case kMiStart: {
        static const char* const names[8] = {"Flight", "GNSS", "Radar", "List", "WiFi", "BT", "QR", "Info"};
        v = names[set_.startPage < 8 ? set_.startPage : 0];
        break;
      }
      case kMiRing: v = set_.ring ? "ON" : "OFF"; vc = set_.ring ? kGreen : kGrey; break;
      case kMiNmea: v = set_.nmeaTcp ? "ON" : "OFF"; vc = set_.nmeaTcp ? kGreen : kGrey; break;
      case kMiBle: v = set_.ble ? "ON" : "OFF"; vc = set_.ble ? kGreen : kGrey; break;
      case kMiGdl90: v = set_.gdl90 ? "ON" : "OFF"; vc = set_.gdl90 ? kGreen : kGrey; break;
      case kMiSim: v = set_.simWifi ? "ON" : "OFF"; vc = set_.simWifi ? kRed : kGrey; break;  // test data replaces the real input
      default: break;
    }
    if (*v) c.text(kFontSmall, 202, y + 2, Right, sel && editing_ ? kGreen : vc, v);
  }
  drawDots(c);
}

void DialUi::drawConfirm(Canvas& c) {
  const char* head = "Factory reset?";
  const char* sub = "All settings are lost";
  const char* ok = "Reset now";
  if (confirmKind_ == 1) {
    head = "Delete flight?";
    sub = "Cannot be undone";
    ok = "Delete";
  } else if (confirmKind_ == 2) {
    head = "Delete all?";
    sub = "All flights are lost";
    ok = "Delete all";
  }
  c.text(kFontMedium, 120, 52, Center, kRed, head);
  c.text(kFontSmall, 120, 88, Center, kWhite, sub);
  c.roundRect(40, 126, 160, 32, confirm_ == 1 ? kGreen : kGrey, false);
  c.text(kFontSmall, 120, 133, Center, confirm_ == 1 ? kWhite : kGrey, "Cancel");
  c.roundRect(40, 166, 160, 32, confirm_ == 2 ? kRed : kGrey, false);
  c.text(kFontSmall, 120, 173, Center, confirm_ == 2 ? kWhite : kGrey, ok);
}

// days since 1970-01-01 -> civil date (Howard Hinnant)
static void civilFromDays(int32_t z, int& y, int& m, int& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (int)yoe + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  if (m <= 2) ++y;
}

void DialUi::drawFlights(Canvas& c, const DialEnv& e) {
  title(c, "Flights", 12);
  if (!fview_) {
    char n[24];
    snprintf(n, sizeof n, "%d recorded", nFl_);
    c.text(kFontSmall, 120, 80, Center, kWhite, n);
    if (e.flActive) c.text(kFontTiny, 120, 104, Center, kGreen, "flight in progress");
    c.text(kFontSmall, 120, 130, Center, kGrey, "Press the wheel");
    c.text(kFontSmall, 120, 152, Center, kGrey, "to open");
    drawDots(c);
    return;
  }
  if (nFl_ == 0) {
    c.text(kFontSmall, 120, 96, Center, kGrey, "No flights");
    c.text(kFontTiny, 120, 124, Center, kGrey, e.flActive ? "flight in progress" : "recorded");
    c.text(kFontSmall, 120, 160, Center, kWhite, "Back");
    return;
  }
  char b[40];
  if (fsel_ < nFl_) {
    const flightlog::Flight& f = e.fl[fsel_];
    int y, m, d;
    civilFromDays((int32_t)(f.start / 86400u), y, m, d);
    uint32_t sod = f.start % 86400u, mins = (f.dur + 30) / 60;
    snprintf(b, sizeof b, "%d/%d", fsel_ + 1, nFl_);
    c.text(kFontSmall, 120, 44, Center, kGrey, b);
    snprintf(b, sizeof b, "%04d-%02d-%02d", y, m, d);
    c.text(kFontMedium, 120, 62, Center, kWhite, b);
    uint32_t eod = (f.start + f.dur) % 86400u;
    snprintf(b, sizeof b, "%02u:%02u - %02u:%02u UTC", (unsigned)(sod / 3600), (unsigned)(sod / 60 % 60), (unsigned)(eod / 3600), (unsigned)(eod / 60 % 60));
    c.text(kFontSmall, 120, 92, Center, kGreen, b);
    snprintf(b, sizeof b, "Duration %u:%02u h", (unsigned)(mins / 60), (unsigned)(mins % 60));
    c.text(kFontSmall, 120, 114, Center, kWhite, b);
    snprintf(b, sizeof b, "Start %.4f %.4f", f.sLat / 1e5, f.sLon / 1e5);
    c.text(kFontTiny, 120, 142, Center, kGrey, b);
    snprintf(b, sizeof b, "End   %.4f %.4f", f.eLat / 1e5, f.eLon / 1e5);
    c.text(kFontTiny, 120, 158, Center, kGrey, b);
    c.text(kFontTiny, 120, 188, Center, kGrey, "click = delete");
  } else if (fsel_ == nFl_) {
    snprintf(b, sizeof b, "%d flights", nFl_);
    c.text(kFontSmall, 120, 80, Center, kGrey, b);
    c.text(kFontMedium, 120, 104, Center, kRed, "Delete all");
    c.text(kFontTiny, 120, 188, Center, kGrey, "click = delete");
  } else {
    c.text(kFontMedium, 120, 104, Center, kWhite, "Back");
  }
}

void DialUi::drawSplash(Canvas& c, const DialEnv& e) {
  c.text(kFontMedium, 120, 80, Center, kGreen, "GS Avionik");
  c.text(kFontSmall, 120, 114, Center, kWhite, "GS-Traffic2WIFI");
  char b[32];
  snprintf(b, sizeof b, "v%s  HW %s", e.fw, e.hw);
  c.text(kFontSmall, 120, 138, Center, kGrey, b);
}

void DialUi::render(Canvas& c, const NavState& nav, const DialEnv& env, uint32_t now) {
  c.clear(kBlack);
  nFl_ = env.fl ? (env.nFl < flightlog::kMaxFlights ? env.nFl : (int)flightlog::kMaxFlights) : 0;
  for (int i = 0; i < nFl_; ++i) flStart_[i] = env.fl[i].start;
  if (fsel_ >= fEntries()) fsel_ = fEntries() - 1;
  if (rebooting_) {
    c.text(kFontMedium, 120, 100, Center, kGreen, action_ == kDialFactoryReset ? "Resetting..." : "Restarting...");
    return;
  }
  if (NavState::age(now, t0_) < 2500) {
    drawSplash(c, env);
    return;
  }
  if (confirm_) {
    drawConfirm(c);
    return;
  }
  switch (page_) {
    case kFlight: drawFlight(c, nav, now); break;
    case kGnss: drawGnss(c, nav, now); break;
    case kRadar: drawRadar(c, nav, now); drawDots(c); break;
    case kList: drawList(c, nav, now); break;
    case kNet: drawNet(c, env); break;
    case kBle: drawBle(c, env); break;
    case kQr: drawQr(c, env); break;
    case kInfo: drawInfo(c, env); break;
    case kMenu:
      drawMenu(c);
      break;
    case kFlights: drawFlights(c, env); break;
  }
}

// ---------------------------------------------------------------------------------------------

void computeRing(const NavState& nav, uint32_t now, uint8_t statusLed, RingFrame& out) {
  memset(&out, 0, sizeof out);
  switch (statusLed) {
    case 1: out.rgb[0][1] = 255; break;
    case 2: out.rgb[0][0] = 255; break;
    case 3: out.rgb[0][0] = 255; out.rgb[0][1] = 110; break;
    default: break;
  }
  float rot = (nav.posFresh(now) && nav.trackValid && nav.sogValid && nav.sogKt > 5.0f) ? nav.trackDeg : 0.0f;
  uint8_t prio[24];
  memset(prio, 0, sizeof prio);
  for (int i = 0; i < TrafficTable::kMax; ++i) {
    const Target& t = nav.traffic.slot(i);
    if (!t.used || t.alarm == 0 || t.distM > 5.0f * 1852.0f) continue;  // only targets with an alarm
    float rel = t.bearingDeg - rot;
    while (rel < 0) rel += 360.0f;
    while (rel >= 360.0f) rel -= 360.0f;
    int led = (int)lroundf(rel / 15.0f) % 24;
    uint8_t p = t.alarm > 3 ? 3 : t.alarm;  // alarm level 1..3
    if (p > prio[led]) prio[led] = p;
  }
  for (int l = 0; l < 24; ++l) {
    uint8_t* px = out.rgb[1 + l];
    switch (prio[l]) {
      case 1: px[0] = 200; px[1] = 200; px[2] = 0; break;     // alarm level 1: yellow
      case 2: px[0] = 255; px[1] = 110; px[2] = 0; break;     // alarm level 2: orange
      case 3: px[0] = 255; px[1] = 0; px[2] = 0; break;       // alarm level 3: red
      default: break;
    }
  }
}

static void hsv(float h, float v, uint8_t* rgb) {  // h in degrees, full saturation
  h = fmodf(h, 360.0f);
  float x = 1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f), r, g, b;
  int k = (int)(h / 60.0f);
  switch (k) {
    case 0: r = 1; g = x; b = 0; break;
    case 1: r = x; g = 1; b = 0; break;
    case 2: r = 0; g = 1; b = x; break;
    case 3: r = 0; g = x; b = 1; break;
    case 4: r = x; g = 0; b = 1; break;
    default: r = 1; g = 0; b = x; break;
  }
  rgb[0] = (uint8_t)(r * v * 255.0f);
  rgb[1] = (uint8_t)(g * v * 255.0f);
  rgb[2] = (uint8_t)(b * v * 255.0f);
}

void computeBootRing(uint32_t elapsedMs, RingFrame& out) {
  memset(&out, 0, sizeof out);
  const float sweepMs = 1300.0f, tail = 8.0f;
  float head = (float)elapsedMs * 24.0f / sweepMs;  // LED position of the comet head, runs past 24 while the tail fades
  for (int i = 0; i < 24; ++i) {
    float d = head - (float)i;  // how far the head has passed this LED
    if (d < 0.0f || d >= tail) continue;
    float v = 1.0f - d / tail;
    hsv((float)i * 15.0f, v * v, out.rgb[1 + i]);  // colour changes around the circle
  }
}
