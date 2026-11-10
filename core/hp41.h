/*
 * HP-41CV clean-room core - public interface
 *
 * The core is platform independent. A front-end (DM42/DMCP or the PC test
 * harness) feeds key events using the DM42 key codes (1..43, 0 = release),
 * calls hp41_tick() regularly and draws what hp41_get_display() returns.
 */
#ifndef HP41_H
#define HP41_H

#include <stdint.h>

/* DM42 key codes (same values as DMCP KEY_xxx) */
enum {
  K_SIGMA = 1, K_INV, K_SQRT, K_LOG, K_LN, K_XEQ,
  K_STO, K_RCL, K_RDN, K_SIN, K_COS, K_TAN,
  K_ENTER, K_SWAP, K_CHS, K_E, K_BSP,
  K_UP, K_7, K_8, K_9, K_DIV,
  K_DOWN, K_4, K_5, K_6, K_MUL,
  K_SHIFT, K_1, K_2, K_3, K_SUB,
  K_EXIT, K_0, K_DOT, K_RUN, K_ADD,
  K_F1, K_F2, K_F3, K_F4, K_F5, K_F6,
  K_LAST = K_F6
};

#define HP41_CELLS 12

/* Special display characters (outside ASCII) */
#define CH_TEE    0x80  /* T-bar used for global labels / text lines */
#define CH_APPEND 0x81  /* |- append */
#define CH_SIGMA  0x82
#define CH_NE     0x83  /* not equal */
#define CH_LE     0x84  /* less or equal */
#define CH_ANGLE  0x85  /* angle sign, used as "goose" substitute */
#define CH_GOOSE  0x86  /* running indicator */
#define CH_UNDER  '_'

/* Annunciators */
enum {
  AN_BAT = 1<<0, AN_USER = 1<<1, AN_G = 1<<2, AN_RAD = 1<<3, AN_SHIFT = 1<<4,
  AN_0 = 1<<5, AN_1 = 1<<6, AN_2 = 1<<7, AN_3 = 1<<8, AN_4 = 1<<9,
  AN_PRGM = 1<<10, AN_ALPHA = 1<<11
};

typedef struct {
  uint8_t ch[HP41_CELLS];     /* character in each 14-segment cell */
  uint8_t punct[HP41_CELLS];  /* 0, '.', ',' or ':' attached to the cell */
  uint32_t annun;
  const char *menu[6];        /* softkey labels, NULL entries = blank; menu[0]==NULL && !has_menu -> no menu */
  int has_menu;
  int menu_more;              /* menu has more pages (show arrows) */
  const char *info;           /* optional small text line (e.g. menu title), may be NULL */
} hp41_disp_t;

/* Platform services, set by the front-end */
typedef struct {
  void (*beep)(int freq_hz, int ms);   /* may be NULL */
  uint32_t (*ms)(void);                /* monotonic milliseconds */
  void (*power_off)(void);             /* OFF executed */
  void (*keep_on)(int on);             /* ON (continuous on) flag 44 */
} hp41_platform_t;

void hp41_set_platform(const hp41_platform_t *p);

/* Memory: the whole calculator state is one block, so it can be saved as is */
uint32_t hp41_state_size(void);
void    *hp41_state_ptr(void);
int      hp41_attach_state(void *mem, int restore); /* restore=1: validate existing contents; returns 1 if restored */
void     hp41_master_clear(void);   /* MEMORY LOST */

void hp41_key(int key);          /* key code >0 pressed, 0 released */
void hp41_tick(void);            /* call often; handles PSE, catalogs, NULL timing */
int  hp41_busy(void);            /* bits: 1 running, 2 pause, 4 catalog, 8 key held */
void hp41_run_slice(int steps);  /* execute some program steps if running */
void hp41_get_display(hp41_disp_t *d);
void hp41_wakeup(void);          /* called after power on */
int  hp41_display_text(char *buf, int len); /* ASCII rendering of the 12 cells, for tests */

int  hp41_shift_active(void);
void hp41_clear_shift(void);
void hp41_set_lowbat(int low);
int  hp41_wants_off(void);       /* OFF executed since last call */
void hp41_goto_new_program(void);/* GTO .. (used before importing a listing) */

/* Program import/export as plain text listing (one instruction per line) */
int  hp41_import_line(const char *line, char *err, int errlen);
int  hp41_export_program(int (*out)(void *ctx, const char *line), void *ctx); /* exports current program */

#endif
