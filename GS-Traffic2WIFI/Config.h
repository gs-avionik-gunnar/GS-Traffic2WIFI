#pragma once

#define GS_FW_VERSION "4.1"

// ---- GPIO assignment (unchanged from 3.x) ------------------------------------------
#define PIN_HWTYPE_A 12      // HW 2.0 boards pull both pins to GND
#define PIN_HWTYPE_B 14
#define PIN_FACTORY_RESET 19 // GND at power-up = factory reset
#define PIN_LED_GREEN 18
#define PIN_LED_RED 5
#define PIN_SERIAL_RX_V1 21
#define PIN_SERIAL_TX_V1 1
#define PIN_SERIAL_RX_V2 22
#define PIN_SERIAL_TX_V2 23

// ---- Bluetooth LE (NMEA like a CC2541/HM-10 module) ------------------------------------------------
// Always included on the ESP32-S3 (hardware 3.0). On the classic ESP32 (hardware 1/2) the Bluedroid BLE stack
// of Arduino-ESP32 makes the sketch about 1.77 MB, which does not fit the 1.31 MB app partition (measured).
// Set this to 1 only together with a larger partition scheme, e.g. "Minimal SPIFFS (1.9MB APP with OTA)".
#ifndef GS_BLE_ON_CLASSIC
#define GS_BLE_ON_CLASSIC 0
#endif

// ---- Network ------------------------------------------------------------------------
#define AP_IP_A 192
#define AP_IP_B 168
#define AP_IP_C 1
#define AP_IP_D 1
#define MAX_TCP_CLIENTS 10
#define AP_MAX_STATIONS 8
#define CLIENT_MODE_CONNECT_TIMEOUT_MS 20000

// ---- Buffers ------------------------------------------------------------------------
#define SERIAL_RX_BUFFER 2048
#define SERIAL_READ_CHUNK 128

// ---- Hardware v3 (ESP32-S3, GC9A01 240x240 round display) ---------------------
// Only compiled when the board target is an ESP32-S3. Pins as on the v3 hardware; if your unit differs
// (or colours/orientation look wrong) change them here.
#define PIN_SERIAL_RX_V3 2
#define PIN_SERIAL_TX_V3 1
#define PIN_DIAL_HOLD 46       // keeps the power on (battery operation)
#define PIN_DIAL_LCD_MOSI 5
#define PIN_DIAL_LCD_SCLK 6
#define PIN_DIAL_LCD_CS 7
#define PIN_DIAL_LCD_DC 4
#define PIN_DIAL_LCD_RST 8
#define PIN_DIAL_LCD_BL 9
#define PIN_DIAL_ENC_A 40
#define PIN_DIAL_ENC_B 41
#define PIN_DIAL_BUTTON 42
#define PIN_DIAL_BUZZER 3
#define PIN_DIAL_RING 13       // Trafficmod LEDs (WS2812, 1 status LED + 24 ring LEDs) on the Grove port
#define DIAL_LCD_MADCTL 0x48   // 0x08 = BGR; +0x40 mirrors horizontally (needed on v3), +0x80 vertically
#define DIAL_LCD_INVERT 1      // GC9A01 panels on the Dial need inversion on
#define DIAL_ENC_TRANSITIONS_PER_DETENT 4  // set to 2 if one click of the wheel jumps two steps
#define DIAL_ENC_INVERT 0      // 1 = reverse the wheel direction
#define DIAL_BEEP 1            // short click on every wheel step / press
#define DIAL_RING_OFFSET 0     // LED number (0..23) that is "straight ahead"
#define DIAL_RING_REVERSE 0    // 1 = ring LEDs are wired counter-clockwise
