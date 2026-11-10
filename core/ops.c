/* HP-41CV built-in functions */
#include <math.h>
#include "core.h"

int err;
int skip_next;   /* conditional false in a running program */

void raise_err(int e) { if (!err) err = e; }

/* ---------- stack helpers ---------- */
static val_t numv(double x) { val_t v; memset(&v, 0, sizeof v); v.x = x; return v; }

void lift_stack(void) {
  S->stk[ST] = S->stk[SZ];
  S->stk[SZ] = S->stk[SY];
  S->stk[SY] = S->stk[SX];
}
void push_num(double x) {
  if (S->lift) lift_stack();
  S->stk[SX] = numv(x);
  S->lift = 1;
}
static void push_val(const val_t *v) {
  val_t t = *v;
  if (S->lift) lift_stack();
  S->stk[SX] = t;
  S->lift = 1;
}
void set_x(double x) { S->stk[SX] = numv(x); S->lift = 1; }
int need_num(const val_t *v) { if (v->isalpha) { raise_err(E_ALPHA); return 0; } return 1; }
double vx(void) { return S->stk[SX].x; }
double vy(void) { return S->stk[SY].x; }
void save_lastx(void) { S->stk[SL] = S->stk[SX]; }
void drop_set_x(double x) {
  S->stk[SX] = numv(x);
  S->stk[SY] = S->stk[SZ];
  S->stk[SZ] = S->stk[ST];
  S->lift = 1;
}

/* unary numeric function frame */
#define UNARY(expr) do { \
  if (!need_num(&S->stk[SX])) return; \
  double x = vx(); (void)x; double r = (expr); \
  if (err) return; r = num_round(r); if (err) return; \
  save_lastx(); set_x(r); } while (0)

#define BINARY(expr) do { \
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return; \
  double x = vx(), y = vy(); (void)x; (void)y; double r = (expr); \
  if (err) return; r = num_round(r); if (err) return; \
  save_lastx(); drop_set_x(r); } while (0)

static double data_err(void) { raise_err(E_DATA); return 0; }

/* ---------- angle helpers ---------- */
static int angmode(void) { return FL_GET(FLG_RAD) ? 1 : FL_GET(FLG_GRAD) ? 2 : 0; }
double to_rad(double a) {
  switch (angmode()) { case 1: return a; case 2: return a * M_PI / 200.0; default: return a * M_PI / 180.0; }
}
double from_rad(double a) {
  switch (angmode()) { case 1: return a; case 2: return a * 200.0 / M_PI; default: return a * 180.0 / M_PI; }
}
/* exact quadrant handling in DEG / GRAD: returns quadrant 0..3 if x is an exact multiple of 90 deg, else -1 */
static int exact_quadrant(double x) {
  int m = angmode();
  if (m == 1) return -1;
  double full = m == 2 ? 400.0 : 360.0, q = full / 4;
  double r = fmod(x, full);
  if (r < 0) r += full;
  if (fmod(r, q) != 0) return -1;
  return (int)(r / q) & 3;
}
static double f_sin_(double x) {
  int q = exact_quadrant(x);
  if (q >= 0) return q == 1 ? 1 : q == 3 ? -1 : 0;
  if (angmode() != 1) { double full = angmode() == 2 ? 400 : 360; x = fmod(x, full); }
  return sin(to_rad(x));
}
static double f_cos_(double x) {
  int q = exact_quadrant(x);
  if (q >= 0) return q == 0 ? 1 : q == 2 ? -1 : 0;
  if (angmode() != 1) { double full = angmode() == 2 ? 400 : 360; x = fmod(x, full); }
  return cos(to_rad(x));
}
static double f_tan_(double x) {
  int q = exact_quadrant(x);
  if (q == 0 || q == 2) return 0;
  if (q == 1 || q == 3) { raise_err(E_RANGE); if (FL_GET(FLG_RANGE)) { err = 0; return q == 1 ? 9.999999999e99 : -9.999999999e99; } return 0; }
  if (angmode() != 1) { double full = angmode() == 2 ? 400 : 360; x = fmod(x, full); }
  return tan(to_rad(x));
}

