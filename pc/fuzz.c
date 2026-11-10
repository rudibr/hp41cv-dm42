#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/hp41.h"
static uint32_t clk; static uint32_t ms(void){return clk;}
static void gt(hp41_time_t *t){uint32_t s=clk/1000; t->year=2026;t->month=11;t->day=10+s/86400%10;s%=86400;t->hour=s/3600;t->min=s/60%60;t->sec=s%60;t->csec=0;}
static void st_(const hp41_time_t *t){(void)t;}
static const hp41_platform_t plat = {0, ms, 0, 0, gt, st_};
int main(int argc,char**argv){
  int seed = argc>1?atoi(argv[1]):1; srand(seed);
  hp41_set_platform(&plat);
  void *m = malloc(hp41_state_size()); hp41_attach_state(m,0);
  char err[64];
  hp41_import_line("LBL \"T\"", err, 64); hp41_import_line("1", err, 64); hp41_import_line("+", err, 64);
  hp41_import_line("GTO \"T\"", err, 64);
  for (long i=0;i<400000;i++){
    int k = 1 + rand()%43;
    if (rand()%50==0) k = K_EXIT;
    hp41_key(k); clk+=10; hp41_tick(); hp41_run_slice(20);
    if (rand()%3) { hp41_key(0); clk+=10; }
    if (hp41_busy()&1 && rand()%5==0) { hp41_key(K_RUN); hp41_key(0);}  /* stop runaway */
    hp41_tick();
    char b[64]; hp41_display_text(b,64);
    hp41_disp_t d; hp41_get_display(&d);
    if (i % 100000 == 0) { /* save/restore round trip */
      void *c = malloc(hp41_state_size()); memcpy(c, m, hp41_state_size());
      if (!hp41_attach_state(c, 1)) { printf("restore failed\n"); return 1; }
      free(m); m = c;
    }
  }
  printf("seed %d ok: %s\n", seed, "done");
  return 0;
}
