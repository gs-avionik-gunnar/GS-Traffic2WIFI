#include "CallsignStore.h"

#include <esp_partition.h>
#include <stdlib.h>
#include <string.h>

#include "CallsignDb.h"

namespace csstore {

namespace {
const esp_partition_t* s_part = nullptr;
uint8_t* s_buf = nullptr;  // one flash sector, filled before it is written
size_t s_fill = 0;
uint32_t s_off = 0, s_total = 0;  // offsets inside the slot
int s_slot = 0;
bool s_ok = false;
const char* s_err = "";
const size_t kSector = 4096;

// slot 0 gets a third of the partition (FLARMnet is small), slot 1 the rest; both sector aligned
uint32_t slotSize(int slot) {
  if (!s_part) return 0;
  uint32_t a = (s_part->size / 3) & ~(uint32_t)(kSector - 1);
  return slot == 0 ? a : ((s_part->size - a) & ~(uint32_t)(kSector - 1));
}
uint32_t slotBase(int slot) { return slot == 0 ? 0 : slotSize(0); }

bool readFn(uint32_t off, void* buf, size_t n) { return s_part && esp_partition_read(s_part, off, buf, n) == ESP_OK; }

bool flushSector() {
  if (s_fill == 0) return true;
  if (s_off + kSector > slotSize(s_slot)) {
    s_err = "too large for this slot";
    return false;
  }
  memset(s_buf + s_fill, 0xFF, kSector - s_fill);
  uint32_t abs = slotBase(s_slot) + s_off;
  if (esp_partition_erase_range(s_part, abs, kSector) != ESP_OK || esp_partition_write(s_part, abs, s_buf, kSector) != ESP_OK) {
    s_err = "flash write failed";
    return false;
  }
  s_off += kSector;
  s_fill = 0;
  return true;
}
}  // namespace

void begin() {
  s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (!s_part) return;
  csdb::setReader(readFn);
  for (int i = 0; i < csdb::kSlots; ++i) csdb::openSlot(i, slotBase(i), slotSize(i));
}

bool available() { return s_part != nullptr; }
uint32_t capacity(int slot) { return slotSize(slot); }
uint32_t count(int slot) { return csdb::count(slot); }

const char* lastError() { return s_err; }

bool uploadBegin(int slot) {
  s_err = "";
  if (!s_part) {
    s_err = "no data partition (use partition scheme: Default 4MB with spiffs)";
    return false;
  }
  if (slot < 0 || slot >= csdb::kSlots) return false;
  csdb::closeSlot(slot);
  free(s_buf);
  s_buf = (uint8_t*)malloc(kSector);
  s_slot = slot;
  s_fill = 0;
  s_off = 0;
  s_total = 0;
  s_ok = s_buf != nullptr;
  if (s_ok) s_ok = esp_partition_erase_range(s_part, slotBase(slot), kSector) == ESP_OK;  // the old header is gone from now on
  return s_ok;
}

bool uploadWrite(const uint8_t* data, size_t n) {
  if (!s_ok) return false;
  s_total += (uint32_t)n;
  while (n > 0) {
    size_t take = kSector - s_fill;
    if (take > n) take = n;
    memcpy(s_buf + s_fill, data, take);
    s_fill += take;
    data += take;
    n -= take;
    if (s_fill == kSector && !flushSector()) {
      s_ok = false;
      return false;
    }
  }
  return true;
}

bool uploadEnd() {
  bool ok = s_ok && flushSector();
  free(s_buf);
  s_buf = nullptr;
  s_ok = false;
  if (ok) ok = csdb::openSlot(s_slot, slotBase(s_slot), slotSize(s_slot)) && (uint32_t)csdb::kHeader + csdb::count(s_slot) * csdb::kRec == s_total;
  if (!ok) {
    if (!*s_err) s_err = "invalid database file";
    erase(s_slot);
  }
  return ok;
}

void uploadAbort() {
  free(s_buf);
  s_buf = nullptr;
  s_ok = false;
  erase(s_slot);
}

void erase(int slot) {
  if (slot < 0 || slot >= csdb::kSlots) return;
  csdb::closeSlot(slot);
  if (s_part) esp_partition_erase_range(s_part, slotBase(slot), kSector);
}

}  // namespace csstore
