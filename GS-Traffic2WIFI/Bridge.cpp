#include "Bridge.h"

#include "App.h"
#include "Ble.h"
#include "CallsignDb.h"
#include "FlightStore.h"
#include "StatusLed.h"

static void onStaIpAssigned(arduino_event_id_t, arduino_event_info_t);

void Bridge::begin() {
  const Settings& s = g_settings;

  serial_->setRxBufferSize(SERIAL_RX_BUFFER);  // must be called before begin()
  if (g_hw3) serial_->begin(s.baud, SERIAL_8N1, PIN_SERIAL_RX_V3, PIN_SERIAL_TX_V3);
  else if (g_hw2) serial_->begin(s.baud, SERIAL_8N1, PIN_SERIAL_RX_V2, PIN_SERIAL_TX_V2);
  else serial_->begin(s.baud, SERIAL_8N1, PIN_SERIAL_RX_V1, PIN_SERIAL_TX_V1);

  simulate_ = s.simWifi || s.simSerial;
  g_stats.simActive = simulate_ ? 1 : 0;

  const bool apActive = !s.clientMode || g_apFallback;
  if (apActive) {
    bcast_ = IPAddress(AP_IP_A, AP_IP_B, AP_IP_C, 255);
    server_.begin(s.tcpPort);
    server_.setNoDelay(true);
  } else {
    bcast_ = WiFi.broadcastIP();
    upstreamIp_.fromString(s.clientHost);
  }

  if (!s.clientMode || g_apFallback) WiFi.onEvent(onStaIpAssigned, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
  // Send from the GDL90 port itself (the 4.0 alpha did this via udp.begin; some apps may filter on the source port).
  if (s.gdl90) udp_.begin(s.gdl90Port);
  g_nav.requireChecksum = s.nmeaStrict;
  if (s.ble) ble::begin(s.bleName);
  gdl90::PublisherConfig pc;
  memset(&pc, 0, sizeof pc);
  pc.traffic = s.gdl90Traffic;
  pc.stratuxHb = s.stratuxHb;
  pc.foreFlightId = s.ffId;
  pc.utcInvalid = simulate_;  // the simulator clock is not real time: do not claim a valid UTC time
  pc.ownAddr = s.ownAddr;
  pc.ownAddrType = s.ownAddrType;
  strncpy(pc.callsign, s.callsign, 8);
  pc.emitter = s.emitter;
  pc.lookup = csdb::lookup;
  publisher_.configure(pc);
}

// ------------------------------------------------------------------ parsing
void Bridge::handleLine(const char* line, uint32_t now) {
  LOG(3, "NMEA: %s\n", line);
  if (xSemaphoreTake(g_navMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;  // drop one sentence rather than stall
  nmea::Type t = g_nav.ingestLine(line, now);
  xSemaphoreGive(g_navMutex);
  if (t == nmea::Type::Unknown) LOG(3, "  (ignored)\n");
}

void Bridge::ingest(const uint8_t* data, size_t n, uint32_t now) {
  for (size_t i = 0; i < n; ++i)
    if (assembler_.push(data[i])) handleLine(assembler_.line(), now);
}

// -------------------------------------------------------------- TCP clients
void Bridge::acceptClients() {
  if (!server_.hasClient()) return;
  WiFiClient c = server_.available();
  if (!c) return;
  for (int i = 0; i < MAX_TCP_CLIENTS; ++i) {
    if (!clients_[i] || !clients_[i].connected()) {
      clients_[i].stop();
      clients_[i] = c;
      clients_[i].setNoDelay(true);
      failCount_[i] = 0;
      LOG(1, "TCP client %d connected\n", i);
      return;
    }
  }
  c.stop();  // all slots busy
  LOG(1, "TCP client rejected (full)\n");
}

void Bridge::forwardRaw(const uint8_t* data, size_t n) {
  uint32_t active = 0;
  for (int i = 0; i < MAX_TCP_CLIENTS; ++i) {
    if (!clients_[i]) continue;
    if (!clients_[i].connected()) {
      clients_[i].stop();
      continue;
    }
    size_t w = clients_[i].write(data, n);
    if (w != n) {
      // Slow or stalled client: after three short writes in a row drop it. NMEA clients reconnect on their own.
      if (++failCount_[i] >= 3) {
        LOG(1, "TCP client %d dropped (slow)\n", i);
        clients_[i].stop();
        continue;
      }
    } else {
      failCount_[i] = 0;
    }
    ++active;
  }
  g_stats.tcpClients = active;
}

// ------------------------------------------------------------------- inputs
void Bridge::pollSerial(uint32_t now) {
  uint8_t chunk[SERIAL_READ_CHUNK];
  int budget = 1024;  // bytes per poll, keeps the loop responsive under a flood
  while (budget > 0) {
    int avail = serial_->available();
    if (avail <= 0) break;
    size_t n = (size_t)avail < sizeof chunk ? (size_t)avail : sizeof chunk;
    n = serial_->readBytes(chunk, n);
    if (n == 0) break;
    budget -= (int)n;
    g_stats.rxBytes += n;
    forwardNmea(chunk, n);  // forward first: lowest latency
    ingest(chunk, n, now);
  }
}

void Bridge::emitNmea(const uint8_t* d, size_t n, void* ctx) {
  Bridge* b = static_cast<Bridge*>(ctx);
  if (g_settings.nmeaTcp) b->forwardRaw(d, n);
  if (g_settings.ble) ble::send(d, n);
}

void Bridge::forwardNmea(const uint8_t* data, size_t n) {
  if (g_settings.nmeaReg && csdb::count() > 0) rewriter_.feed(data, n, emitNmea, this);  // PFLAA ID -> ID!registration
  else emitNmea(data, n, this);
}

void Bridge::onSimLine(const char* line) {
  char buf[nmea::kMaxLine + 4];
  size_t n = snprintf(buf, sizeof buf, "%s\r\n", line);
  if (g_settings.simWifi) forwardNmea((const uint8_t*)buf, n);
  if (g_settings.simSerial) serial_->write((const uint8_t*)buf, n);
  ingest((const uint8_t*)buf, n, lastNow_);
}

void Bridge::pollSimulator(uint32_t now) {
  lastNow_ = now;
  while (serial_->available()) serial_->read();  // simulation replaces bridging: discard real input
  simulator_.tick(now, simThunk, this);
}

void Bridge::pollClientMode(uint32_t now) {
  if (WiFi.status() != WL_CONNECTED) {
    if (upstream_.connected()) upstream_.stop();
    return;
  }
  if (!upstream_.connected()) {
    upstream_.stop();
    if ((int32_t)(now - nextConnectTryMs_) < 0) return;
    nextConnectTryMs_ = now + 3000;
    if (upstream_.connect(upstreamIp_, g_settings.tcpPort, 800)) {
      upstream_.setNoDelay(true);
      LOG(1, "Connected to upstream %s\n", g_settings.clientHost);
    }
    return;
  }
  uint8_t chunk[SERIAL_READ_CHUNK];
  int budget = 1024;
  while (budget > 0) {
    int avail = upstream_.available();
    if (avail <= 0) break;
    size_t n = (size_t)avail < sizeof chunk ? (size_t)avail : sizeof chunk;
    int r = upstream_.read(chunk, n);
    if (r <= 0) break;
    budget -= r;
    g_stats.rxBytes += (uint32_t)r;
    serial_->write(chunk, (size_t)r);  // client mode bridges TCP -> serial
    if (g_settings.ble) ble::send(chunk, (size_t)r);
    ingest(chunk, (size_t)r, now);
  }
}

// ------------------------------------------------------------------- GDL90
// IPs handed out by the DHCP server of the access point (filled from the WiFi event task).
static const int kMaxStaIp = 8;
static volatile uint32_t s_staIp[kMaxStaIp];

static void onStaIpAssigned(arduino_event_id_t, arduino_event_info_t info) {
  uint32_t ip = (uint32_t)info.wifi_ap_staipassigned.ip.addr;
  if (!ip) return;
  int freeSlot = -1;
  for (int i = 0; i < kMaxStaIp; ++i) {
    if (s_staIp[i] == ip) return;
    if (!s_staIp[i] && freeSlot < 0) freeSlot = i;
  }
  if (freeSlot < 0) {  // full: replace the oldest by shifting
    for (int i = 1; i < kMaxStaIp; ++i) s_staIp[i - 1] = s_staIp[i];
    freeSlot = kMaxStaIp - 1;
  }
  s_staIp[freeSlot] = ip;
}

void Bridge::sendTo(IPAddress ip, const uint8_t* frame, size_t len) {
  if (udp_.beginPacket(ip, g_settings.gdl90Port)) {
    udp_.write(frame, len);
    if (!udp_.endPacket()) ++g_stats.udpErrors;
  } else {
    ++g_stats.udpErrors;
  }
}

void Bridge::sendGdl90(const uint8_t* frame, size_t len) {
  if (!canSend_) return;
  const bool apActive = !g_settings.clientMode || g_apFallback;
  const uint8_t cast = g_settings.g90Cast;
  if (apActive && cast >= 1) {
    if (WiFi.softAPgetStationNum() == 0) {
      for (int i = 0; i < kMaxStaIp; ++i) s_staIp[i] = 0;  // everybody left
    }
    for (int i = 0; i < kMaxStaIp; ++i) {
      uint32_t ip = s_staIp[i];
      if (ip) sendTo(IPAddress(ip), frame, len);
    }
  }
  if (cast != 1 || !apActive) sendTo(bcast_, frame, len);
  ++g_stats.gdl90Frames;
}

void Bridge::pollGdl90(uint32_t now) {
  if (!g_settings.gdl90) return;
  // In AP mode only transmit while a station is associated: saves airtime and lwIP buffers.
  if ((int32_t)(now - bcastCheckMs_) >= 0) {
    bcastCheckMs_ = now + 1000;
    const bool apActive = !g_settings.clientMode || g_apFallback;
    canSend_ = apActive ? (WiFi.softAPgetStationNum() > 0) : (WiFi.status() == WL_CONNECTED);
  }
  if (xSemaphoreTake(g_navMutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
  publisher_.tick(now, g_nav, gdl90Thunk, this);
  xSemaphoreGive(g_navMutex);
}

// --------------------------------------------------------------------- LED
void Bridge::updateLed(uint32_t now) {
  uint8_t led = 0;
  if (xSemaphoreTake(g_navMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    led = computeLedState(g_nav, now, g_settings.trafficLed, g_settings.positionLed,
                          (uint32_t)g_settings.trafficHoldS * 1000u, (uint32_t)g_settings.positionHoldS * 1000u);
    xSemaphoreGive(g_navMutex);
  } else {
    led = g_stats.led;
  }
  g_stats.led = led;
  statusled::set(led);
}

void Bridge::poll() {
  uint32_t now = millis();
  const bool apActive = !g_settings.clientMode || g_apFallback;
  if (apActive) acceptClients();

  if (simulate_) pollSimulator(now);
  else if (apActive) pollSerial(now);
  else pollClientMode(now);

  flights::tick(g_nav, now, !simulate_);
  pollGdl90(now);
  if (g_settings.ble) ble::poll(now);
  updateLed(now);
}
