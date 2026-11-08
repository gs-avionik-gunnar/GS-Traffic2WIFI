// Host tool: looks up IDs (hex, one per line on stdin) in a database blob and prints "ID REG" (empty REG if unknown).
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "CallsignDb.h"

static std::vector<unsigned char> g;
static bool rd(uint32_t off, void* b, size_t n) {
  if (off + n > g.size()) return false;
  for (size_t i = 0; i < n; ++i) ((unsigned char*)b)[i] = g[off + i];
  return true;
}
int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "rb");
  if (!f) return 2;
  int c;
  while ((c = fgetc(f)) != EOF) g.push_back((unsigned char)c);
  fclose(f);
  csdb::setReader(rd);
  if (!csdb::openSlot(0, 0, (uint32_t)g.size())) return 3;
  char line[64], r[16];
  while (fgets(line, sizeof line, stdin)) {
    unsigned id = (unsigned)strtoul(line, nullptr, 16);
    if (!csdb::lookup(id, r)) r[0] = 0;
    printf("%06X %s\n", id, r);
  }
  return 0;
}