/* ---------- arithmetic ---------- */
static void f_add(int a) { (void)a; BINARY(y + x); }
static void f_sub(int a) { (void)a; BINARY(y - x); }
static void f_mul(int a) { (void)a; BINARY(y * x); }
static void f_div(int a) { (void)a; BINARY(x == 0 ? data_err() : y / x); }
static void f_chs(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  S->stk[SX].x = -S->stk[SX].x;
  S->lift = 1;
}
static void f_abs(int a)  { (void)a; UNARY(fabs(x)); }
static void f_int(int a)  { (void)a; UNARY(trunc(x)); }
static void f_frc(int a)  { (void)a; UNARY(x - trunc(x)); }
static void f_inv(int a)  { (void)a; UNARY(x == 0 ? data_err() : 1.0 / x); }
static void f_sqrt(int a) { (void)a; UNARY(x < 0 ? data_err() : sqrt(x)); }
static void f_sq(int a)   { (void)a; UNARY(x * x); }
static void f_ln(int a)   { (void)a; UNARY(x <= 0 ? data_err() : log(x)); }
static void f_log(int a)  { (void)a; UNARY(x <= 0 ? data_err() : log10(x)); }
static void f_exp(int a)  { (void)a; UNARY(exp(x)); }
static void f_alog(int a) { (void)a; UNARY(pow(10.0, x)); }
static void f_ln1p(int a) { (void)a; UNARY(x <= -1 ? data_err() : log1p(x)); }
static void f_expm1(int a){ (void)a; UNARY(expm1(x)); }
static void f_sin(int a)  { (void)a; UNARY(f_sin_(x)); }
static void f_cos(int a)  { (void)a; UNARY(f_cos_(x)); }
static void f_tan(int a)  { (void)a; UNARY(f_tan_(x)); }
static void f_asin(int a) { (void)a; UNARY(fabs(x) > 1 ? data_err() : from_rad(asin(x))); }
static void f_acos(int a) { (void)a; UNARY(fabs(x) > 1 ? data_err() : from_rad(acos(x))); }
static void f_atan(int a) { (void)a; UNARY(from_rad(atan(x))); }
static void f_d_r(int a)  { (void)a; UNARY(x * M_PI / 180.0); }
static void f_r_d(int a)  { (void)a; UNARY(x * 180.0 / M_PI); }

static double ypowx(double y, double x) {
  if (y == 0) { if (x <= 0) return data_err(); return 0; }
  if (y < 0) {
    if (x != trunc(x)) return data_err();
    double r = pow(-y, x);
    return fmod(fabs(x), 2.0) == 1.0 ? -r : r;
  }
  return pow(y, x);
}
static void f_pow(int a) { (void)a; BINARY(ypowx(y, x)); }

static double fact(double x) {
  if (x < 0 || x != trunc(x)) return data_err();
  if (x > 69) { raise_err(E_RANGE); return 0; }
  double r = 1;
  for (int i = 2; i <= (int)x; i++) r *= i;
  return r;
}
static void f_fact(int a) { (void)a; UNARY(fact(x)); }

static double fmodhp(double y, double x) {
  if (x == 0) return y;
  double r = fmod(y, x);
  if (r != 0 && ((r < 0) != (x < 0))) r += x;
  return r;
}
static void f_mod(int a) { (void)a; BINARY(fmodhp(y, x)); }

static void f_pct(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  double r = num_round(vx() * vy() / 100.0);
  if (err) return;
  save_lastx(); set_x(r);
}
static void f_pctch(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  if (vy() == 0) { raise_err(E_DATA); return; }
  double r = num_round((vx() - vy()) / vy() * 100.0);
  if (err) return;
  save_lastx(); set_x(r);
}
static void f_sign(int a) {
  (void)a;
  double r = S->stk[SX].isalpha ? 0 : (vx() < 0 ? -1 : 1);
  save_lastx(); set_x(r);
}
static void f_rnd(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  uint8_t buf[32];
  int n = num_format(vx(), buf);
  /* convert the displayed string back into a number */
  char t[40]; int j = 0;
  uint8_t rad = FL_GET(FLG_RADIX) ? '.' : ',';
  int exp_part = 0;
  for (int i = 0; i < n; i++) {
    uint8_t c = buf[i];
    if (c >= '0' && c <= '9') t[j++] = (char)c;
    else if (c == rad && !exp_part) t[j++] = '.';
    else if (c == '-' && j == 0) t[j++] = '-';
    else if ((c == ' ' || c == '-') && j > 0) { t[j++] = 'E'; if (c == '-') t[j++] = '-'; exp_part = 1; }
  }
  t[j] = 0;
  double r;
  if (!num_parse(t, &r)) r = vx();
  save_lastx(); set_x(r);
}

