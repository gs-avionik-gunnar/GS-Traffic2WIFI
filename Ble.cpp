#include "Ble.h"

#include <Arduino.h>

#include "Config.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3) || GS_BLE_ON_CLASSIC

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace ble {

namespace {

const size_t kRing = 2048;
const size_t kChunk = 20;  // fits the default ATT MTU of 23 on every phone (HM-10 modules do the same)
uint8_t s_ring[kRing];
volatile size_t s_head = 0, s_tail = 0;  // head: write position, tail: read position
BLECharacteristic* s_char = nullptr;
volatile bool s_conn = false;
bool s_started = false;
uint32_t s_last = 0;

class ServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    s_conn = true;
    s_tail = s_head;  // start with fresh data
  }
  void onDisconnect(BLEServer*) override {
    s_conn = false;
    BLEDevice::startAdvertising();
  }
};

size_t used() { return (s_head + kRing - s_tail) % kRing; }

}  // namespace

bool available() { return true; }
bool connected() { return s_conn; }

void begin(const char* name) {
  if (s_started) return;
  s_started = true;
  BLEDevice::init(name);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCb());
  BLEService* svc = server->createService(BLEUUID((uint16_t)0xFFE0));
  s_char = svc->createCharacteristic(BLEUUID((uint16_t)0xFFE1),
                                     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_WRITE_NR);
  s_char->addDescriptor(new BLE2902());
  svc->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLEUUID((uint16_t)0xFFE0));
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void send(const uint8_t* d, size_t n) {
  if (!s_started || !s_conn) return;
  if (used() + n >= kRing) return;  // phone is too slow: drop instead of blocking the data path
  for (size_t i = 0; i < n; ++i) {
    s_ring[s_head] = d[i];
    s_head = (s_head + 1) % kRing;
  }
}

void poll(uint32_t now) {
  if (!s_started || !s_conn || !s_char) return;
  if ((uint32_t)(now - s_last) < 12) return;  // up to 3 x 20 bytes every 12 ms = 5 kB/s
  s_last = now;
  for (int k = 0; k < 3; ++k) {
    size_t n = used();
    if (!n) break;
    if (n > kChunk) n = kChunk;
    uint8_t buf[kChunk];
    for (size_t i = 0; i < n; ++i) buf[i] = s_ring[(s_tail + i) % kRing];
    s_tail = (s_tail + n) % kRing;
    s_char->setValue(buf, n);
    s_char->notify();
  }
}

}  // namespace ble

#else  // BLE not included in this build

namespace ble {
bool available() { return false; }
void begin(const char*) {}
void send(const uint8_t*, size_t) {}
void poll(uint32_t) {}
bool connected() { return false; }
}  // namespace ble

#endif
