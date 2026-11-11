/* 14-segment font and renderer; needs blk(x,y,w,h) and abs() */
/* ================= 14-segment rendering ================= */
enum {
  SA = 1<<0, SB = 1<<1, SC = 1<<2, SD = 1<<3, SE = 1<<4, SF = 1<<5, SG1 = 1<<6, SG2 = 1<<7,
  SH = 1<<8, SI = 1<<9, SJ = 1<<10, SK = 1<<11, SL = 1<<12, SM = 1<<13
};
#define SG (SG1|SG2)

/* Segment patterns follow the HP-41 LCD character table (codes 00-4F). */
static uint16_t seg_of(uint8_t c) {
  switch (c) {
    case '@': return SA|SB|SF|SE|SD|SI|SG2;
    case 'A': return SA|SB|SC|SE|SF|SG;
    case 'B': return SA|SB|SC|SD|SI|SL|SG2;
    case 'C': return SA|SF|SE|SD;
    case 'D': return SA|SB|SC|SD|SI|SL;
    case 'E': return SA|SF|SE|SD|SG;
    case 'F': return SA|SF|SE|SG;
    case 'G': return SA|SF|SE|SD|SC|SG2;
    case 'H': return SF|SE|SB|SC|SG;
    case 'I': return SA|SD|SI|SL;
    case 'J': return SB|SC|SD|SE;
    case 'K': return SF|SE|SG1|SJ|SM;
    case 'L': return SF|SE|SD;
    case 'M': return SF|SE|SB|SC|SH|SJ;
    case 'N': return SF|SE|SB|SC|SH|SM;
    case 'O': return SA|SB|SC|SD|SE|SF;
    case 'P': return SA|SB|SF|SE|SG;
    case 'Q': return SA|SB|SC|SD|SE|SF|SM;
    case 'R': return SA|SB|SF|SE|SG|SM;
    case 'S': return SA|SF|SG|SC|SD;
    case 'T': return SA|SI|SL;
    case 'U': return SF|SE|SB|SC|SD;
    case 'V': return SF|SE|SK|SJ;
    case 'W': return SF|SE|SB|SC|SK|SM;
    case 'X': return SH|SJ|SK|SM;
    case 'Y': return SH|SJ|SL;
    case 'Z': return SA|SD|SJ|SK;
    case '[': return SA|SF|SE|SD;
    case '\\': return SH|SM;
    case ']': return SA|SB|SC|SD;
    case '_': return SD;
    case '!': return SI|SL;
    case '"': return SF|SI;
    case '#': return SI|SL|SG|SB|SC|SD;
    case '$': return SA|SF|SI|SL|SG|SC|SD;
    case '%': return SF|SH|SJ|SG|SK|SM|SC;
    case '&': return SA|SH|SI|SK|SM|SD|SC;
    case '\'': return SI;
    case '(': return SJ|SM;
    case ')': return SH|SK;
    case '*': return SH|SI|SJ|SG|SK|SL|SM;
    case '+': return SI|SL|SG;
    case ',': return SG1|SJ|SM;
    case '-': return SG;
    case '.': return SH|SK|SG2;
    case '/': return SJ|SK;
    case '0': return SA|SB|SC|SD|SE|SF|SJ|SK;
    case '1': return SB|SC;
    case '2': return SA|SB|SG|SE|SD;
    case '3': return SA|SB|SG|SC|SD;
    case '4': return SF|SG|SB|SC;
    case '5': return SA|SH|SG2|SC|SD;
    case '6': return SA|SF|SE|SD|SC|SG;
    case '7': return SA|SB|SC;
    case '8': return SA|SB|SC|SD|SE|SF|SG;
    case '9': return SA|SB|SC|SD|SF|SG;
    case ':': return SI|SL;
    case ';': return SG1|SK;
    case '<': return SJ|SK|SD;
    case '=': return SG|SD;
    case '>': return SH|SM|SD;
    case '?': return SA|SB|SF|SG2|SL;
    case 'a': return SG1|SE|SD|SC|SM;
    case 'b': return SF|SE|SD|SC|SG;
    case 'c': return SG|SE|SD;
    case 'd': return SB|SC|SD|SE|SG;
    case 'e': return SG1|SE|SD|SK;
    case '^': return SA|SB|SJ|SK;          /* 41 up arrow */
    case CH_APPEND: return SF|SE|SG;
    case CH_TEE:    return SA|SI;
    case CH_SIGMA:  return SA|SH|SK|SD;
    case CH_NE:     return SG|SD|SJ|SK;
    case CH_LE:     return SJ|SK|SD;
    case CH_ANGLE:  return SJ|SK|SL|SD;
    case CH_GOOSE:  return SH|SK|SG2;
  }
  if (c >= 'f' && c <= 'z') return seg_of((uint8_t)(c - 32));
  return 0;
}