/* hours <-> H.MMSS */
static double to_hms(double x) {
  int neg = x < 0; x = fabs(x);
  double h = trunc(x);
  double mfull = (x - h) * 60.0;
  double m = trunc(mfull + 1e-9);
  double s = (mfull - m) * 60.0;
  if (s < 0) s = 0;
  double r = h + m / 100.0 + s / 10000.0;
  return neg ? -r : r;
}
static double from_hms(double x) {
  int neg = x < 0; x = fabs(x);
  double h = trunc(x);
  double f = num_round((x - h) * 100.0);
  double m = trunc(f + 1e-9);
  double s = (f - m) * 100.0;
  double r = h + m / 60.0 + s / 3600.0;
  return neg ? -r : r;
}
static void f_hms(int a) { (void)a; UNARY(to_hms(x)); }
static void f_hr(int a)  { (void)a; UNARY(from_hms(x)); }
static void f_hmsadd(int a) { (void)a; BINARY(to_hms(num_round(from_hms(y) + from_hms(x)))); }
static void f_hmssub(int a) { (void)a; BINARY(to_hms(num_round(from_hms(y) - from_hms(x)))); }

static void f_p_r(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  double r = vx(), th = vy();
  double c = f_cos_(th), s = f_sin_(th);
  double nx = num_round(r * c), ny = num_round(r * s);
  if (err) return;
  save_lastx();
  S->stk[SX] = numv(nx); S->stk[SY] = numv(ny); S->lift = 1;
}
static void f_r_p(int a) {
  (void)a;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  double x = vx(), y = vy();
  double r = num_round(hypot(x, y));
  double th = num_round(from_rad(atan2(y, x)));
  if (err) return;
  save_lastx();
  S->stk[SX] = numv(r); S->stk[SY] = numv(th); S->lift = 1;
}
static void f_oct(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  double x = vx();
  if (x != trunc(x) || fabs(x) > 1073741823.0) { raise_err(E_DATA); return; }
  long v = (long)fabs(x), r = 0, p = 1;
  while (v) { r += (v % 8) * p; p *= 10; v /= 8; }
  save_lastx(); set_x(x < 0 ? -(double)r : (double)r);
}
static void f_dec(int a) {
  (void)a;
  if (!need_num(&S->stk[SX])) return;
  double x = vx();
  if (x != trunc(x) || fabs(x) > 7777777777.0) { raise_err(E_DATA); return; }
  long long v = (long long)fabs(x), r = 0, p = 1;
  while (v) { int d = (int)(v % 10); if (d > 7) { raise_err(E_DATA); return; } r += d * p; p *= 8; v /= 10; }
  save_lastx(); set_x(x < 0 ? -(double)r : (double)r);
}
static void f_pi(int a) { (void)a; push_num(3.141592654); }

/* ---------- stack ---------- */
static void f_enter(int a) { (void)a; lift_stack(); S->lift = 0; }
static void f_swap(int a) { (void)a; val_t t = S->stk[SX]; S->stk[SX] = S->stk[SY]; S->stk[SY] = t; S->lift = 1; }
static void f_rdn(int a) {
  (void)a;
  val_t t = S->stk[SX];
  S->stk[SX] = S->stk[SY]; S->stk[SY] = S->stk[SZ]; S->stk[SZ] = S->stk[ST]; S->stk[ST] = t;
  S->lift = 1;
}
static void f_clx(int a) { (void)a; S->stk[SX] = numv(0); S->lift = 0; }
static void f_lastx(int a) { (void)a; push_val(&S->stk[SL]); }

/* ---------- registers ---------- */
val_t *reg_ref(int arg, int *ok) {
  *ok = 0;
  if (arg & ARG_IND) {
    int ok2;
    val_t *p = reg_ref(arg & 0x7f, &ok2);
    if (!ok2) return 0;
    if (p->isalpha) { raise_err(E_ALPHA); return 0; }
    double v = fabs(p->x);
    if (v >= 1000) { raise_err(E_NONEX); return 0; }
    int r = (int)v;
    if (r >= S->size) { raise_err(E_NONEX); return 0; }
    *ok = 1;
    return &S->reg[r];
  }
  if (arg >= ARG_ST_T && arg <= ARG_ST_L) {
    static const int map[5] = { ST, SZ, SY, SX, SL };
    *ok = 1;
    return &S->stk[map[arg - ARG_ST_T]];
  }
  if (arg >= S->size || arg >= 112) { raise_err(E_NONEX); return 0; }
  *ok = 1;
  return &S->reg[arg];
}
/* resolve a numeric argument (FIX IND nn, SF IND nn...) */
static int num_arg(int arg, int *ok) {
  *ok = 1;
  if (!(arg & ARG_IND)) return arg;
  int ok2;
  val_t *q = reg_ref(arg & 0x7f, &ok2);
  if (!ok2) { *ok = 0; return 0; }
  if (q->isalpha) { raise_err(E_ALPHA); *ok = 0; return 0; }
  double v = fabs(q->x);
  if (v > 999) { raise_err(E_NONEX); *ok = 0; return 0; }
  return (int)v;
}

