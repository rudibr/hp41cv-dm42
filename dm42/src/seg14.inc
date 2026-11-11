/* 14-segment font and renderer; needs blk(x,y,w,h) and abs() */
/* ================= 14-segment rendering ================= */
enum {
  SA = 1<<0, SB = 1<<1, SC = 1<<2, SD = 1<<3, SE = 1<<4, SF = 1<<5, SG1 = 1<<6, SG2 = 1<<7,
  SH = 1<<8, SI = 1<<9, SJ = 1<<10, SK = 1<<11, SL = 1<<12, SM = 1<<13
};
#define SG (SG1|SG2)

static uint16_t seg_of(uint8_t c) {
  if (c >= 'a' && c <= 'z' && c > 'e') c -= 32;
  switch (c) {
    case '0': return SA|SB|SC|SD|SE|SF;
    case '1': return SB|SC;
    case '2': return SA|SB|SG|SE|SD;
    case '3': return SA|SB|SG2|SC|SD;
    case '4': return SF|SG|SB|SC;
    case '5': return SA|SF|SG|SC|SD;
    case '6': return SA|SF|SE|SD|SC|SG;
    case '7': return SA|SB|SC;
    case '8': return SA|SB|SC|SD|SE|SF|SG;
    case '9': return SA|SB|SC|SD|SF|SG;
    case 'A': return SA|SB|SC|SE|SF|SG;
    case 'B': return SA|SB|SC|SD|SG2|SI|SL;
    case 'C': return SA|SD|SE|SF;
    case 'D': return SA|SB|SC|SD|SI|SL;
    case 'E': return SA|SD|SE|SF|SG1|SG2;
    case 'F': return SA|SE|SF|SG1;
    case 'G': return SA|SC|SD|SE|SF|SG2;
    case 'H': return SB|SC|SE|SF|SG;
    case 'I': return SA|SD|SI|SL;
    case 'J': return SB|SC|SD|SE;
    case 'K': return SE|SF|SG1|SJ|SM;
    case 'L': return SD|SE|SF;
    case 'M': return SB|SC|SE|SF|SH|SJ;
    case 'N': return SB|SC|SE|SF|SH|SM;
    case 'O': return SA|SB|SC|SD|SE|SF;
    case 'P': return SA|SB|SE|SF|SG;
    case 'Q': return SA|SB|SC|SD|SE|SF|SM;
    case 'R': return SA|SB|SE|SF|SG|SM;
    case 'S': return SA|SC|SD|SF|SG;
    case 'T': return SA|SI|SL;
    case 'U': return SB|SC|SD|SE|SF;
    case 'V': return SE|SF|SK|SJ;
    case 'W': return SB|SC|SE|SF|SK|SM;
    case 'X': return SH|SJ|SK|SM;
    case 'Y': return SH|SJ|SL;
    case 'Z': return SA|SD|SJ|SK;
    case 'a': return SD|SE|SG1|SL;
    case 'b': return SC|SD|SE|SF|SG;
    case 'c': return SD|SE|SG;
    case 'd': return SB|SC|SD|SE|SG;
    case 'e': return SD|SE|SG1|SK;
    case '-': return SG;
    case '+': return SG|SI|SL;
    case '*': return SG|SH|SI|SJ|SK|SL|SM;
    case '/': return SJ|SK;
    case '\\': return SH|SM;
    case '_': return SD;
    case '=': return SG|SD;
    case '<': return SJ|SM;
    case '>': return SH|SK;
    case '(': return SJ|SM;
    case ')': return SH|SK;
    case '[': return SA|SD|SE|SF;
    case ']': return SA|SB|SC|SD;
    case '?': return SA|SB|SG2|SL;
    case '!': return SI|SL;
    case '"': return SF|SI;
    case '\'': return SI;
    case '^': return SK|SM;
    case '%': return SF|SC|SG|SJ|SK;
    case '$': return SA|SF|SG|SC|SD|SI|SL;
    case '&': return SA|SH|SI|SG1|SE|SD|SM;
    case '@': return SA|SB|SD|SE|SF|SG2|SI;
    case '#': return SB|SC|SD|SG|SI|SL;
    case CH_TEE:    return SA|SI;
    case CH_APPEND: return SE|SF|SG;
    case CH_SIGMA:  return SA|SD|SH|SK;
    case CH_NE:     return SG|SD|SJ|SK;
    case CH_LE:     return SJ|SM|SD;
    case CH_ANGLE:  return SK|SD;
    case CH_GOOSE:  return SH|SK|SG2;
  }
  return 0;
}


/* ---- polygon segments, HP-41 style: thin strokes with small gaps ---- */
typedef struct { float x, y; } pt_t;

