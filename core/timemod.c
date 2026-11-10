/* HP-41CX time module functions (clean-room) */
#include <math.h>
#include "core.h"

#define FLG_DMY 31

/* ---------- calendar ---------- */
static int64_t jdn(int y, int m, int d) {
  int a = (14 - m) / 12;
  int64_t yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}
static void from_jdn(int64_t j, int *y, int *m, int *d) {
  int64_t a = j + 32044, b = (4 * a + 3) / 146097, c = a - 146097 * b / 4;
  int64_t dd = (4 * c + 3) / 1461, e = c - 1461 * dd / 4, mm = (5 * e + 2) / 153;
  *d = (int)(e - (153 * mm + 2) / 5 + 1);
  *m = (int)(mm + 3 - 12 * (mm / 10));
  *y = (int)(100 * b + dd - 4800 + mm / 10);
}
static int days_in(int y, int m) {
  static const int dm[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return dm[m - 1];
}

static void get_now(hp41_time_t *t) {
  if (PLAT && PLAT->get_time) { PLAT->get_time(t); return; }
  memset(t, 0, sizeof *t);
  t->year = 2026; t->month = 1; t->day = 1;
  uint32_t ms = PLAT && PLAT->ms ? PLAT->ms() : 0;
  t->sec = (ms / 1000) % 60; t->min = (ms / 60000) % 60; t->hour = (ms / 3600000) % 24;
  t->csec = (ms / 10) % 100;
}
static int64_t abs_cs(const hp41_time_t *t) {
  return ((jdn(t->year, t->month, t->day) * 86400 + t->hour * 3600 + t->min * 60 + t->sec) * 100) + t->csec;
}
static void from_abs_cs(int64_t cs, hp41_time_t *t) {
  int64_t s = cs / 100;
  t->csec = (int)(cs % 100);
  int64_t day = s / 86400, r = s % 86400;
  from_jdn(day, &t->year, &t->month, &t->day);
  t->hour = (int)(r / 3600); t->min = (int)(r / 60 % 60); t->sec = (int)(r % 60);
}
int64_t tm_now_cs(void) { hp41_time_t t; get_now(&t); return abs_cs(&t); }

/* ---------- number <-> date/time ---------- */
static int dmy(void) { return FL_GET(FLG_DMY); }

static int parse_date(double x, int *y, int *m, int *d) {
  if (x < 0 || x >= 100) { raise_err(E_DATA); return 0; }
  int a = (int)x;
  int64_t r = (int64_t)floor((x - a) * 1e6 + 0.5);
  int b = (int)(r / 10000), yy = (int)(r % 10000);
  int mm = dmy() ? b : a, dd = dmy() ? a : b;
  if (mm < 1 || mm > 12 || yy < 1583 || yy > 4320 || dd < 1 || dd > days_in(yy, mm)) { raise_err(E_DATA); return 0; }
  *y = yy; *m = mm; *d = dd;
  return 1;
}
static double date_num(int y, int m, int d) {
  int a = dmy() ? d : m, b = dmy() ? m : d;
  return num_round(a + b / 100.0 + y / 1e6);
}
static int parse_time(double x, int64_t *cs) {
  int neg = x < 0; x = fabs(x);
  if (x >= 1e6) { raise_err(E_DATA); return 0; }
  int h = (int)x;
  int64_t r = (int64_t)floor((x - h) * 1e6 + 0.5);
  int mi = (int)(r / 10000), s = (int)(r / 100 % 100), c = (int)(r % 100);
  if (mi > 59 || s > 59) { raise_err(E_DATA); return 0; }
  int64_t v = ((int64_t)h * 3600 + mi * 60 + s) * 100 + c;
  *cs = neg ? -v : v;
  return 1;
}
static double time_num(int64_t cs) {
  int neg = cs < 0; if (neg) cs = -cs;
  int64_t s = cs / 100;
  double v = (double)(s / 3600) + (double)(s / 60 % 60) / 100.0 + (double)(s % 60) / 1e4 + (double)(cs % 100) / 1e6;
  v = num_round(v);
  return neg ? -v : v;
}

static int disp_digits(void) { return (FL_GET(36) << 3) | (FL_GET(37) << 2) | (FL_GET(38) << 1) | FL_GET(39); }

static int put2(uint8_t *o, int v) { o[0] = (uint8_t)('0' + v / 10 % 10); o[1] = (uint8_t)('0' + v % 10); return 2; }

static int fmt_date(int y, int m, int d, int fullyear, uint8_t *o) {
  int n = 0;
  n += put2(o + n, dmy() ? d : m);
  o[n++] = dmy() ? '.' : '/';
  n += put2(o + n, dmy() ? m : d);
  o[n++] = dmy() ? '.' : '/';
  if (fullyear) { n += put2(o + n, y / 100); }
  n += put2(o + n, y % 100);
  return n;
}
/* parts: 1 = HH, 2 = HH:MM, 3 = HH:MM:SS, 4 = +.cc */
static int fmt_time(int64_t cs, int parts, int h24, uint8_t *o) {
  int n = 0;
  if (cs < 0) { o[n++] = '-'; cs = -cs; }
  int64_t s = cs / 100;
  int h = (int)(s / 3600), mi = (int)(s / 60 % 60), sec = (int)(s % 60), c = (int)(cs % 100);
  int pm = 0, hh = h;
  if (!h24) { pm = (h % 24) >= 12; hh = h % 12; if (hh == 0) hh = 12; }
  if (hh >= 100) o[n++] = (uint8_t)('0' + hh / 100 % 10);
  if (hh >= 10 || h24) n += put2(o + n, hh); else o[n++] = (uint8_t)('0' + hh);
  if (parts >= 2) { o[n++] = ':'; n += put2(o + n, mi); }
  if (parts >= 3) { o[n++] = ':'; n += put2(o + n, sec); }
  if (parts >= 4) { o[n++] = '.'; n += put2(o + n, c); }
  if (!h24) { o[n++] = ' '; o[n++] = pm ? 'P' : 'A'; o[n++] = 'M'; }
  return n;
}

int tm_clock_text(uint8_t *o) {
  hp41_time_t t; get_now(&t);
  int64_t cs = ((int64_t)t.hour * 3600 + t.min * 60 + t.sec) * 100;
  if (!S->clkmode) return fmt_time(cs, 3, S->clk24, o);
  int n = fmt_time(cs, 2, S->clk24, o);
  if (!S->clk24) n--;               /* "10:35 P" */
  o[n++] = ' ';
  n += put2(o + n, dmy() ? t.day : t.month);
  o[n++] = dmy() ? '.' : '/';
  n += put2(o + n, dmy() ? t.month : t.day);
  return n;
}

int64_t tm_sw_value(void) {
  int64_t v = S->sw_acc;
  if (S->sw_run) v += tm_now_cs() - S->sw_start;
  return v;
}
int tm_sw_text(uint8_t *o) {
  int64_t v = tm_sw_value();
  return fmt_time(v, 4, 1, o);
}

/* ---------- alarms ---------- */
static void alm_sort(void) {
  for (int i = 1; i < S->nalm; i++)
    for (int j = i; j > 0 && S->alm[j - 1].due > S->alm[j].due; j--) {
      __typeof__(S->alm[0]) t = S->alm[j]; S->alm[j] = S->alm[j - 1]; S->alm[j - 1] = t;
    }
}
int tm_alarm_text(int i, uint8_t *o) {
  if (i < 0 || i >= S->nalm) return -1;
  hp41_time_t t; from_abs_cs(S->alm[i].due, &t);
  int n = 0;
  n += put2(o + n, dmy() ? t.day : t.month); o[n++] = dmy() ? '.' : '/';
  n += put2(o + n, dmy() ? t.month : t.day); o[n++] = ' ';
  n += fmt_time(((int64_t)t.hour * 3600 + t.min * 60) * 100, 2, 1, o + n);
  if (S->alm[i].len) { o[n++] = ' '; memcpy(o + n, S->alm[i].msg, S->alm[i].len); n += S->alm[i].len; }
  return n;
}
static void fire_alarm(int i) {
  uint8_t msg[24]; int len = S->alm[i].len;
  memcpy(msg, S->alm[i].msg, len);
  if (S->alm[i].rep > 0) {
    int64_t now = tm_now_cs();
    while (S->alm[i].due <= now) S->alm[i].due += S->alm[i].rep;
    alm_sort();
  } else {
    S->alm[i] = S->alm[--S->nalm];
    alm_sort();
  }
  /* a message naming a global label runs it (program alarm) */
  if (len && !running && find_global_label(msg, len) >= 0) {
    err = 0; S->rsp = 0;
    xeq_name(msg, len, 0);
    err = 0;
    return;
  }
  if (FL_GET(FLG_AUDIO)) for (int k = 0; k < 3; k++) { ui_beep(1760, 120); ui_beep(1320, 120); }
  if (len) show_message(msg, len);
  else show_message_str("ALARM");
}
int tm_check_alarms(void) {
  if (!S || S->nalm == 0) return 0;
  int64_t now = tm_now_cs();
  if (S->alm[0].due > now) return 0;
  fire_alarm(0);
  return 1;
}
int hp41_check_alarms(void) { return tm_check_alarms(); }

/* ---------- functions ---------- */

void f_date(int a) { (void)a; hp41_time_t t; get_now(&t); push_num(date_num(t.year, t.month, t.day)); }
void f_time(int a) { (void)a; push_num(time_num(tm_now_cs() % 8640000)); }
void f_dateplus(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  int y, m, d;
  if (!parse_date(vy(), &y, &m, &d)) return;
  if (fabs(vx()) > 1e7) { raise_err(E_DATA); return; }
  int64_t j = jdn(y, m, d) + (int64_t)trunc(vx());
  from_jdn(j, &y, &m, &d);
  if (y < 1583 || y > 4320) { raise_err(E_DATA); return; }
  double r = date_num(y, m, d);
  save_lastx(); drop_set_x(r);
}
void f_ddays(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  int y1, m1, d1, y2, m2, d2;
  if (!parse_date(vy(), &y1, &m1, &d1) || !parse_date(vx(), &y2, &m2, &d2)) return;
  save_lastx(); drop_set_x((double)(jdn(y2, m2, d2) - jdn(y1, m1, d1)));
}
void f_dow(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int y, m, d;
  if (!parse_date(vx(), &y, &m, &d)) return;
  int w = (int)((jdn(y, m, d) + 1) % 7);     /* 0 = Sunday */
  save_lastx(); set_x(w);
  if (!running) {
    static const char *nm[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    show_message_str(nm[w]);
  }
}
void f_dmy(int a) { (void)a; FL_SET(FLG_DMY); }
void f_mdy(int a) { (void)a; FL_CLR(FLG_DMY); }
void f_clk12(int a) { (void)a; S->clk24 = 0; }
void f_clk24(int a) { (void)a; S->clk24 = 1; }
void f_clkt(int a) { (void)a; S->clkmode = 0; }
void f_clktd(int a) { (void)a; S->clkmode = 1; }
void f_clock(int a) { (void)a; stop_running(); ui_clock_mode(1); }
static void set_abs(int64_t cs) {
  hp41_time_t t; from_abs_cs(cs, &t);
  if (PLAT && PLAT->set_time) PLAT->set_time(&t);
}
void f_setdate(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int y, m, d;
  if (!parse_date(vx(), &y, &m, &d)) return;
  hp41_time_t t; get_now(&t);
  t.year = y; t.month = m; t.day = d;
  if (PLAT && PLAT->set_time) PLAT->set_time(&t);
}
void f_setime(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int64_t cs;
  if (!parse_time(vx(), &cs)) return;
  if (cs < 0 || cs >= 8640000) { raise_err(E_DATA); return; }
  hp41_time_t t; get_now(&t);
  int64_t day = jdn(t.year, t.month, t.day);
  set_abs(day * 8640000 + cs);
}
void f_tplusx(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int64_t cs;
  if (!parse_time(vx(), &cs)) return;
  set_abs(tm_now_cs() + cs);
}
void f_adate(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int y, m, d;
  if (!parse_date(vx(), &y, &m, &d)) return;
  uint8_t b[16];
  int n = fmt_date(y, m, d, disp_digits() >= 6, b);
  alpha_append(b, n);
}
static void atime(int force24) {
  if (!need_num(&S->stk[SX])) return;
  int64_t cs;
  if (!parse_time(vx(), &cs)) return;
  int nd = disp_digits();
  int parts = nd < 2 ? 1 : nd < 4 ? 2 : nd < 6 ? 3 : 4;
  int h24 = force24 || S->clk24 || fabs(vx()) >= 24;
  uint8_t b[24];
  int n = fmt_time(cs, parts, h24, b);
  alpha_append(b, n);
}
void f_atime(int a) { (void)a; atime(0); }
void f_atime24(int a) { (void)a; atime(1); }
void f_sw(int a) { (void)a; stop_running(); ui_sw_mode(1); }
void f_setsw(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  int64_t cs;
  if (!parse_time(vx(), &cs)) return;
  S->sw_acc = cs;
  if (S->sw_run) S->sw_start = tm_now_cs();
}
void f_rclsw(int a) { (void)a; push_num(time_num(tm_sw_value())); }
void f_runsw(int a) { (void)a; if (!S->sw_run) { S->sw_start = tm_now_cs(); S->sw_run = 1; } }
void f_stopsw(int a) { (void)a; if (S->sw_run) { S->sw_acc = tm_sw_value(); S->sw_run = 0; } }
void f_xyzalm(int a) {
  (void)a;
  for (int i = 0; i < 3; i++) if (!need_num(&S->stk[i])) return;
  if (S->nalm >= MAXALM) { raise_err(E_NOROOM); return; }
  int64_t tcs, rep = 0;
  if (!parse_time(vx(), &tcs)) return;
  if (tcs < 0 || tcs >= 8640000) { raise_err(E_DATA); return; }
  hp41_time_t now; get_now(&now);
  int64_t day;
  if (vy() == 0) {
    day = jdn(now.year, now.month, now.day);
    if (day * 8640000 + tcs <= abs_cs(&now)) day++;
  } else {
    int y, m, d;
    if (!parse_date(vy(), &y, &m, &d)) return;
    day = jdn(y, m, d);
  }
  if (S->stk[SZ].x != 0) {
    if (!parse_time(S->stk[SZ].x, &rep)) return;
    if (rep < 0) rep = -rep;
    if (rep < 1000) rep = 1000;          /* at least 10 seconds */
  }
  int i = S->nalm++;
  S->alm[i].due = day * 8640000 + tcs;
  S->alm[i].rep = rep;
  S->alm[i].len = (uint8_t)(S->alen > 24 ? 24 : S->alen);
  memcpy(S->alm[i].msg, S->alpha, S->alm[i].len);
  alm_sort();
}
void f_almcat(int a) { (void)a; stop_running(); ui_almcat(); }
void f_almnow(int a) { (void)a; if (S->nalm) fire_alarm(0); }
void f_clralms(int a) { (void)a; S->nalm = 0; }
void f_rclalm(int a) {
  (void)a;
  if (!S->nalm) { raise_err(E_NONEX); return; }
  hp41_time_t t; from_abs_cs(S->alm[0].due, &t);
  int64_t tod = ((int64_t)t.hour * 3600 + t.min * 60 + t.sec) * 100;
  push_num(time_num(S->alm[0].rep));
  push_num(date_num(t.year, t.month, t.day));
  push_num(time_num(tod));
  alpha_clear(); alpha_append(S->alm[0].msg, S->alm[0].len);
}