static void f_sto(int a) { int ok; val_t *r = reg_ref(a, &ok); if (!ok) return; *r = S->stk[SX]; S->lift = 1; }
static void f_rcl(int a) { int ok; val_t *r = reg_ref(a, &ok); if (!ok) return; push_val(r); }
static void st_arith(int a, int op) {
  int ok; val_t *r = reg_ref(a, &ok); if (!ok) return;
  if (!need_num(r) || !need_num(&S->stk[SX])) return;
  double x = vx(), y = r->x, v = 0;
  switch (op) {
    case 0: v = y + x; break;
    case 1: v = y - x; break;
    case 2: v = y * x; break;
    case 3: if (x == 0) { raise_err(E_DATA); return; } v = y / x; break;
  }
  v = num_round(v);
  if (err) return;
  *r = numv(v);
  S->lift = 1;
}
static void f_stadd(int a) { st_arith(a, 0); }
static void f_stsub(int a) { st_arith(a, 1); }
static void f_stmul(int a) { st_arith(a, 2); }
static void f_stdiv(int a) { st_arith(a, 3); }
static void f_xex(int a) {
  int ok; val_t *r = reg_ref(a, &ok); if (!ok) return;
  val_t t = *r; *r = S->stk[SX]; S->stk[SX] = t; S->lift = 1;
}
static void f_view(int a) { int ok; val_t *r = reg_ref(a, &ok); if (!ok) return; show_view(r); }

static void isg_dse(int a, int dse) {
  int ok; val_t *r = reg_ref(a, &ok); if (!ok) return;
  if (!need_num(r)) return;
  double v = r->x;
  double av = fabs(v);
  double c = trunc(av); if (v < 0) c = -c;
  double frac = av - trunc(av);
  int64_t f5 = (int64_t)floor(frac * 100000.0 + 0.5);
  int fin = (int)(f5 / 100);
  int inc = (int)(f5 % 100);
  if (inc == 0) inc = 1;
  double fr = (double)f5 / 100000.0;
  int skip;
  if (dse) { c -= inc; skip = c <= fin; }
  else     { c += inc; skip = c > fin; }
  double nv = c < 0 ? -(-c + fr) : c + fr;
  if (c == 0) nv = fr;
  nv = num_round(nv);
  if (err) return;
  *r = numv(nv);
  if (skip) { if (running) skip_next = 1; }
  if (!running) show_message_str(skip ? "NO" : "YES");
}
static void f_isg(int a) { isg_dse(a, 0); }
static void f_dse(int a) { isg_dse(a, 1); }

/* ---------- conditionals ---------- */
static void cond(int t) {
  if (running) { if (!t) skip_next = 1; }
  else show_message_str(t ? "YES" : "NO");
}
/* x=y? works on alpha data too */
static int vequal(const val_t *a, const val_t *b) {
  if (a->isalpha != b->isalpha) return 0;
  if (a->isalpha) return a->len == b->len && !memcmp(a->s, b->s, a->len);
  return a->x == b->x;
}
static void f_xeqy(int a) { (void)a; cond(vequal(&S->stk[SX], &S->stk[SY])); }
static void f_xney(int a) { (void)a; cond(!vequal(&S->stk[SX], &S->stk[SY])); }
#define NUMCOND(e) do { if (!need_num(&S->stk[SX])) return; double x = vx(), y = vy(); (void)y; cond(e); } while (0)
#define NUMCOND2(e) do { if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return; double x = vx(), y = vy(); cond(e); } while (0)
static void f_xlty(int a) { (void)a; NUMCOND2(x < y); }
static void f_xley(int a) { (void)a; NUMCOND2(x <= y); }
static void f_xgty(int a) { (void)a; NUMCOND2(x > y); }
static void f_xeq0(int a) { (void)a; if (S->stk[SX].isalpha) { cond(0); return; } NUMCOND(x == 0); }
static void f_xne0(int a) { (void)a; if (S->stk[SX].isalpha) { cond(1); return; } NUMCOND(x != 0); }
static void f_xlt0(int a) { (void)a; NUMCOND(x < 0); }
static void f_xle0(int a) { (void)a; NUMCOND(x <= 0); }
static void f_xgt0(int a) { (void)a; NUMCOND(x > 0); }

