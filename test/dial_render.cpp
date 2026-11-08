// Renders every dial page to PPM files (host tool, used to check the layouts by eye and in CI).
#include <stdio.h>
#include <string.h>

#include "DialUi.h"
#include "Settings.h"



static uint16_t buf[240 * 30];
static DialEnv env = {"192.168.1.1", "GS-Traffic-a1b2", "c3d4a1b2", 2, true, false, true, true, 1, "GS-Traffic-a1b2", "4.0", "3.0"};

static void dump(DialUi& ui, const NavState& n, uint32_t now, const char* name) {
  FILE* f = fopen(name, "wb");
  fprintf(f, "P6\n240 240\n255\n");
  gfx::Canvas c;
  for (int y0 = 0; y0 < 240; y0 += 30) {
    c.setBand(buf, y0, 30);
    ui.render(c, n, env, now);
    for (int i = 0; i < 240 * 30; ++i) {
      uint16_t v = (uint16_t)((buf[i] << 8) | (buf[i] >> 8));
      int x = i % 240, y = y0 + i / 240;
      bool inside = (x - 119.5) * (x - 119.5) + (y - 119.5) * (y - 119.5) <= 120.0 * 120.0;
      uint8_t px[3] = {(uint8_t)(((v >> 11) & 31) << 3), (uint8_t)(((v >> 5) & 63) << 2), (uint8_t)((v & 31) << 3)};
      if (!inside) { px[0] = px[1] = px[2] = 60; if ((x+y)&1) px[0]=px[1]=px[2]=90; }
      fwrite(px, 1, 3, f);
    }
  }
  fclose(f);
}

int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : ".";
  char name[256];
  Settings s;
  memset(&s, 0, sizeof s);
  s.dialBright = 80; s.ringBright = 30; s.nmeaTcp = true; s.gdl90 = true;
  NavState n;
  uint32_t now = 10000;
  const char* lines[] = {
      "$GPRMC,123456.00,A,5230.0000,N,01330.0000,E,95.0,87.0,011125,,,A",
      "$GPGGA,123456.00,5230.0000,N,01330.0000,E,1,9,1.1,1234.0,M,45.0,M,,",
      "$PGRMZ,3500,f,3",
      "$PFLAU,3,1,2,1,2,45,2,-120,800,AB12CD",
      "$PFLAA,2,-800,900,-120,1,AB12CD,270,,95,2.1,8",
      "$PFLAA,0,3000,-1500,300,1,112233,90,,70,-1.0,1",
      "$PFLAA,0,-2000,-2500,-500,1,445566,10,,60,0,1",
      "$PFLAA,0,9000,6000,1000,1,778899,200,,60,0,1",
      "$PFLAA,1,500,-300,50,1,A1B2C3,180,,120,3.0,3",
  };
  for (const char* l : lines) n.ingestLine(l, now);
  DialUi ui;
  ui.begin(s, 0);
  now = 10500;
  const char* names[] = {"flight", "gnss", "radar", "list", "net", "qr", "ble", "flights", "menu", "info"};
  for (int p = 0; p < 10; ++p) {
    ui.gotoPage(p);
    snprintf(name, sizeof name, "%s/page_%d_%s.ppm", dir, p, names[p]);
    dump(ui, n, now, name);
  }
  ui.gotoPage(DialUi::kRadar);
  ui.onClick();
  snprintf(name, sizeof name, "%s/radar_2nm.ppm", dir);
  dump(ui, n, now, name);
  ui.gotoPage(DialUi::kMenu);
  ui.onRotate(1); ui.onClick();
  snprintf(name, sizeof name, "%s/menu_edit.ppm", dir);
  dump(ui, n, now, name);
  // settings categories
  { DialUi mu; mu.begin(s, 0); mu.gotoPage(DialUi::kMenu); mu.onClick();
    snprintf(name, sizeof name, "%s/set_top.ppm", dir); dump(mu, n, now, name);
    for (int i = 0; i < 2; ++i) mu.onRotate(1); mu.onClick();
    snprintf(name, sizeof name, "%s/set_outputs.ppm", dir); dump(mu, n, now, name);
    mu.onRotate(1); mu.onRotate(1); mu.onRotate(1); mu.onClick(); mu.onRotate(1); mu.onRotate(1); mu.onRotate(1); mu.onRotate(1);
    mu.onRotate(-1); mu.onRotate(-1);
  }
  { DialUi mu; mu.begin(s, 0); mu.gotoPage(DialUi::kMenu); mu.onClick();
    for (int i = 0; i < 4; ++i) mu.onRotate(1); mu.onClick(); mu.onClick();
    snprintf(name, sizeof name, "%s/set_system.ppm", dir); dump(mu, n, now, name); }
  // flight log screens
  static flightlog::Flight fls[3] = {{1793966400u, 3725, 5412345, 950000, 5450000, 975000}, {1793880000u, 600, 5301234, 1054321, 5310000, 1060000}, {1793000000u, 300, 1, 2, 3, 4}};
  env.fl = fls; env.nFl = 3;
  s.flightLog = true; s.flightKmh = 60;
  DialUi fu;
  fu.begin(s, 0);
  fu.gotoPage(DialUi::kFlights);
  snprintf(name, sizeof name, "%s/fl_closed.ppm", dir); dump(fu, n, now, name);
  fu.onClick();
  snprintf(name, sizeof name, "%s/fl_detail.ppm", dir); dump(fu, n, now, name);
  fu.onClick();
  snprintf(name, sizeof name, "%s/fl_confirm.ppm", dir); dump(fu, n, now, name);
  fu.onClick(); for (int i = 0; i < 3; ++i) fu.onRotate(1);
  snprintf(name, sizeof name, "%s/fl_delall.ppm", dir); dump(fu, n, now, name);
  ui.begin(s, now - 100);
  ui.gotoPage(0);
  snprintf(name, sizeof name, "%s/splash.ppm", dir);
  dump(ui, n, now, name);
  return 0;
}
