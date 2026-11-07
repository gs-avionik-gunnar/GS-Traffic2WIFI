// Aircraft registration database (FLARMnet TDB converted in the browser to a compact table), plus the
// optional "ID!registration" rewrite of PFLAA sentences. Pure C++: the flash access is behind ReadFn.
//
// Table layout (little endian): "GSDB", u32 count, u32 source version, u32 0,
// then count records of 12 bytes: u32 id (ascending), 8 bytes registration (zero padded).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace csdb {

enum { kHeader = 16, kRec = 12, kRegLen = 8 };

// Two independent tables (slot 0 = FLARMnet, slot 1 = OGN) in one storage area; slot 0 wins for an ID in both.
enum { kSlots = 2 };
typedef bool (*ReadFn)(uint32_t off, void* buf, size_t n);  // absolute offset in the storage area
void setReader(ReadFn fn);
bool openSlot(int slot, uint32_t base, uint32_t size);      // validates the header; false = no usable database
void closeSlot(int slot);
uint32_t count(int slot = -1);                              // entries of one slot, -1 = all slots
// out: registration (letters, digits, '-'), zero terminated. False if unknown or no database.
bool lookup(uint32_t id, char out[kRegLen + 1]);

// "$PFLAA,...,ID,..." -> "$PFLAA,...,ID!REG,..." with a recomputed checksum. False if nothing was changed.
bool rewritePflaa(const char* line, char* out, size_t cap);

typedef void (*Emit)(const uint8_t* data, size_t n, void* ctx);
// Passes the NMEA stream through line by line and adds the registration to PFLAA sentences.
class LineRewriter {
 public:
  void feed(const uint8_t* data, size_t n, Emit emit, void* ctx);

 private:
  char buf_[200];
  size_t n_ = 0;
  bool overflow_ = false;
};

}  // namespace csdb