/* ---------- flags ---------- */
static void f_sf(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 29) { raise_err(E_NONEX); return; } FL_SET(n); }
static void f_cf(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 29) { raise_err(E_NONEX); return; } FL_CLR(n); }
static void f_fs(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 55) { raise_err(E_NONEX); return; } cond(FL_GET(n)); }
static void f_fc(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 55) { raise_err(E_NONEX); return; } cond(!FL_GET(n)); }
static void f_fsc(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 29) { raise_err(E_NONEX); return; } int t = FL_GET(n); FL_CLR(n); cond(t); }
static void f_fcc(int a) { int ok; int n = num_arg(a, &ok); if (!ok) return; if (n > 29) { raise_err(E_NONEX); return; } int t = !FL_GET(n); FL_CLR(n); cond(t); }

/* ---------- display modes ---------- */
static void set_digits(int a, int fix, int eng) {
  int ok; int n = num_arg(a, &ok); if (!ok) return;
  if (n > 9) { raise_err(E_DATA); return; }
  for (int i = 0; i < 4; i++) { if (n & (8 >> i)) FL_SET(36 + i); else FL_CLR(36 + i); }
  if (fix) FL_SET(FLG_FIX); else FL_CLR(FLG_FIX);
  if (eng) FL_SET(FLG_ENG); else FL_CLR(FLG_ENG);
}
static void f_fix(int a) { set_digits(a, 1, 0); }
static void f_sci(int a) { set_digits(a, 0, 0); }
static void f_eng(int a) { set_digits(a, 0, 1); }
static void f_deg(int a)  { (void)a; FL_CLR(FLG_RAD); FL_CLR(FLG_GRAD); }
static void f_rad(int a)  { (void)a; FL_SET(FLG_RAD); FL_CLR(FLG_GRAD); }
static void f_grad(int a) { (void)a; FL_CLR(FLG_RAD); FL_SET(FLG_GRAD); }

/* ---------- statistics ---------- */
static val_t *sreg(int i) {
  int r = S->sreg + i;
  if (r >= S->size) { raise_err(E_NONEX); return 0; }
  return &S->reg[r];
}
static int sregs_ok(void) {
  if (S->sreg + 6 > S->size) { raise_err(E_NONEX); return 0; }
  for (int i = 0; i < 6; i++) if (S->reg[S->sreg + i].isalpha) { raise_err(E_ALPHA); return 0; }
  return 1;
}
static void sigma(int sign) {
  if (!sregs_ok()) return;
  if (!need_num(&S->stk[SX]) || !need_num(&S->stk[SY])) return;
  double x = vx(), y = vy();
  double v[6];
  for (int i = 0; i < 6; i++) v[i] = S->reg[S->sreg + i].x;
  v[0] += sign * x; v[1] += sign * x * x; v[2] += sign * y;
  v[3] += sign * y * y; v[4] += sign * x * y; v[5] += sign;
  for (int i = 0; i < 6; i++) { v[i] = num_round(v[i]); }
  if (err) return;
  for (int i = 0; i < 6; i++) S->reg[S->sreg + i] = numv(v[i]);
  save_lastx();
  S->stk[SX] = numv(v[5]);
  S->lift = 0;
}
static void f_sigp(int a) { (void)a; sigma(1); }
static void f_sigm(int a) { (void)a; sigma(-1); }
static void f_mean(int a) {
  (void)a;
  if (!sregs_ok()) return;
  double n = sreg(5)->x;
  if (n == 0) { raise_err(E_DATA); return; }
  double mx = num_round(sreg(0)->x / n), my = num_round(sreg(2)->x / n);
  if (err) return;
  save_lastx();
  push_num(my); S->lift = 1; push_num(mx);
}
static void f_sdev(int a) {
  (void)a;
  if (!sregs_ok()) return;
  double n = sreg(5)->x;
  if (n <= 1) { raise_err(E_DATA); return; }
  double vx_ = (sreg(1)->x - sreg(0)->x * sreg(0)->x / n) / (n - 1);
  double vy_ = (sreg(3)->x - sreg(2)->x * sreg(2)->x / n) / (n - 1);
  if (vx_ < 0 || vy_ < 0) { raise_err(E_DATA); return; }
  double sx = num_round(sqrt(vx_)), sy = num_round(sqrt(vy_));
  if (err) return;
  save_lastx();
  push_num(sy); S->lift = 1; push_num(sx);
}
static void f_clsig(int a) {
  (void)a;
  for (int i = 0; i < 6; i++) { int r = S->sreg + i; if (r < S->size) S->reg[r] = numv(0); }
}
static void f_sigreg(int a) {
  int ok; int n;
  if (a & ARG_IND) { n = num_arg(a, &ok); if (!ok) return; }
  else n = a;
  if (n >= 112 || n + 6 > S->size) { raise_err(E_NONEX); return; }
  S->sreg = n;
}

