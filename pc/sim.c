/* PC harness: drive the HP-41CV core with DM42 key names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/hp41.h"

static uint32_t clk;
static uint32_t ms(void) { return clk; }
static void beep(int f, int m) { (void)f; (void)m; }
static int offs;
static void poff(void) { offs++; }
static const hp41_platform_t plat = { beep, ms, poff, 0 };

static const char *names[] = { "", "S+","INV","SQRT","LOG","LN","XEQ","STO","RCL","RDN","SIN","COS","TAN",
  "ENTER","SWAP","CHS","E","BSP","UP","7","8","9","DIV","DOWN","4","5","6","MUL",
  "SH","1","2","3","SUB","EXIT","0",".","RS","ADD","F1","F2","F3","F4","F5","F6", 0 };

static int keycode(const char *t) {
  for (int i = 1; names[i]; i++) if (!strcmp(names[i], t)) return i;
  /* aliases */
  if (!strcmp(t, "+")) return K_ADD;
  if (!strcmp(t, "-")) return K_SUB;
  if (!strcmp(t, "*")) return K_MUL;
  if (!strcmp(t, "/")) return K_DIV;
  return -1;
}

static void settle(void) {
  for (int i = 0; i < 100000 && (hp41_busy() & 3); i++) { clk += 5; hp41_tick(); hp41_run_slice(50); }
}

/* press a sequence of keys separated by spaces; a token of letters in quotes types alpha chars */
void keys(const char *seq) {
  char buf[512]; strncpy(buf, seq, sizeof buf - 1); buf[sizeof buf - 1] = 0;
  for (char *t = strtok(buf, " "); t; t = strtok(0, " ")) {
    int k = keycode(t);
    if (k < 0) { fprintf(stderr, "bad key %s\n", t); exit(2); }
    hp41_key(k); clk += 20; hp41_tick();
    hp41_key(0); clk += 20; hp41_tick();
    settle();
  }
}

const char *disp(void) { static char b[64]; hp41_display_text(b, sizeof b); return b; }

static int fails, total;
void expect(const char *seq, const char *want, int line) {
  keys(seq);
  total++;
  if (strcmp(disp(), want)) { fails++; printf("FAIL line %d: [%s] -> '%s' expected '%s'\n", line, seq, disp(), want); }
}
#define T(seq, want) expect(seq, want, __LINE__)

void fresh(void) {
  static void *mem;
  if (!mem) mem = malloc(hp41_state_size());
  hp41_attach_state(mem, 0);
  keys("EXIT");
}

int import(const char *src) {
  char buf[4096]; strncpy(buf, src, sizeof buf - 1); buf[sizeof buf-1] = 0;
  char err[64];
  for (char *l = strtok(buf, "\n"); l; l = strtok(0, "\n")) {
    if (!hp41_import_line(l, err, sizeof err)) { printf("import error: %s (%s)\n", err, l); return 0; }
  }
  return 1;
}

static int pr(void *c, const char *l) { (void)c; printf("  %s\n", l); return 1; }

#include "tests.inc"

int main(int argc, char **argv) {
  hp41_set_platform(&plat);
  if (argc > 1 && !strcmp(argv[1], "-i")) {
    /* interactive: read key lines from stdin */
    fresh();
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
      line[strcspn(line, "\n")] = 0;
      if (!strcmp(line, "LIST")) { hp41_export_program(pr, 0); continue; }
      keys(line);
      printf("[%s]\n", disp());
    }
    return 0;
  }
  run_tests();
  printf("%d/%d passed\n", total - fails, total);
  return fails != 0;
}
