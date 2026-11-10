/* Generates frames for the README animation using the real core */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/hp41.h"

static unsigned char fb[70][400];
static void blk(int x, int y, int w, int h) { for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) if (i >= 0 && i < 400 && j >= 0 && j < 70) fb[j][i] = 1; }
#include "../dm42/src/seg14.inc"

static uint32_t clk; static uint32_t ms(void) { return clk; }
static const hp41_platform_t plat = { 0, ms, 0, 0 };
static int nframe;
static const char *names[] = { "", "S+","INV","SQRT","LOG","LN","XEQ","STO","RCL","RDN","SIN","COS","TAN",
  "ENTER","SWAP","CHS","E","BSP","UP","7","8","9","DIV","DOWN","4","5","6","MUL",
  "SH","1","2","3","SUB","EXIT","0",".","RS","ADD","F1","F2","F3","F4","F5","F6", 0 };

static void frame(const char *cap, int hold) {
  hp41_disp_t d; hp41_get_display(&d);
  memset(fb, 0, sizeof fb);
  int x0 = (400 - HP41_CELLS * CPITCH) / 2 + 4;
  for (int i = 0; i < HP41_CELLS; i++) draw_char(x0 + i * CPITCH, 8, d.ch[i], d.punct[i]);
  char fn[64]; sprintf(fn, "frames/%03d.pgm", nframe);
  FILE *f = fopen(fn, "wb"); fprintf(f, "P5\n400 70\n255\n");
  for (int j = 0; j < 70; j++) for (int i = 0; i < 400; i++) fputc(fb[j][i] ? 0 : 255, f);
  fclose(f);
  sprintf(fn, "frames/%03d.txt", nframe);
  f = fopen(fn, "w");
  fprintf(f, "%d\n%s\n%u\n%s\n", hold, cap, d.annun, d.info ? d.info : "");
  for (int i = 0; i < 6; i++) {
    const char *m = d.has_menu && d.menu[i] ? d.menu[i] : "";
    for (const unsigned char *p = (const unsigned char *)m; *p; p++) {
      if (*p == CH_SIGMA) fputs("\xce\xa3", f); else if (*p == CH_NE) fputs("\xe2\x89\xa0", f);
      else if (*p == CH_LE) fputs("\xe2\x89\xa4", f); else if (*p == CH_APPEND) fputs("\xe2\x8a\xa2", f);
      else fputc(*p, f);
    }
    fputc('\n', f);
  }
  fclose(f);
  nframe++;
}

static int keycode(const char *t) { for (int i = 1; names[i]; i++) if (!strcmp(names[i], t)) return i; fprintf(stderr, "bad %s\n", t); exit(1); }

/* press keys; caption shown; one frame while last key held (preview), one after */
static void k(const char *seq, const char *cap, int hold) {
  char buf[256]; strcpy(buf, seq);
  char *toks[40]; int n = 0;
  for (char *t = strtok(buf, " "); t; t = strtok(0, " ")) toks[n++] = t;
  for (int i = 0; i < n; i++) {
    hp41_key(keycode(toks[i])); clk += 30; hp41_tick();
    if (i == n - 1) frame(cap, 7);      /* key held: 41 shows function name */
    hp41_key(0); clk += 30; hp41_tick();
    while (hp41_busy() & 3) { clk += 5; hp41_tick(); hp41_run_slice(1); }
  }
  frame(cap, hold);
}

/* run a program in simulated real time, grabbing frames */
static void run_frames(const char *seq, const char *cap, int maxframes) {
  char buf[256]; strcpy(buf, seq);
  for (char *t = strtok(buf, " "); t; t = strtok(0, " ")) { hp41_key(keycode(t)); hp41_key(0); }
  for (int f = 0; f < maxframes && (hp41_busy() & 3); f++) {
    for (int s = 0; s < 25; s++) { clk += 10; hp41_tick(); hp41_run_slice(1); }
    frame(cap, 5);
  }
  frame(cap, 20);
}

int main(void) {
  hp41_set_platform(&plat);
  hp41_attach_state(malloc(hp41_state_size()), 0);
  frame("HP-41CV on the DM42", 25);
  k("EXIT", "HP-41CV on the DM42", 10);
  k("2 ENTER 3", "2 ENTER 3", 12);
  k("MUL", "\xc3\x97", 15);
  k("SQRT", "\xe2\x88\x9ax", 18);
  k("SH E", "SHIFT DISP: 41 functions in 42S menus", 18);
  k("F2", "SCI", 8);
  k("3", "SCI 3", 18);
  k("F1 4 EXIT", "FIX 4", 12);
  k("3 0 SIN", "30 SIN", 18);
  k("SH ENTER", "SHIFT ENTER = ALPHA", 10);
  k("RCL LN TAN TAN CHS", "H E L L O", 18);
  k("SH ENTER", "ALPHA off", 8);
  k("SH XEQ . .", "GTO ..", 8);
  k("SH RS", "PRGM mode", 14);
  k("SH 3 F1 LN", "LBL E", 14);
  k("2 MUL", "2 \xc3\x97", 12);
  k("1 ADD", "1 +", 12);
  k("SH 3 F2", "RTN", 12);
  k("EXIT EXIT", "back to run mode", 10);
  k("4 XEQ LN", "4 XEQ E", 22);
  k("SH XEQ . .", "a counting loop", 2);
  char err[64];
  const char *p[] = { "LBL \"CNT\"", "0", "STO 00", "LBL 01", "1", "ST+ 00", "VIEW 00", "PSE", "RCL 00", "5", "X>Y?", "GTO 01", "\"DONE\"", "AVIEW", "END" };
  for (unsigned i = 0; i < sizeof p / sizeof *p; i++) hp41_import_line(p[i], err, 64);
  k("EXIT", "XEQ \"CNT\"", 6);
  run_frames("XEQ SH ENTER SQRT SWAP DIV SH ENTER", "XEQ \"CNT\": VIEW, PSE, loop", 40);
  k("SH ADD F3 RS", "CAT 3", 6);
  for (int i = 0; i < 8; i++) k("DOWN", "CAT 3", 7);
  k("EXIT", "", 30);
  return 0;
}