/* ---- polygon segments, HP-41 style: thin strokes with small gaps ---- */
typedef struct { float x, y; } pt_t;

static float seg_slant;   /* x shift per pixel of height, for the 41's italic lean */
static float seg_h;

static void fill_poly(const pt_t *p0, int n, int ox, int oy) {
  pt_t p[8];
  for (int i = 0; i < n; i++) { p[i].x = p0[i].x + (seg_h - p0[i].y) * seg_slant; p[i].y = p0[i].y; }
  float ymin = p[0].y, ymax = p[0].y;
  for (int i = 1; i < n; i++) { if (p[i].y < ymin) ymin = p[i].y; if (p[i].y > ymax) ymax = p[i].y; }
  for (int y = (int)ymin; y <= (int)ymax + 1; y++) {
    float yc = y + 0.5f, xs[8]; int k = 0;
    for (int i = 0; i < n && k < 8; i++) {
      pt_t a = p[i], b = p[(i + 1) % n];
      if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc))
        xs[k++] = a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y);
    }
    for (int i = 1; i < k; i++) for (int j = i; j > 0 && xs[j-1] > xs[j]; j--) { float t = xs[j]; xs[j] = xs[j-1]; xs[j-1] = t; }
    for (int i = 0; i + 1 < k; i += 2) {
      int x0 = (int)(xs[i] + 0.5f), x1 = (int)(xs[i+1] + 0.5f);
      if (x1 > x0) blk(ox + x0, oy + y, x1 - x0, 1);
    }
  }
}

static void taper(float x0, float y0, float x1, float y1, float t0, float t1, int ox, int oy) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  float nx = -dy / l, ny = dx / l;
  pt_t p[4] = { { x0 + nx * t0 / 2, y0 + ny * t0 / 2 }, { x1 + nx * t1 / 2, y1 + ny * t1 / 2 },
                { x1 - nx * t1 / 2, y1 - ny * t1 / 2 }, { x0 - nx * t0 / 2, y0 - ny * t0 / 2 } };
  fill_poly(p, 4, ox, oy);
}

#ifndef CW
#define CW 21    /* char width  */
#define CH 36    /* char height */
#define CPITCH 29
#endif