/* ---------- alpha ---------- */
static void f_cla(int a) { (void)a; alpha_clear(); }
static void f_aview(int a) { (void)a; show_message(S->alpha, S->alen); }
static void f_ashf(int a) {
  (void)a;
  int n = S->alen > 6 ? 6 : S->alen;
  memmove(S->alpha, S->alpha + n, S->alen - n);
  S->alen -= n;
}
static void f_asto(int a) {
  int ok; val_t *r = reg_ref(a, &ok); if (!ok) return;
  val_t v; memset(&v, 0, sizeof v);
  v.isalpha = 1;
  v.len = (uint8_t)(S->alen > 6 ? 6 : S->alen);
  memcpy(v.s, S->alpha, v.len);
  *r = v;
}
static void f_arcl(int a) {
  int ok; val_t *r = reg_ref(a, &ok); if (!ok) return;
  uint8_t buf[32];
  int n = val_format(r, buf);
  alpha_append(buf, n);
}
static void f_aon(int a)  { (void)a; FL_SET(FLG_ALPHA); }
static void f_aoff(int a) { (void)a; FL_CLR(FLG_ALPHA); }

/* ---------- misc ---------- */
static const int tone_freq[10] = { 165, 196, 220, 247, 294, 330, 392, 440, 494, 587 };
static void f_beep(int a) {
  (void)a;
  if (!FL_GET(FLG_AUDIO)) return;
  ui_beep(880, 180); ui_beep(660, 180); ui_beep(880, 180); ui_beep(1320, 300);
}
static void f_tone(int a) {
  int ok; int n = num_arg(a, &ok); if (!ok) return;
  if (n > 9) { raise_err(E_DATA); return; }
  if (FL_GET(FLG_AUDIO)) ui_beep(tone_freq[n] * 2, 250);
}
static void f_on(int a)  { (void)a; FL_SET(FLG_CONTON); if (PLAT && PLAT->keep_on) PLAT->keep_on(1); }
static void f_off(int a) { (void)a; ui_off(); }
static void f_cld(int a) { (void)a; msg_active = 0; FL_CLR(FLG_MSG); }
static void f_clrg(int a) { (void)a; for (int i = 0; i < S->size; i++) S->reg[i] = numv(0); }
static void f_pse(int a) { (void)a; ui_pause(1000); }
static void f_null(int a) { (void)a; }
static void f_pack(int a) { (void)a; pack_memory(); show_message_str("PACKING"); }
static void f_size(int a) {
  if (a > NREGS) { raise_err(E_NOROOM); return; }
  int avail = NREGS - regs_used_by_programs();
  if (a > avail) { raise_err(E_NOROOM); return; }
  for (int i = S->size; i < a; i++) S->reg[i] = numv(0);
  S->size = a;
  if (S->sreg + 6 > S->size && S->size >= 6) { /* keep pointer, Σ fns will complain */ }
}

/* functions implemented in exec.c / ui.c */
void f_lbl(int a);
void f_gto(int a);
void f_xeq(int a);
void f_rtn(int a);
void f_stop(int a);
void f_prompt(int a);
void f_end(int a);
void f_del(int a);
void f_cat(int a);
void f_clp(int a);
void f_asn(int a);

/* time module (timemod.c) */
void f_date(int a);
void f_time(int a);
void f_dateplus(int a);
void f_ddays(int a);
void f_dow(int a);
void f_dmy(int a);
void f_mdy(int a);
void f_clk12(int a);
void f_clk24(int a);
void f_clock(int a);
void f_clkt(int a);
void f_clktd(int a);
void f_setdate(int a);
void f_setime(int a);
void f_tplusx(int a);
void f_adate(int a);
void f_atime(int a);
void f_atime24(int a);
void f_sw(int a);
void f_setsw(int a);
void f_rclsw(int a);
void f_runsw(int a);
void f_stopsw(int a);
void f_xyzalm(int a);
void f_almcat(int a);
void f_almnow(int a);
void f_clralms(int a);
void f_rclalm(int a);

