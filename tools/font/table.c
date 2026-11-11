/* render the HP-41 character table (codes 00-4F) with the emulator font */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../../core/hp41.h"
static unsigned char fb[5 * 50][16 * 34];
static void blk(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 16 * 34 && j >= 0 && j < 250) fb[j][i] = 1; }
#include "../../dm42/src/seg14.inc"
int main(void) {
  int map[80];
  for (int i = 0; i < 32; i++) map[i] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"[i];
  for (int i = 32; i < 64; i++) map[i] = i;
  int r4[16] = { CH_APPEND, 'a', 'b', 'c', 'd', 'e', 0, CH_TEE, 0, 0, 0, 0, 0, 0, CH_SIGMA, CH_ANGLE };
  for (int i = 0; i < 16; i++) map[64 + i] = r4[i];
  for (int c = 0; c < 80; c++) if (map[c]) draw_char(8 + (c % 16) * 34, 6 + (c / 16) * 50, (uint8_t)map[c], 0);
  FILE *f = fopen("table.pgm", "wb"); fprintf(f, "P5\n%d %d\n255\n", 16 * 34, 250);
  for (int j = 0; j < 250; j++) for (int i = 0; i < 16 * 34; i++) fputc(fb[j][i] ? 0 : 255, f);
  fclose(f); return 0;
}
