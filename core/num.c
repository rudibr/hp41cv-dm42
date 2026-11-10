/* Number rounding, formatting and display cell layout */
#include <math.h>
#include "core.h"

static const double P10[23] = {
  1e0,1e1,1e2,1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11,1e12,1e13,1e14,1e15,
  1e16,1e17,1e18,1e19,1e20,1e21,1e22
};

static double scale10(double x, int k) {
  /* x * 10^k with exact powers where possible */
  while (k > 22) { x *= 1e22; k -= 22; }
  while (k < -22) { x /= 1e22; k += 22; }
  if (k >= 0) return x * P10[k];
  return x / P10[-k];
}

static const int64_t I10[] = {
  1LL,10LL,100LL,1000LL,10000LL,100000LL,1000000LL,10000000LL,100000000LL,
  1000000000LL,10000000000LL,100000000000LL,1000000000000LL
};

/* x != 0: |x| ~= m * 10^(e-nd+1), m has exactly nd digits (rounded half up) */
void num_decompose(double x, int nd, int64_t *pm, int *pe) {
  double ax = fabs(x);
  int e = (int)floor(log10(ax));
  for (int iter = 0; iter < 4; iter++) {
    double y = scale10(ax, nd - 1 - e);
    int64_t m = (int64_t)floor(y + 0.5);
    if (m >= I10[nd]) { e++; continue; }
    if (m < I10[nd-1]) {
      /* may be a rounding artefact of log10 */
      double y2 = scale10(ax, nd - e);
      int64_t m2 = (int64_t)floor(y2 + 0.5);
      if (m2 < I10[nd]) { e--; continue; }
      m = I10[nd-1];
    }
    *pm = m; *pe = e;
    return;
  }
  *pm = I10[nd-1]; *pe = e;
}

double num_round(double x) {
  if (x != x) { raise_err(E_DATA); return 0; }
  if (x == 0) return 0;
  if (isinf(x) || fabs(x) >= 9.9999999995e99) {
    if (FL_GET(FLG_RANGE)) return x < 0 ? -9.999999999e99 : 9.999999999e99;
    raise_err(E_RANGE);
    return x < 0 ? -9.999999999e99 : 9.999999999e99;
  }
  int64_t m; int e;
  num_decompose(x, 10, &m, &e);
  if (e > 99) {
    if (FL_GET(FLG_RANGE)) return x < 0 ? -9.999999999e99 : 9.999999999e99;
    raise_err(E_RANGE);
    return x < 0 ? -9.999999999e99 : 9.999999999e99;
  }
  if (e < -99) return 0;
  double r = scale10((double)m, e - 9);
  return x < 0 ? -r : r;
}

static uint8_t radix_char(void) { return FL_GET(FLG_RADIX) ? '.' : ','; }
static uint8_t sep_char(void)   { return FL_GET(FLG_RADIX) ? ',' : '.'; }

static int disp_digits(void) {
  return (FL_GET(36) << 3) | (FL_GET(37) << 2) | (FL_GET(38) << 1) | FL_GET(39);
}

/* write integer digits of q (k digits, zero padded to at least minlen) */
static int put_digits(uint8_t *o, int64_t q, int len) {
  for (int i = len - 1; i >= 0; i--) { o[i] = (uint8_t)('0' + q % 10); q /= 10; }
  return len;
}

/* mantissa with nd significant digits in SCI/ENG style, intd integer digits */
static int fmt_exp(double x, int nd, int eng, uint8_t *out) {
  int n = 0;
  int64_t m; int e;
  if (nd > 8) nd = 8;
  if (nd < 1) nd = 1;
  if (x == 0) { m = 0; e = 0; }
  else num_decompose(x, nd, &m, &e);
  int intd = 1;
  if (eng && x != 0) {
    int ee = e >= 0 ? (e / 3) * 3 : -(((-e) + 2) / 3) * 3;
    intd = e - ee + 1;
    e = ee;
  }
  if (x < 0) out[n++] = '-';
  uint8_t dig[16];
  int dl = nd;
  if (intd > dl) dl = intd;       /* ENG with few digits: pad zeros */
  int64_t mm = m;
  for (int i = nd; i < dl; i++) mm *= 10;
  put_digits(dig, mm, dl);
  for (int i = 0; i < dl; i++) {
    out[n++] = dig[i];
    if (i == intd - 1 && (dl > intd || 1)) out[n++] = radix_char();
  }
  out[n++] = e < 0 ? '-' : ' ';
  int ae = e < 0 ? -e : e;
  out[n++] = (uint8_t)('0' + ae / 10);
  out[n++] = (uint8_t)('0' + ae % 10);
  return n;
}

static int group_int(const uint8_t *ip, int il, uint8_t *o) {
  int n = 0;
  for (int i = 0; i < il; i++) {
    o[n++] = ip[i];
    int rem = il - 1 - i;
    if (FL_GET(FLG_GROUP) && rem > 0 && rem % 3 == 0) o[n++] = sep_char();
  }
  return n;
}

