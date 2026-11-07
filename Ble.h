// NMEA over Bluetooth LE, emulating a CC2541/HM-10 serial module (service 0xFFE0, characteristic 0xFFE1,
// notify), which is what SkyDemon and other EFBs expect from "Bluetooth LE" avionics adapters.
// Compiled in on ESP32-S3 and, switchable in Config.h (GS_BLE_ON_CLASSIC), on the classic ESP32.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ble {
bool available();                  // true when this build contains the BLE stack
void begin(const char* name);      // starts advertising (call once, after WiFi is up)
void send(const uint8_t* d, size_t n);  // queue NMEA bytes for the connected phone
void poll(uint32_t now);           // call from loop(): sends queued data in 20 byte notifications
bool connected();
int connections();                 // number of connected phones
}  // namespace ble
