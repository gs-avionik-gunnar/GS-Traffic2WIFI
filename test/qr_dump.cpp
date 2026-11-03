#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Qr.h"
// usage: qrtest "text" out.pgm
int main(int argc, char** argv) {
  qr::Code c;
  if (!qr::encode(argv[1], c)) { printf("fail\n"); return 1; }
  FILE* f = fopen(argv[2], "wb");
  int q = 4, s = 8, N = (c.size + 2 * q) * s;
  fprintf(f, "P5\n%d %d\n255\n", N, N);
  for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
    int mx = x / s - q, my = y / s - q;
    unsigned char v = (mx >= 0 && my >= 0 && mx < c.size && my < c.size && c.m[my][mx]) ? 0 : 255;
    fputc(v, f);
  }
  fclose(f);
  printf("%d\n", c.size);
}