/* Table order is part of the stored-program format: append only. */
const fdef_t FN[] = {
  { "",       A_NONE, F_HIDE, f_null },      /* 0 */
  { "+",      A_NONE, 0, f_add },
  { "-",      A_NONE, 0, f_sub },
  { "*",      A_NONE, 0, f_mul },
  { "/",      A_NONE, 0, f_div },
  { "CHS",    A_NONE, 0, f_chs },
  { "ABS",    A_NONE, 0, f_abs },
  { "INT",    A_NONE, 0, f_int },
  { "FRC",    A_NONE, 0, f_frc },
  { "RND",    A_NONE, 0, f_rnd },
  { "SIGN",   A_NONE, 0, f_sign },            /* 10 */
  { "1/X",    A_NONE, 0, f_inv },
  { "SQRT",   A_NONE, 0, f_sqrt },
  { "X^2",    A_NONE, 0, f_sq },
  { "Y^X",    A_NONE, 0, f_pow },
  { "LN",     A_NONE, 0, f_ln },
  { "LOG",    A_NONE, 0, f_log },
  { "E^X",    A_NONE, 0, f_exp },
  { "10^X",   A_NONE, 0, f_alog },
  { "LN1+X",  A_NONE, 0, f_ln1p },
  { "E^X-1",  A_NONE, 0, f_expm1 },           /* 20 */
  { "SIN",    A_NONE, 0, f_sin },
  { "COS",    A_NONE, 0, f_cos },
  { "TAN",    A_NONE, 0, f_tan },
  { "ASIN",   A_NONE, 0, f_asin },
  { "ACOS",   A_NONE, 0, f_acos },
  { "ATAN",   A_NONE, 0, f_atan },
  { "FACT",   A_NONE, 0, f_fact },
  { "MOD",    A_NONE, 0, f_mod },
  { "%",      A_NONE, 0, f_pct },
  { "%CH",    A_NONE, 0, f_pctch },           /* 30 */
  { "PI",     A_NONE, 0, f_pi },
  { "D-R",    A_NONE, 0, f_d_r },
  { "R-D",    A_NONE, 0, f_r_d },
  { "HMS",    A_NONE, 0, f_hms },
  { "HR",     A_NONE, 0, f_hr },
  { "HMS+",   A_NONE, 0, f_hmsadd },
  { "HMS-",   A_NONE, 0, f_hmssub },
  { "P-R",    A_NONE, 0, f_p_r },
  { "R-P",    A_NONE, 0, f_r_p },
  { "OCT",    A_NONE, 0, f_oct },             /* 40 */
  { "DEC",    A_NONE, 0, f_dec },
  { "ENTER^", A_NONE, 0, f_enter },
  { "X<>Y",   A_NONE, 0, f_swap },
  { "RDN",    A_NONE, 0, f_rdn },
  { "CLX",    A_NONE, 0, f_clx },
  { "LASTX",  A_NONE, 0, f_lastx },
  { "\x82+",  A_NONE, 0, f_sigp },
  { "\x82-",  A_NONE, 0, f_sigm },
  { "MEAN",   A_NONE, 0, f_mean },
  { "SDEV",   A_NONE, 0, f_sdev },            /* 50 */
  { "CL\x82", A_NONE, 0, f_clsig },
  { "\x82REG",A_REG,  0, f_sigreg },
  { "X=Y?",   A_NONE, 0, f_xeqy },
  { "X\x83Y?",A_NONE, 0, f_xney },
  { "X<Y?",   A_NONE, 0, f_xlty },
  { "X\x84Y?",A_NONE, 0, f_xley },
  { "X>Y?",   A_NONE, 0, f_xgty },
  { "X=0?",   A_NONE, 0, f_xeq0 },
  { "X\x83" "0?",A_NONE, 0, f_xne0 },
  { "X<0?",   A_NONE, 0, f_xlt0 },            /* 60 */
  { "X\x84" "0?",A_NONE, 0, f_xle0 },
  { "X>0?",   A_NONE, 0, f_xgt0 },
  { "SF",     A_FLAG, 0, f_sf },
  { "CF",     A_FLAG, 0, f_cf },
  { "FS?",    A_FLAG, 0, f_fs },
  { "FC?",    A_FLAG, 0, f_fc },
  { "FS?C",   A_FLAG, 0, f_fsc },
  { "FC?C",   A_FLAG, 0, f_fcc },
  { "STO",    A_REG,  0, f_sto },
  { "RCL",    A_REG,  0, f_rcl },             /* 70 */
  { "ST+",    A_REG,  0, f_stadd },
  { "ST-",    A_REG,  0, f_stsub },
  { "ST*",    A_REG,  0, f_stmul },
  { "ST/",    A_REG,  0, f_stdiv },
  { "X<>",    A_REG,  0, f_xex },
  { "VIEW",   A_REG,  0, f_view },
  { "ISG",    A_REG,  0, f_isg },
  { "DSE",    A_REG,  0, f_dse },
  { "ASTO",   A_REG,  0, f_asto },
  { "ARCL",   A_REG,  0, f_arcl },            /* 80 */
  { "FIX",    A_DIG,  0, f_fix },
  { "SCI",    A_DIG,  0, f_sci },
  { "ENG",    A_DIG,  0, f_eng },
  { "DEG",    A_NONE, 0, f_deg },
  { "RAD",    A_NONE, 0, f_rad },
  { "GRAD",   A_NONE, 0, f_grad },
  { "CLA",    A_NONE, 0, f_cla },
  { "AVIEW",  A_NONE, 0, f_aview },
  { "ASHF",   A_NONE, 0, f_ashf },
  { "AON",    A_NONE, 0, f_aon },             /* 90 */
  { "AOFF",   A_NONE, 0, f_aoff },
  { "PROMPT", A_NONE, 0, f_prompt },
  { "LBL",    A_LBL,  0, f_lbl },
  { "GTO",    A_GOTO, 0, f_gto },
  { "XEQ",    A_GOTO, 0, f_xeq },
  { "RTN",    A_NONE, 0, f_rtn },
  { "STOP",   A_NONE, 0, f_stop },
  { "PSE",    A_NONE, 0, f_pse },
  { "BEEP",   A_NONE, 0, f_beep },
  { "TONE",   A_DIG,  0, f_tone },            /* 100 */
  { "ON",     A_NONE, 0, f_on },
  { "OFF",    A_NONE, 0, f_off },
  { "CLD",    A_NONE, 0, f_cld },
  { "CLRG",   A_NONE, 0, f_clrg },
  { "END",    A_NONE, 0, f_end },
  { "SIZE",   A_SIZE, F_NP, f_size },
  { "PACK",   A_NONE, F_NP, f_pack },
  { "DEL",    A_DEL,  F_NP, f_del },
  { "CAT",    A_CAT,  F_NP, f_cat },
  { "CLP",    A_NONE, F_NP, f_clp },          /* 110 */
  { "ASN",    A_ASN,  F_NP, f_asn },
  { "DATE", A_NONE, 0, f_date },  /* 112 */
  { "TIME", A_NONE, 0, f_time },
  { "DATE+", A_NONE, 0, f_dateplus },
  { "DDAYS", A_NONE, 0, f_ddays },
  { "DOW", A_NONE, 0, f_dow },
  { "DMY", A_NONE, 0, f_dmy },
  { "MDY", A_NONE, 0, f_mdy },
  { "CLK12", A_NONE, 0, f_clk12 },
  { "CLK24", A_NONE, 0, f_clk24 },
  { "CLOCK", A_NONE, 0, f_clock },
  { "CLKT", A_NONE, 0, f_clkt },
  { "CLKTD", A_NONE, 0, f_clktd },
  { "SETDATE", A_NONE, 0, f_setdate },
  { "SETIME", A_NONE, 0, f_setime },
  { "T+X", A_NONE, 0, f_tplusx },
  { "ADATE", A_NONE, 0, f_adate },
  { "ATIME", A_NONE, 0, f_atime },
  { "ATIME24", A_NONE, 0, f_atime24 },
  { "SW", A_NONE, 0, f_sw },
  { "SETSW", A_NONE, 0, f_setsw },
  { "RCLSW", A_NONE, 0, f_rclsw },
  { "RUNSW", A_NONE, 0, f_runsw },
  { "STOPSW", A_NONE, 0, f_stopsw },
  { "XYZALM", A_NONE, 0, f_xyzalm },
  { "ALMCAT", A_NONE, 0, f_almcat },
  { "ALMNOW", A_NONE, 0, f_almnow },
  { "CLRALMS", A_NONE, 0, f_clralms },
  { "RCLALM", A_NONE, 0, f_rclalm },
};
const int FN_COUNT = sizeof(FN) / sizeof(FN[0]);
