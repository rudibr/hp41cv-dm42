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


static void seg_line(int x0, int y0, int x1, int y1, int t) {
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int e = dx - dy;
  for (;;) {
    blk(x0, y0, t, t);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 > -dy) { e -= dy; x0 += sx; }
    if (e2 < dx) { e += dx; y0 += sy; }
  }
}

#define CW 24   /* char width  */
#define CH 46   /* char height */
#define CT 4    /* stroke */
#define CPITCH 32

static void draw_char(int x, int y, uint8_t c, uint8_t p) {
  uint16_t s = seg_of(c);
  const int W = CW, H = CH, t = CT, h2 = H / 2;
  if (s & SA)  blk(x + 2, y, W - 4, t);
  if (s & SD)  blk(x + 2, y + H - t, W - 4, t);
  if (s & SG1) blk(x + 2, y + h2 - t / 2, W / 2 - 2, t);
  if (s & SG2) blk(x + W / 2, y + h2 - t / 2, W / 2 - 2, t);
  if (s & SF)  blk(x, y + 2, t, h2 - 3);
  if (s & SB)  blk(x + W - t, y + 2, t, h2 - 3);
  if (s & SE)  blk(x, y + h2 + 1, t, h2 - 3);
  if (s & SC)  blk(x + W - t, y + h2 + 1, t, h2 - 3);
  if (s & SI)  blk(x + W / 2 - t / 2, y + t + 1, t, h2 - t - 3);
  if (s & SL)  blk(x + W / 2 - t / 2, y + h2 + t / 2 + 1, t, h2 - t - 3);
  if (s & SH)  seg_line(x + t, y + t + 1, x + W / 2 - 3, y + h2 - 4, 3);
  if (s & SJ)  seg_line(x + W - t - 3, y + t + 1, x + W / 2, y + h2 - 4, 3);
  if (s & SK)  seg_line(x + W / 2 - 3, y + h2 + 2, x + t, y + H - t - 4, 3);
  if (s & SM)  seg_line(x + W / 2, y + h2 + 2, x + W - t - 3, y + H - t - 4, 3);
  if (p == '.' || p == ',') blk(x + W + 3, y + H - t, t, t);
  if (p == ',') seg_line(x + W + 5, y + H, x + W + 2, y + H + 5, 2);
  if (p == ':') { blk(x + W + 3, y + H / 3, t, t); blk(x + W + 3, y + 2 * H / 3, t, t); }
}

