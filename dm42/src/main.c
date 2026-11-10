/*
 * HP-41CV clean-room emulator for the SwissMicros DM42 (DMCP 3.14+)
 * Front-end: 14-segment display, annunciators, keyboard loop, persistence.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <main.h>
#include <dmcp.h>
#include "hp41.h"

#define STATE_DIR   "/HP41CV"
#define STATE_FILE  "/HP41CV/STATE.DAT"
#define PGM_DIR     "/HP41CV"

/* ================= platform services ================= */
static void p_beep(int f, int ms) {
  if (ST(STAT_BEEP_MUTE)) return;
  start_buzzer_freq((uint32_t)f * 1000);
  sys_delay(ms);
  stop_buzzer();
}
static uint32_t p_ms(void) { return sys_current_ms(); }
static int keep_on;
static void p_keep_on(int on) { keep_on = on; }
static void p_get_time(hp41_time_t *t) {
  tm_t tm; dt_t dt;
  rtc_read(&tm, &dt);
  t->year = dt.year; t->month = dt.month; t->day = dt.day;
  t->hour = tm.hour; t->min = tm.min; t->sec = tm.sec; t->csec = tm.csec;
}
static void p_set_time(const hp41_time_t *t) {
  tm_t tm; dt_t dt;
  rtc_read(&tm, &dt);
  dt.year = (uint16_t)t->year; dt.month = (uint8_t)t->month; dt.day = (uint8_t)t->day;
  tm.hour = (uint8_t)t->hour; tm.min = (uint8_t)t->min; tm.sec = (uint8_t)t->sec; tm.csec = 0;
  tm.dow = (uint8_t)(julian_day(&dt) % 7 + 1);
  rtc_write(&tm, &dt);
}
static const hp41_platform_t plat = { p_beep, p_ms, 0, p_keep_on, p_get_time, p_set_time };

static void blk(int x, int y, int w, int h) { if (w > 0 && h > 0) lcd_fill_rect(x, y, w, h, 1); }
#include "seg14.inc"

static const char *ascii_label(const char *s, char *buf, int len) {
  if (!s) return 0;
  int n = 0;
  for (; *s && n < len - 3; s++) {
    uint8_t c = (uint8_t)*s;
    switch (c) {
      case CH_SIGMA:  buf[n++] = 'S'; break;
      case CH_NE:     buf[n++] = '#'; break;
      case CH_LE:     buf[n++] = '<'; buf[n++] = '='; break;
      case CH_APPEND: buf[n++] = '|'; buf[n++] = '-'; break;
      case CH_TEE:    buf[n++] = 'T'; break;
      case CH_ANGLE:  buf[n++] = '<'; break;
      default: buf[n++] = (char)c;
    }
  }
  buf[n] = 0;
  return buf;
}

static int show_stack = 0;   /* optional Y/Z/T helper line (off = authentic) */

