// Aircraft databases in the (otherwise unused) data partition of the standard partition scheme.
// No file system: the compact tables from CallsignDb.h are written to the raw partition.
// Two independent slots (0 = FLARMnet, 1 = OGN), each with its own upload and delete.
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace csstore {

void begin();                   // find the partition, open existing databases
bool available();               // a data partition exists
uint32_t capacity(int slot);    // bytes of the slot
const char* lastError();        // reason of the last failed upload (empty if none)
uint32_t count(int slot);       // entries, 0 = empty
bool uploadBegin(int slot);     // closes the slot, starts writing
bool uploadWrite(const uint8_t* data, size_t n);
bool uploadEnd();               // validates; false = invalid data, the slot stays empty
void uploadAbort();
void erase(int slot);

}  // namespace csstore
