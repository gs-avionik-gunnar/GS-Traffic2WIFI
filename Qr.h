// Tiny QR code encoder: byte mode, error correction level L, versions 1-6 (up to 134 bytes).
// Enough for a "WIFI:T:WPA;S:<ssid>;P:<key>;;" string. Pure C++, host-testable.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace qr {

constexpr int kMaxSize = 41;  // version 6

struct Code {
  int size;                         // modules per side, 0 = failed
  uint8_t m[kMaxSize][kMaxSize];    // 1 = dark
};

// Returns false when the text does not fit into version 6.
bool encode(const char* text, Code& out);

// Builds the Wi-Fi join string understood by the camera apps of Android and iOS
// (escapes \ ; , : "). Returns false if the buffer is too small.
bool wifiString(const char* ssid, const char* key, char* out, size_t cap);

}  // namespace qr
