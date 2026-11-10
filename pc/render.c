/* render the display to a PGM image for checking the 14-segment font */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/hp41.h"
static unsigned char fb[330][400];
static void blk(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 400 && j >= 0 && j < 330) fb[j][i] = 1; }
#include "../dm42/src/seg14.inc"
int main(int argc, char **argv) {
  const char *lines[] = { "0123456789AB", "CDEFGHIJKLMN", "OPQRSTUVWXYZ", "-1,234.5678", "+-*/=<>?_%$ ", "abcde HELLO" };
  int nl = 6;
  memset(fb, 0, sizeof fb);
  for (int l = 0; l < nl; l++) {
    const char *s = lines[l]; int cell = 0;
    for (int i = 0; s[i] && cell < 12; i++) {
      uint8_t p = 0;
      if (s[i+1] == '.' || s[i+1] == ',') p = s[i+1];
      draw_char(8 + cell * CPITCH, 4 + l * 54, s[i], p);
      if (p) i++;
      cell++;
    }
  }
  /* scale y: use smaller rows by drawing chars at 46 height, rows overlap -> use 2 images */
  FILE *f = fopen(argv[1], "wb");
  fprintf(f, "P5\n400 330\n255\n");
  for (int j = 0; j < 330; j++) for (int i = 0; i < 400; i++) fputc(fb[j][i] ? 0 : 230, f);
  fclose(f);
  return 0;
}