int num_format(double x, uint8_t *out) {
  int nd = disp_digits();
  int fix = FL_GET(FLG_FIX), eng = FL_GET(FLG_ENG);
  if (!fix) return fmt_exp(x, nd + 1, eng, out);
  /* FIX */
  int n = 0;
  if (x == 0) {
    out[n++] = '0';
    if (nd > 0 || FL_GET(FLG_GROUP)) out[n++] = radix_char();
    for (int i = 0; i < nd && i < 9; i++) out[n++] = '0';
    return n;
  }
  int64_t m; int e;
  num_decompose(x, 10, &m, &e);
  if (e >= 10) return fmt_exp(x, nd + 1, 0, out);
  int d = nd;
  int maxd = e >= 0 ? 9 - e : 9;
  if (d > maxd) d = maxd;
  int k = e + 1 + d;           /* significant digits kept */
  int64_t q;
  if (k >= 10) q = m * I10[k - 10];
  else if (k <= 0) q = (k == 0 && m >= 5000000000LL) ? 1 : 0;
  else {
    q = m / I10[10 - k];
    if (m % I10[10 - k] >= I10[10 - k] / 2) q++;
  }
  if (k >= 0 && q >= I10[k]) {   /* carry: value grew by a decade */
    e++;
    if (e >= 10) return fmt_exp(x, nd + 1, 0, out);
    d = nd; maxd = e >= 0 ? 9 - e : 9;
    if (d > maxd) d = maxd;
    k = e + 1 + d;
    q = I10[k - 1];
  }
  if (q == 0) return fmt_exp(x, nd + 1, 0, out);   /* would display as zero */
  uint8_t dig[24];
  int tot = k > d + 1 ? k : d + 1;
  put_digits(dig, q, tot);
  if (x < 0) out[n++] = '-';
  n += group_int(dig, tot - d, out + n);
  if (d > 0 || FL_GET(FLG_GROUP)) out[n++] = radix_char();
  for (int i = tot - d; i < tot; i++) out[n++] = dig[i];
  return n;
}

/* all ten digits, used by SHOW */
int num_format_full(double x, uint8_t *out) {
  int n = 0;
  if (x == 0) { memcpy(out, "0000000000 00", 13); return 13; }
  int64_t m; int e;
  num_decompose(x, 10, &m, &e);
  if (x < 0) out[n++] = '-';
  n += put_digits(out + n, m, 10);
  out[n++] = e < 0 ? '-' : ' ';
  int ae = e < 0 ? -e : e;
  out[n++] = (uint8_t)('0' + ae / 10);
  out[n++] = (uint8_t)('0' + ae % 10);
  return n;
}

int val_format(const val_t *v, uint8_t *out) {
  if (v->isalpha) { memcpy(out, v->s, v->len); return v->len; }
  return num_format(v->x, out);
}

/* parse "[-]ddd.ddd[E[-]dd]" */
int num_parse(const char *s, double *res) {
  int neg = 0, eneg = 0, ex = 0, fd = 0, sig = 0, any = 0, dot = 0;
  int64_t m = 0;
  int extra = 0;
  while (*s == ' ') s++;
  if (*s == '-' || *s == '+') { neg = (*s == '-'); s++; }
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9') {
      any = 1;
      if (sig < 15) {
        if (m || *s != '0') sig++;
        m = m * 10 + (*s - '0');
        if (dot) fd++;
      } else if (!dot) extra++;
    } else if (*s == '.' && !dot) dot = 1;
    else break;
  }
  if (*s == 'E' || *s == 'e') {
    s++;
    if (!any) { m = 1; any = 1; }
    if (*s == '-' || *s == '+') { eneg = (*s == '-'); s++; }
    while (*s >= '0' && *s <= '9') { ex = ex * 10 + (*s - '0'); s++; if (ex > 999) ex = 999; }
  }
  while (*s == ' ') s++;
  if (!any || *s) return 0;
  if (eneg) ex = -ex;
  double v = scale10((double)m, ex - fd + extra);
  *res = num_round(neg ? -v : v);
  return 1;
}

/* ---- text -> 14 segment cells ---- */
static int is_punct(uint8_t c) { return c == '.' || c == ',' || c == ':'; }

int cells_len(const uint8_t *s, int len) {
  int n = 0, lastp = 1;
  for (int i = 0; i < len; i++) {
    if (is_punct(s[i]) && !lastp) { lastp = 1; continue; }
    n++;
    lastp = is_punct(s[i]);
  }
  return n;
}

/* Lay text out into cells. start = first cell to show (for scrolling);
   right_align: show the last 12 cells */
void cells_from_text(const uint8_t *s, int len, hp41_disp_t *d, int start) {
  uint8_t ch[80], pu[80];
  int n = 0, lastp = 1;
  for (int i = 0; i < len && n < 80; i++) {
    uint8_t c = s[i];
    if (is_punct(c) && !lastp && n > 0) { pu[n-1] = c; lastp = 1; continue; }
    if (is_punct(c)) { ch[n] = ' '; pu[n] = c; n++; lastp = 1; continue; }
    ch[n] = c; pu[n] = 0; n++; lastp = 0;
  }
  if (start < 0) start = n > HP41_CELLS ? n - HP41_CELLS : 0;   /* right aligned window */
  for (int i = 0; i < HP41_CELLS; i++) {
    int j = start + i;
    if (j < n) { d->ch[i] = ch[j]; d->punct[i] = pu[j]; }
    else { d->ch[i] = ' '; d->punct[i] = 0; }
  }
}