static void draw_char(int x, int y, uint8_t c, uint8_t p) {
  uint16_t s = seg_of(c);
  const float W = CW, H = CH, t = 3.0f, h2 = H / 2, m = W / 2, g = 1.0f;
  seg_slant = 0.11f; seg_h = H;
  x -= 2;    /* the slant moves the top right; keep the cell centred */
  /* top and bottom bars: trapezoids; the verticals start just below them */
  if (s & SA) { pt_t q[4] = { { 1.2f, 0 }, { W - 1.2f, 0 }, { W - t - 0.6f, t }, { t + 0.6f, t } }; fill_poly(q, 4, x, y); }
  if (s & SD) { pt_t q[4] = { { t + 0.6f, H - t }, { W - t - 0.6f, H - t }, { W - 1.2f, H }, { 1.2f, H } }; fill_poly(q, 4, x, y); }
  /* middle bars: two lozenges with a gap in the middle and at the sides */
  if (s & SG1) { pt_t q[6] = { { t + 0.2f, h2 }, { t + 1.3f, h2 - 1.8f }, { m - 1.4f, h2 - 1.8f }, { m - 0.3f, h2 }, { m - 1.4f, h2 + 1.8f }, { t + 1.3f, h2 + 1.8f } }; fill_poly(q, 6, x, y); }
  if (s & SG2) { pt_t q[6] = { { m + 0.3f, h2 }, { m + 1.4f, h2 - 1.8f }, { W - t - 1.3f, h2 - 1.8f }, { W - t - 0.2f, h2 }, { W - t - 1.3f, h2 + 1.8f }, { m + 1.4f, h2 + 1.8f } }; fill_poly(q, 6, x, y); }
  /* outer verticals: pointed at both ends, with gaps to the bars */
  const float vt = t + 1.0f, vb = h2 - 1.2f, vt2 = h2 + 1.2f, vb2 = H - t - 1.0f;
  if (s & SF) { pt_t q[5] = { { 0, vt + 1.2f }, { t * 0.6f, vt - 0.3f }, { t, vt + 1.6f }, { t, vb - 1.0f }, { 0, vb } }; fill_poly(q, 5, x, y); }
  if (s & SB) { pt_t q[5] = { { W, vt + 1.2f }, { W - t * 0.6f, vt - 0.3f }, { W - t, vt + 1.6f }, { W - t, vb - 1.0f }, { W, vb } }; fill_poly(q, 5, x, y); }
  if (s & SE) { pt_t q[5] = { { 0, vt2 }, { t, vt2 + 1.0f }, { t, vb2 - 1.6f }, { t * 0.6f, vb2 + 0.3f }, { 0, vb2 - 1.2f } }; fill_poly(q, 5, x, y); }
  if (s & SC) { pt_t q[5] = { { W, vt2 }, { W - t, vt2 + 1.0f }, { W - t, vb2 - 1.6f }, { W - t * 0.6f, vb2 + 0.3f }, { W, vb2 - 1.2f } }; fill_poly(q, 5, x, y); }
  /* centre verticals: wide at the outer end, pointed towards the middle */
  if (s & SI) { pt_t q[4] = { { m - t / 2 - 0.4f, t + g }, { m + t / 2 + 0.4f, t + g }, { m + 0.4f, h2 - g - 1 }, { m - 0.4f, h2 - g - 1 } }; fill_poly(q, 4, x, y); }
  if (s & SL) { pt_t q[4] = { { m - 0.4f, h2 + g + 1 }, { m + 0.4f, h2 + g + 1 }, { m + t / 2 + 0.4f, H - t - g }, { m - t / 2 - 0.4f, H - t - g } }; fill_poly(q, 4, x, y); }
  /* diagonals: leaf shaped, thin at the centre */
  const float e = 1.3f;
  if (s & SH) taper(t + e, t + e + 1, m - 1.2f, h2 - 2.0f, 3.8f, 1.4f, x, y);
  if (s & SJ) taper(W - t - e, t + e + 1, m + 1.2f, h2 - 2.0f, 3.8f, 1.4f, x, y);
  if (s & SK) taper(t + e, H - t - e - 1, m - 1.2f, h2 + 2.0f, 3.8f, 1.4f, x, y);
  if (s & SM) taper(W - t - e, H - t - e - 1, m + 1.2f, h2 + 2.0f, 3.8f, 1.4f, x, y);
  /* punctuation sits upright to the right of the character */
  seg_slant = 0;
  if (p == '.' || p == ',') { pt_t q[4] = { { W + 2.5f, H - 3.2f }, { W + 5.7f, H - 3.2f }, { W + 5.7f, H }, { W + 2.5f, H } }; fill_poly(q, 4, x, y); }
  if (p == ',') taper(W + 5.2f, H + 0.5f, W + 2.6f, H + 5, 2.4f, 1.4f, x, y);
  if (p == ':') {
    pt_t q1[4] = { { W + 3, H * 0.3f }, { W + 6, H * 0.3f }, { W + 6, H * 0.3f + 3 }, { W + 3, H * 0.3f + 3 } }; fill_poly(q1, 4, x, y);
    pt_t q2[4] = { { W + 3, H * 0.65f }, { W + 6, H * 0.65f }, { W + 6, H * 0.65f + 3 }, { W + 3, H * 0.65f + 3 } }; fill_poly(q2, 4, x, y);
  }
}