static void fill_poly(const pt_t *p, int n, int ox, int oy) {
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

static void thick(float x0, float y0, float x1, float y1, float t, int ox, int oy) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  float nx = -dy / l * t / 2, ny = dx / l * t / 2;
  pt_t p[4] = { { x0 + nx, y0 + ny }, { x1 + nx, y1 + ny }, { x1 - nx, y1 - ny }, { x0 - nx, y0 - ny } };
  fill_poly(p, 4, ox, oy);
}

#ifndef CW
#define CW 24    /* char width  */
#define CH 40    /* char height */
#define CPITCH 32
#endif

static void draw_char(int x, int y, uint8_t c, uint8_t p) {
  uint16_t s = seg_of(c);
  const float W = CW, H = CH, t = 3.2f, g = 1.2f, h2 = H / 2, m = W / 2;
  if (s & SA) { pt_t q[4] = { { t/2+g, 0 }, { W-t/2-g, 0 }, { W-t-g, t }, { t+g, t } }; fill_poly(q, 4, x, y); }
  if (s & SD) { pt_t q[4] = { { t+g, H-t }, { W-t-g, H-t }, { W-t/2-g, H }, { t/2+g, H } }; fill_poly(q, 4, x, y); }
  if (s & SG1) { pt_t q[6] = { { t/2+g, h2 }, { t+g, h2-t/2 }, { m-g, h2-t/2 }, { m-g+0.01f, h2+t/2 }, { t+g, h2+t/2 }, { t/2+g, h2 } }; fill_poly(q, 5, x, y); }
  if (s & SG2) { pt_t q[5] = { { m+g, h2-t/2 }, { W-t-g, h2-t/2 }, { W-t/2-g, h2 }, { W-t-g, h2+t/2 }, { m+g, h2+t/2 } }; fill_poly(q, 5, x, y); }
  if (s & SF) { pt_t q[5] = { { 0, t/2+g }, { t, t+g }, { t, h2-t/2-g }, { t/2, h2-g }, { 0, h2-t/2-g } }; fill_poly(q, 5, x, y); }
  if (s & SB) { pt_t q[5] = { { W, t/2+g }, { W-t, t+g }, { W-t, h2-t/2-g }, { W-t/2, h2-g }, { W, h2-t/2-g } }; fill_poly(q, 5, x, y); }
  if (s & SE) { pt_t q[5] = { { 0, h2+t/2+g }, { t/2, h2+g }, { t, h2+t/2+g }, { t, H-t-g }, { 0, H-t/2-g } }; fill_poly(q, 5, x, y); }
  if (s & SC) { pt_t q[5] = { { W, h2+t/2+g }, { W-t/2, h2+g }, { W-t, h2+t/2+g }, { W-t, H-t-g }, { W, H-t/2-g } }; fill_poly(q, 5, x, y); }
  if (s & SI) { pt_t q[4] = { { m-t/2, t+g }, { m+t/2, t+g }, { m+t/2, h2-t/2-g }, { m-t/2, h2-t/2-g } }; fill_poly(q, 4, x, y); }
  if (s & SL) { pt_t q[4] = { { m-t/2, h2+t/2+g }, { m+t/2, h2+t/2+g }, { m+t/2, H-t-g }, { m-t/2, H-t-g } }; fill_poly(q, 4, x, y); }
  const float dt = 3.2f, e = 1.0f;
  if (s & SH) thick(t+e+0.8f, t+e+1.5f, m-t/2-e, h2-t/2-e-0.5f, dt, x, y);
  if (s & SJ) thick(W-t-e-0.8f, t+e+1.5f, m+t/2+e, h2-t/2-e-0.5f, dt, x, y);
  if (s & SK) thick(m-t/2-e, h2+t/2+e+0.5f, t+e+0.8f, H-t-e-1.5f, dt, x, y);
  if (s & SM) thick(m+t/2+e, h2+t/2+e+0.5f, W-t-e-0.8f, H-t-e-1.5f, dt, x, y);
  if (p == '.' || p == ',') { pt_t q[4] = { { W+3, H-3.4f }, { W+6.4f, H-3.4f }, { W+6.4f, H }, { W+3, H } }; fill_poly(q, 4, x, y); }
  if (p == ',') thick(W+5.6f, H, W+3.2f, H+5, 2.2f, x, y);
  if (p == ':') {
    pt_t q1[4] = { { W+3, H*0.3f }, { W+6.4f, H*0.3f }, { W+6.4f, H*0.3f+3.4f }, { W+3, H*0.3f+3.4f } }; fill_poly(q1, 4, x, y);
    pt_t q2[4] = { { W+3, H*0.65f }, { W+6.4f, H*0.65f }, { W+6.4f, H*0.65f+3.4f }, { W+3, H*0.65f+3.4f } }; fill_poly(q2, 4, x, y);
  }
}

