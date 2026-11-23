/* load examples/SELFTEST.txt into the core, run XEQ "TEST", report prompts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/hp41.h"
static uint32_t clk; static uint32_t ms(void) { return clk; }
static void gt(hp41_time_t *t) { memset(t, 0, sizeof *t); t->year = 2026; t->month = 11; t->day = 23; t->hour = 9; }
static const hp41_platform_t plat = { 0, ms, 0, 0, gt, 0 };
static void key(int k) { hp41_key(k); clk += 20; hp41_tick(); hp41_key(0); clk += 20; hp41_tick();
  for (int i = 0; i < 2000000 && (hp41_busy() & 3); i++) { clk += 5; hp41_tick(); hp41_run_slice(50); } }
static int pr(void *c, const char *l) { (void)c; (void)l; return 1; }
int main(int argc, char **argv) {
  hp41_set_platform(&plat);
  hp41_attach_state(malloc(hp41_state_size()), 0);
  key(K_EXIT);
  key(K_SHIFT); key(K_8); key(K_F1); key(K_0); key(K_3); key(K_6); key(K_EXIT);   /* SIZE 036 */
  FILE *f = fopen(argc > 1 ? argv[1] : "../examples/SELFTEST.txt", "r");
  char line[200], err[80]; int n = 0, bad = 0;
  while (fgets(line, sizeof line, f)) { n++; if (!hp41_import_line(line, err, sizeof err)) { if (!bad) printf("line %d: %s\n", n, err); bad++; } }
  fclose(f);
  if (bad) return 1;
  /* free registers after loading */
  char b[64];
  key(K_SHIFT); key(K_RUN); key(K_SHIFT); key(K_XEQ); key(K_DOT); key(K_0); key(K_0); key(K_0);
  hp41_display_text(b, 64); printf("free memory at SIZE 036: %s\n", b); key(K_SHIFT); key(K_RUN);
  /* XEQ "TEST" */
  key(K_XEQ); key(K_SHIFT); key(K_ENTER); key(K_4); key(K_LN); key(K_DIV); key(K_4);
  key(K_SHIFT); key(argc > 2 && argv[2][0] == '2' ? K_2 : K_1); key(K_SHIFT); key(K_ENTER);
  for (int i = 0; i < 200; i++) {
    hp41_display_text(b, 64);
    printf("-> %s\n", b);
    if (strstr(b, "ERR") || strstr(b, "SIZE")) { key(K_RUN); continue; }
    break;
  }
  return 0;
}
