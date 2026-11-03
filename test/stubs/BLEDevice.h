#pragma once
// Stub of the Arduino-ESP32 BLE API surface used by Ble.cpp (syntax check only).
#include "Arduino.h"
class BLEUUID { public: BLEUUID(uint16_t) {} };
class BLEServer;
class BLEServerCallbacks { public: virtual ~BLEServerCallbacks() {} virtual void onConnect(BLEServer*) {} virtual void onDisconnect(BLEServer*) {} };
class BLE2902 { public: BLE2902() {} };
class BLEDescriptor;
class BLECharacteristic {
 public:
  static const uint32_t PROPERTY_READ = 1, PROPERTY_NOTIFY = 2, PROPERTY_WRITE_NR = 4;
  void addDescriptor(BLE2902*) {}
  void setValue(uint8_t*, size_t) {}
  void notify() {}
};
class BLEService { public: BLECharacteristic* createCharacteristic(BLEUUID, uint32_t) { return nullptr; } void start() {} };
class BLEServer { public: void setCallbacks(BLEServerCallbacks*) {} BLEService* createService(BLEUUID) { return nullptr; } };
class BLEAdvertising { public: void addServiceUUID(BLEUUID) {} void setScanResponse(bool) {} };
class BLEDevice {
 public:
  static void init(const String&) {}
  static BLEServer* createServer() { return nullptr; }
  static BLEAdvertising* getAdvertising() { return nullptr; }
  static void startAdvertising() {}
};