static void redraw(void) {
  hp41_disp_t d;
  hp41_get_display(&d);
  lcd_clear_buf();

  /* header: time and date on the left, menu title in the middle, battery on the right */
  {
    const int hh = lcd_lineHeight(t20);
    lcd_fill_rect(0, 0, LCD_X, hh, 1);
    tm_t tm; dt_t dt;
    rtc_read(&tm, &dt);
    char b[40];
    int h = tm.hour, pm = 0;
    if (!ST(STAT_CLK24)) { pm = h >= 12; h %= 12; if (!h) h = 12; }
    int n = snprintf(b, sizeof b, ST(STAT_CLK24) ? "%02d:%02d  " : "%d:%02d%s  ", h, tm.min, pm ? "pm" : "am");
    if (ST(STAT_YMD)) snprintf(b + n, sizeof b - n, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
    else if (ST(STAT_DMY)) snprintf(b + n, sizeof b - n, "%d.%d.%04d", dt.day, dt.month, dt.year);
    else snprintf(b + n, sizeof b - n, "%d/%d/%04d", dt.month, dt.day, dt.year);
    t20->newln = 0; t20->lnfill = 0;
    lcd_setXY(t20, 4, 0);
    lcd_putsR(t20, b);
    if (d.info) {
      char ib[24];
      ascii_label(d.info, ib, sizeof ib);
      lcd_setXY(t20, 236 - lcd_textWidth(t20, ib) / 2, 0);
      lcd_putsR(t20, ib);
    }
    /* battery */
    int bx = LCD_X - 30, by = (hh - 12) / 2;
    if (usb_powered()) {
      lcd_setXY(t20, bx - 12, 0);
      lcd_putsR(t20, "USB");
    } else {
      int mv = get_vbat();
      snprintf(b, sizeof b, "%d.%dV", mv / 1000, (mv % 1000) / 100);
      lcd_setXY(t20, bx - 6 - lcd_textWidth(t20, b), 0);
      lcd_putsR(t20, b);
      int lvl = (mv - 2500) * 20 / 600;          /* 2.5 V empty .. 3.1 V full */
      if (lvl < 0) lvl = 0;
      if (lvl > 20) lvl = 20;
      lcd_fill_rect(bx, by, 24, 12, 0);           /* outline */
      lcd_fill_rect(bx + 24, by + 3, 3, 6, 0);    /* tip */
      lcd_fill_rect(bx + 2, by + 2, 20, 8, 1);
      if (!get_lowbat_state() || (sys_current_ms() / 500) % 2) lcd_fill_rect(bx + 2, by + 2, lvl, 8, 0);
    }
    t20->lnfill = 1; t20->newln = 1;
  }

  /* LCD window frame */
  int y0 = 52;
  int x0 = (LCD_X - HP41_CELLS * CPITCH) / 2 + 4;
  for (int i = 0; i < HP41_CELLS; i++) draw_char(x0 + i * CPITCH, y0, d.ch[i], d.punct[i]);

  /* annunciators */
  static const struct { uint32_t bit; const char *txt; int x; } an[] = {
    { AN_BAT, "BAT", 6 }, { AN_USER, "USER", 48 }, { AN_G, "G", 112 }, { AN_RAD, "RAD", 124 },
    { AN_SHIFT, "SHIFT", 172 }, { AN_0, "0", 236 }, { AN_1, "1", 248 }, { AN_2, "2", 260 },
    { AN_3, "3", 272 }, { AN_4, "4", 284 }, { AN_PRGM, "PRGM", 302 }, { AN_ALPHA, "ALPHA", 352 },
  };
  int ya = y0 + CH + 14;
  for (unsigned i = 0; i < sizeof an / sizeof an[0]; i++) {
    if (!(d.annun & an[i].bit)) continue;
    lcd_setXY(t20, an[i].x, ya);
    t20->newln = 0;
    lcd_puts(t20, an[i].txt);
  }
  t20->newln = 1;

  if (d.has_menu) {
    static char lb[6][16];
    const char *keys[6];
    for (int i = 0; i < 6; i++) keys[i] = d.menu[i] ? ascii_label(d.menu[i], lb[i], sizeof lb[i]) : "";
    lcd_draw_menu_keys(keys);
  }
  (void)show_stack;
  lcd_refresh();
}

/* ================= persistence ================= */
static void *state;

static int save_state(void) {
  if (!state) return 0;
  FIL *f = malloc(sizeof(FIL));
  if (!f) return 0;
  int ok = 0;
  sys_disk_write_enable(1);
  check_create_dir(STATE_DIR);
  if (f_open(f, STATE_FILE, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
    UINT bw = 0;
    if (f_write(f, state, hp41_state_size(), &bw) == FR_OK && bw == hp41_state_size()) ok = 1;
    f_close(f);
  }
  sys_disk_write_enable(0);
  free(f);
  return ok;
}

static int load_state(void) {
  FIL *f = malloc(sizeof(FIL));
  if (!f) return 0;
  int ok = 0;
  if (f_open(f, STATE_FILE, FA_READ) == FR_OK) {
    UINT br = 0;
    if (f_read(f, state, hp41_state_size(), &br) == FR_OK && br == hp41_state_size()) ok = 1;
    f_close(f);
  }
  free(f);
  return ok;
}

/* ================= program text files ================= */
static int import_file(const char *fpath, const char *fname, void *data) {
  (void)fname; (void)data;
  FIL *f = malloc(sizeof(FIL));
  if (!f) return MRET_EXIT;
  char msg[64];
  int lines = 0, bad = 0;
  if (f_open(f, fpath, FA_READ) != FR_OK) { free(f); return MRET_EXIT; }
  hp41_goto_new_program();
  char line[96]; int n = 0; char c; UINT br;
  for (;;) {
    FRESULT r = f_read(f, &c, 1, &br);
    int eof = (r != FR_OK || br == 0);
    if (eof || c == '\n') {
      line[n] = 0;
      if (n > 0) {
        char err[48];
        if (!hp41_import_line(line, err, sizeof err)) {
          bad++;
          if (bad == 1) snprintf(msg, sizeof msg, "Line %d: %s", lines + 1, err);
        }
      }
      lines++;
      n = 0;
      if (eof) break;
    } else if (n < (int)sizeof line - 1) line[n++] = c;
  }
  f_close(f);
  free(f);
  lcd_clear_buf();
  lcd_writeClr(t24);
  lcd_putsR(t24, "Import");
  lcd_puts(t24, bad ? "Finished with errors" : "Program loaded");
  if (bad) lcd_puts(t20, msg);
  lcd_puts(t24, "");
  lcd_puts(t24, "Press any key");
  lcd_refresh();
  wait_for_key_press();
  return MRET_EXIT;
}

typedef struct { FIL *f; } exp_ctx_t;
static int exp_line(void *ctx, const char *l) {
  exp_ctx_t *c = ctx; UINT bw;
  f_write(c->f, l, strlen(l), &bw);
  f_write(c->f, "\n", 1, &bw);
  return 1;
}
static int export_file(const char *fpath, const char *fname, void *data) {
  (void)fname; (void)data;
  FIL *f = malloc(sizeof(FIL));
  if (!f) return MRET_EXIT;
  sys_disk_write_enable(1);
  if (f_open(f, fpath, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
    exp_ctx_t c = { f };
    hp41_export_program(exp_line, &c);
    f_close(f);
  }
  sys_disk_write_enable(0);
  free(f);
  return MRET_EXIT;
}

/* ================= setup menu (SHIFT 0) ================= */
#define MI_IMPORT   1
#define MI_EXPORT   2
#define MI_SAVE     3
#define MI_CLEAR    4
#define MI_ABOUT41  5

static const uint8_t mid_menu[] = { MI_IMPORT, MI_EXPORT, MI_SAVE, MI_MSC, MI_BEEP_MUTE, MI_CLEAR, MI_SYSTEM_ENTER, MI_ABOUT41, 0 };
static const smenu_t MID_MENU = { "HP-41CV Setup", mid_menu, NULL, NULL };

static void about(void) {
  lcd_clear_buf();
  lcd_writeClr(t24);
  lcd_putsR(t24, "HP-41CV for DM42");
  lcd_puts(t24, "Version " PROGRAM_VERSION);
  lcd_puts(t20, "Clean-room re-implementation of");
  lcd_puts(t20, "the HP-41CV, no HP ROM code.");
  lcd_puts(t20, "Keys follow the DM42 labels.");
  lcd_puts(t20, "Programs: /HP41CV/*.TXT");
  lcd_puts(t20, "");
  lcd_puts(t20, "Press any key");
  lcd_refresh();
  wait_for_key_press();
}

static int confirm(const char *q) {
  lcd_clear_buf();
  lcd_writeClr(t24);
  lcd_putsR(t24, "Confirm");
  lcd_puts(t24, q);
  lcd_puts(t24, "");
  lcd_puts(t24, "ENTER = yes, EXIT = no");
  lcd_refresh();
  for (;;) {
    while (key_empty()) sys_sleep();
    int k = key_pop();
    if (k == KEY_ENTER) { wait_for_key_release(-1); return 1; }
    if (k == KEY_EXIT) { wait_for_key_release(-1); return 0; }
  }
}

static int run_menu_item(uint8_t id) {
  switch (id) {
    case MI_IMPORT:
      check_create_dir(PGM_DIR);
      file_selection_screen("Load program (.txt)", PGM_DIR, ".txt", import_file, 0, 0, NULL);
      return MRET_EXIT;
    case MI_EXPORT:
      check_create_dir(PGM_DIR);
      file_selection_screen("Save current program", PGM_DIR, ".txt", export_file, 1, 1, NULL);
      return MRET_EXIT;
    case MI_SAVE:
      save_state();
      return MRET_EXIT;
    case MI_CLEAR:
      if (confirm("MEMORY LOST?")) hp41_master_clear();
      return MRET_EXIT;
    case MI_ABOUT41:
      about();
      return 0;
  }
  return MRET_UNIMPL;
}

static const char *menu_line_str(uint8_t id, char *s, const int slen) {
  switch (id) {
    case MI_IMPORT:  return "Load program >";
    case MI_EXPORT:  return "Save program >";
    case MI_SAVE:    return "Save state now";
    case MI_CLEAR:   return "Master clear";
    case MI_ABOUT41: return "About >";
  }
  return NULL;
}

/* ================= main loop ================= */
void program_main(void) {
  run_menu_item_app = run_menu_item;
  menu_line_str_app = menu_line_str;

  hp41_set_platform(&plat);
  state = malloc(hp41_state_size());
  if (!state) {
    lcd_clear_buf(); lcd_putsAt(t24, 2, "Not enough memory"); lcd_refresh();
    for (;;) sys_sleep();
  }
  SET_ST(STAT_CLK_WKUP_ENABLE);        /* wake up each minute: header clock, alarms */
  int restored = load_state();
  hp41_attach_state(state, restored);
  hp41_set_lowbat(get_lowbat_state());
  redraw();

  uint32_t last_draw = 0;
  for (;;) {
    int busy = hp41_busy();
    /* CLOCK display only needs one update per second: sleep between them */
    int clock_only = busy == 16;
    if (clock_only) SET_ST(STAT_CLK_WKUP_SECONDS); else CLR_ST(STAT_CLK_WKUP_SECONDS);
    if ((ST(STAT_PGM_END) && ST(STAT_SUSPENDED)) || (!ST(STAT_PGM_END) && key_empty() && (!busy || clock_only))) {
      CLR_ST(STAT_RUNNING);
      sys_sleep();
    }

    if (ST(STAT_CLK_WKUP_FLAG)) {
      CLR_ST(STAT_CLK_WKUP_FLAG);
      if (ST(STAT_OFF)) {
        if (hp41_check_alarms()) {       /* alarm while off: turn on */
          CLR_ST(STAT_PGM_END);
          CLR_ST(STAT_SUSPENDED);
          LCD_power_on();
          rtc_wakeup_delay();
          CLR_ST(STAT_OFF);
          reset_auto_off();
          redraw();
        }
        continue;
      }
      hp41_tick();
      redraw();
      if (key_empty() && !(hp41_busy() & ~16)) continue;
    }

    if (ST(STAT_PGM_END) || ST(STAT_SUSPENDED)) {
      if (!ST(STAT_SUSPENDED)) {
        save_state();
        lcd_set_buf_cleared(0);
        draw_power_off_image(1);
        LCD_power_off(0);
        SET_ST(STAT_SUSPENDED);
        SET_ST(STAT_OFF);
      }
      continue;
    }

    SET_ST(STAT_RUNNING);
    CLR_ST(STAT_SUSPENDED);

    if (ST(STAT_OFF)) {
      LCD_power_on();
      rtc_wakeup_delay();
      CLR_ST(STAT_OFF);
      hp41_set_lowbat(get_lowbat_state());
      hp41_wakeup();
      key_pop_all();        /* swallow the wake-up key */
      redraw();
      continue;
    }

    if (!key_empty()) {
      reset_auto_off();
      int key = key_pop();
      if (key == KEY_DOUBLE_RELEASE) key = 0;
      if (key == KEY_SCREENSHOT) { create_screenshot(1); continue; }
      if (key == KEY_SH_UP)   { if (!hp41_shift_active()) hp41_key(K_SHIFT); key = K_UP; }
      if (key == KEY_SH_DOWN) { if (!hp41_shift_active()) hp41_key(K_SHIFT); key = K_DOWN; }
      if (key == KEY_0 && hp41_shift_active() && !(hp41_busy() & 1)) {
        /* SHIFT 0 = SETUP (as on the DM42) */
        hp41_clear_shift();
        wait_for_key_release(-1);
        SET_ST(STAT_MENU);
        handle_menu(&MID_MENU, MENU_RESET, 0);
        CLR_ST(STAT_MENU);
        wait_for_key_release(-1);
        redraw();
        continue;
      }
      if (key >= 0) hp41_key(key);
      if (hp41_wants_off()) { SET_ST(STAT_PGM_END); continue; }
      redraw();
      last_draw = sys_current_ms();
      continue;
    }

    if (busy) {
      hp41_tick();
      if (busy & 1) {
        reset_auto_off();
        hp41_run_slice(40);
        if (hp41_wants_off()) { SET_ST(STAT_PGM_END); continue; }
      } else if (busy & 2) {
        sys_delay(10);
      } else {
        sys_delay(5);
      }
      uint32_t t = sys_current_ms();
      if (t - last_draw > 100 || !(hp41_busy() & 1)) { redraw(); last_draw = t; }
      continue;
    }
    if (keep_on) reset_auto_off();
  }
}

/* newlib needs this when abort() is linked in */
void _exit(int status) { (void)status; for (;;) sys_sleep(); }
