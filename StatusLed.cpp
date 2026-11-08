#include "StatusLed.h"

#include <Arduino.h>

#include "App.h"
#include "Config.h"

namespace statusled {

void begin() {
  if (g_hw3) return;  // GPIO 5/18 are display pins on v3
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  set(0);
}

void set(uint8_t state) {
  if (g_hw3) return;
  digitalWrite(PIN_LED_GREEN, (state == 1 || state == 3) ? HIGH : LOW);
  digitalWrite(PIN_LED_RED, (state == 2 || state == 3) ? HIGH : LOW);
}

void bootSequence() {
  if (g_hw3) return;
  set(1);
  delay(300);
  set(0);
  delay(300);
  set(2);
  delay(300);
  set(0);
  delay(300);
  set(3);
  delay(300);
  set(0);
}

}  // namespace statusled
