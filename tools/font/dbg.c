#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "../../core/hp41.h"
static unsigned char fb[50][40];
static void blk(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 40 && j >= 0 && j < 50) fb[j][i] = 1; }
#include "../../dm42/src/seg14.inc"
int main(int c, char **v) { draw_char(4, 2, v[1][0], 0); for (int j = 0; j < 42; j++) { for (int i = 0; i < 34; i++) putchar(fb[j][i] ? '#' : '.'); putchar('\n'); } }
