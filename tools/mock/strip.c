/* render a 12-cell LCD text line into a PGM (for display mockups) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../../core/hp41.h"
#define CW 21
#define CH 36
#define CPITCH 29
static unsigned char fb[50][400];
static void blk(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 400 && j >= 0 && j < 50) fb[j][i] = 1; }
#include "../../dm42/src/seg14.inc"
int main(int argc, char **argv) {
  const char *s = argv[1]; int cell = 0;
  for (int i = 0; s[i] && cell < 12; i++) {
    uint8_t p = 0;
    if (s[i+1] == '.' || s[i+1] == ',' || s[i+1] == ':') p = s[i+1];
    draw_char(2 + cell * CPITCH, 2, s[i], p);
    if (p) i++;
    cell++;
  }
  FILE *f = fopen(argv[2], "wb"); fprintf(f, "P5\n%d 50\n255\n", 12 * CPITCH + 4);
  for (int j = 0; j < 50; j++) for (int i = 0; i < 12 * CPITCH + 4; i++) fputc(fb[j][i] ? 0 : 255, f);
  fclose(f); return 0;
}
