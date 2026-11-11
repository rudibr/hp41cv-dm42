/* Keyboard, prompts, menus and display composition.
 * Keys follow the labels printed on the DM42 (42S layout); 41CV functions that
 * have no key of their own live in softkey menus under the matching 42S label.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "core.h"

state_t *S;
const hp41_platform_t *PLAT;
int msg_active;

void exec_step(void);
void clp_name(const uint8_t *name, int len);
int arg_text(int argt, int a, uint8_t *o);

static uint32_t now_ms(void) { return PLAT && PLAT->ms ? PLAT->ms() : 0; }

/* ======================= UI state ======================= */
enum { PEND_NONE, PEND_FN, PEND_SST, PEND_BST, PEND_SHOW, PEND_ASN, PEND_LBL, PEND_CMPL, PEND_ACMPL };

static struct {
  int shift;
  int menu, page;
  /* number entry */
  int ent; char em[16]; int eml; int eneg; int eexp; char ee[4]; int eel; int eeneg;
  /* alpha entry */
  int aent;                 /* run mode: next char appends */
  int tact; uint8_t tbuf[24]; int tlen;  /* PRGM mode text line */
  /* prompt */
  int pr, pr_op, pr_argt, pr_ind, pr_stk, pr_ndig, pr_need, pr_val;
  int pr_alpha; uint8_t pr_name[16]; int pr_nlen;
  int pr_dot;               /* GTO . */
  int pr_dotdig;            /* digits after GTO . */
  int pr_asn;               /* 1 = ASN name entry, 2 = waiting for key */
  int pr_clp;
  /* message */
  uint8_t msg[48]; int msglen;
  /* pending key (executed on release) */
  int pend; int pend_op, pend_arg; uint32_t pend_t; int pend_null;
  uint8_t pend_txt[40]; int pend_len;
  uint8_t pend_str[16]; int pend_slen;
  /* pause */
  int paused; uint32_t pause_until;
  /* catalog */
  int cat, cat_type, cat_idx, cat_run; uint32_t cat_t;
  int off_req;
  int clock, swd; uint32_t alm_t;
  uint32_t scroll_t; int scrolling;
} U;

/* ======================= messages ======================= */
void show_message(const uint8_t *s, int len) {
  if (len > (int)sizeof U.msg) len = sizeof U.msg;
  memcpy(U.msg, s, len);
  U.msglen = len;
  msg_active = 1;
  FL_SET(FLG_MSG);
  U.scroll_t = now_ms();
}
void show_message_str(const char *s) { show_message((const uint8_t *)s, (int)strlen(s)); }
void show_view(const val_t *v) {
  uint8_t b[32];
  int n = val_format(v, b);
  show_message(b, n);
}
static const char *err_text(int e) {
  switch (e) {
    case E_DATA: return "DATA ERROR";
    case E_RANGE: return "OUT OF RANGE";
    case E_NONEX: return "NONEXISTENT";
    case E_ALPHA: return "ALPHA DATA";
    case E_NOROOM: return "NO ROOM";
    case E_TRYAGAIN: return "TRY AGAIN";
  }
  return "ERROR";
}
void ui_after_error(void) {
  if (err) show_message_str(err_text(err));
  err = 0;
}
void ui_pause(int ms) {
  if (!running) return;
  U.paused = 1;
  U.pause_until = now_ms() + (uint32_t)ms;
}
void ui_beep(int f, int ms) { if (PLAT && PLAT->beep) PLAT->beep(f, ms); }
void ui_off(void) { stop_running(); U.off_req = 1; if (PLAT && PLAT->power_off) PLAT->power_off(); }
void ui_show_x(void) { msg_active = 0; FL_CLR(FLG_MSG); }

/* ======================= alpha register ======================= */
void alpha_clear(void) { S->alen = 0; }
void alpha_append(const uint8_t *s, int len) {
  for (int i = 0; i < len; i++) {
    if (S->alen == ALPHA_MAX) {
      memmove(S->alpha, S->alpha + 1, ALPHA_MAX - 1);
      S->alen--;
    }
    S->alpha[S->alen++] = s[i];
  }
}

/* ======================= instruction encoding ======================= */
static int encode(int op, int arg, const uint8_t *str, int slen, uint8_t *o) {
  if (op >= OP_NUM) {
    if (op == OP_END) { o[0] = OP_END; return 1; }
    o[0] = (uint8_t)op; o[1] = (uint8_t)slen; memcpy(o + 2, str, slen);
    return 2 + slen;
  }
  if (op == O_END) { o[0] = OP_END; return 1; }
  o[0] = (uint8_t)op;
  if (FN[op].argt != A_NONE) { o[1] = (uint8_t)arg; return 2; }
  return 1;
}

static int prgm_mode(void) { return FL_GET(FLG_PRGM) && !running; }

static int programmable(int op) {
  if (op >= OP_NUM) return 1;
  return !(FN[op].flags & F_NP);
}

/* execute from the keyboard or record in PRGM mode */
static void do_instr(int op, int arg, const uint8_t *str, int slen) {
  err = 0;
  if (prgm_mode() && programmable(op)) {
    uint8_t code[24];
    int n = encode(op, arg, str, slen, code);
    if (!insert_instr(code, n)) ui_after_error();
    return;
  }
  if (op == O_END && !running) return;   /* END from keyboard: nothing */
  exec_op(op, arg, str, slen);
  if (err) { stop_running(); ui_after_error(); }
}

/* ======================= number entry ======================= */
static void entry_text(uint8_t *o, int *n, int cursor);

static double entry_value(void) {
  char t[32]; int n = 0;
  if (U.eneg) t[n++] = '-';
  for (int i = 0; i < U.eml; i++) t[n++] = U.em[i];
  if (U.eml == 0) t[n++] = '1';
  if (U.eexp) {
    t[n++] = 'E';
    if (U.eeneg) t[n++] = '-';
    for (int i = 0; i < U.eel; i++) t[n++] = U.ee[i];
    if (U.eel == 0) t[n++] = '0';
  }
  t[n] = 0;
  double v = 0;
  int saved = err;
  num_parse(t, &v);
  if (err && !saved) { err = 0; v = U.eneg ? -9.999999999e99 : 9.999999999e99; }
  return v;
}

static void entry_update_x(void) {
  if (prgm_mode()) return;
  val_t v; memset(&v, 0, sizeof v);
  v.x = entry_value();
  S->stk[SX] = v;
}

static void entry_start(void) {
  U.ent = 1; U.eml = 0; U.eneg = 0; U.eexp = 0; U.eel = 0; U.eeneg = 0;
  if (!prgm_mode()) {
    if (S->lift) lift_stack();
    S->lift = 1;
  }
}

static int ent_digits(void) { int d = 0; for (int i = 0; i < U.eml; i++) if (U.em[i] != '.') d++; return d; }

static void entry_key_digit(char c) {
  if (!U.ent) entry_start();
  if (U.eexp) {
    if (U.eel == 2) { U.ee[0] = U.ee[1]; U.eel = 1; }
    U.ee[U.eel++] = c;
  } else {
    if (c == '.') {
      for (int i = 0; i < U.eml; i++) if (U.em[i] == '.') return;
      if (U.eml == 0) U.em[U.eml++] = '0';
      U.em[U.eml++] = '.';
    } else {
      if (ent_digits() >= 10) return;
      if (U.eml == 1 && U.em[0] == '0') U.eml = 0;
      U.em[U.eml++] = c;
    }
  }
  entry_update_x();
}

static void entry_eex(void) {
  if (!U.ent) { entry_start(); U.em[0] = '1'; U.eml = 1; }
  if (U.eexp) return;
  if (ent_digits() > 8) { /* 41 allows EEX up to 8 mantissa digits */ }
  U.eexp = 1;
  entry_update_x();
}

/* finish number entry: in PRGM mode insert the number line */
static void entry_finish(void) {
  if (!U.ent) return;
  U.ent = 0;
  if (prgm_mode()) {
    char t[32]; int n = 0;
    if (U.eneg) t[n++] = '-';
    for (int i = 0; i < U.eml; i++) t[n++] = U.em[i];
    if (U.eml == 0) t[n++] = '1';
    if (U.eexp) {
      t[n++] = 'E';
      if (U.eeneg) t[n++] = '-';
      for (int i = 0; i < U.eel; i++) t[n++] = U.ee[i];
    }
    uint8_t code[40];
    code[0] = OP_NUM; code[1] = (uint8_t)n; memcpy(code + 2, t, n);
    err = 0;
    if (!insert_instr(code, n + 2)) ui_after_error();
  } else {
    S->lift = 1;
  }
}

static void entry_bsp(void) {
  if (U.eexp) {
    if (U.eel > 0) U.eel--;
    else if (U.eeneg) U.eeneg = 0;
    else U.eexp = 0;
  } else if (U.eml > 0) {
    U.eml--;
    if (U.eml == 1 && U.em[0] == '0' && 0) U.eml = 0;
  }
  if (!U.eexp && U.eml == 0) {
    /* entry cleared */
    U.ent = 0;
    if (!prgm_mode()) { S->stk[SX].x = 0; S->stk[SX].isalpha = 0; S->lift = 0; }
    return;
  }
  entry_update_x();
}

static void entry_chs(void) {
  if (U.eexp) U.eeneg = !U.eeneg;
  else U.eneg = !U.eneg;
  entry_update_x();
}

static void entry_text(uint8_t *o, int *pn, int cursor) {
  int n = 0;
  uint8_t rad = FL_GET(FLG_RADIX) ? '.' : ',';
  uint8_t sep = FL_GET(FLG_RADIX) ? ',' : '.';
  if (U.eneg) o[n++] = '-';
  int intlen = 0;
  while (intlen < U.eml && U.em[intlen] != '.') intlen++;
  for (int i = 0; i < U.eml; i++) {
    if (U.em[i] == '.') { o[n++] = rad; continue; }
    o[n++] = (uint8_t)U.em[i];
    if (i < intlen && FL_GET(FLG_GROUP)) {
      int rem = intlen - 1 - i;
      if (rem > 0 && rem % 3 == 0) o[n++] = sep;
    }
  }
  if (U.eml == 0) o[n++] = '1';
  if (U.eexp) {
    o[n++] = ' ';
    if (U.eeneg) o[n++] = '-';
    for (int i = 0; i < U.eel; i++) o[n++] = (uint8_t)U.ee[i];
    if (cursor && U.eel < 2) o[n++] = '_';
  } else if (cursor) o[n++] = '_';
  *pn = n;
}

/* ======================= PRGM text line ======================= */
static void text_finish(void) {
  if (!U.tact) return;
  U.tact = 0;
  if (!prgm_mode()) return;
  uint8_t code[30];
  code[0] = OP_TEXT; code[1] = (uint8_t)U.tlen; memcpy(code + 2, U.tbuf, U.tlen);
  err = 0;
  if (!insert_instr(code, U.tlen + 2)) ui_after_error();
}

static void terminate_entries(void) {
  entry_finish();
  text_finish();
  U.aent = 0;
}

/* ======================= menus ======================= */
enum { MI_NONE, MI_FN, MI_CHAR, MI_FLAGT, MI_CAT, MI_APPEND, MI_STK, MI_LBL };
typedef struct { uint8_t kind; uint8_t val; const char *label; } mitem_t;

enum {
  M_NONE, M_DISP, M_MODES, M_CLEAR, M_FLAGS, M_PGM, M_STAT, M_PROB, M_BASE, M_CONV,
  M_MATH, M_ALPHAF, M_MEM, M_CAT, M_ALPHA, M_STK, M_TIME, M_COUNT
};

#define FNI(op) { MI_FN, op, 0 }
#define BLANK { MI_NONE, 0, 0 }
static const mitem_t m_disp[]  = { FNI(81), FNI(82), FNI(83), { MI_FLAGT, 128 + 28, "RDX." }, { MI_FLAGT, 28, "RDX," }, { MI_FLAGT, 29, "GRP" } };
static const mitem_t m_modes[] = { FNI(84), FNI(85), FNI(86), FNI(101), BLANK, BLANK };
static const mitem_t m_clear[] = { FNI(45), FNI(87), FNI(51), FNI(104), FNI(110), FNI(103) };
static const mitem_t m_flags[] = { FNI(63), FNI(64), FNI(65), FNI(66), FNI(67), FNI(68) };
static const mitem_t m_pgm[]   = {
  FNI(93), FNI(96), FNI(97), FNI(98), FNI(92), FNI(105),
  FNI(53), FNI(54), FNI(55), FNI(56), FNI(57), BLANK,
  FNI(58), FNI(59), FNI(60), FNI(61), FNI(62), BLANK,
  FNI(77), FNI(78), FNI(76), FNI(88), FNI(99), FNI(100),
  FNI(95), FNI(94), FNI(101), FNI(102), FNI(103), BLANK };
static const mitem_t m_stat[]  = { FNI(47), FNI(48), FNI(49), FNI(50), FNI(52), FNI(51) };
static const mitem_t m_prob[]  = { FNI(27), FNI(29), FNI(30), FNI(28), BLANK, BLANK };
static const mitem_t m_base[]  = { FNI(40), FNI(41), BLANK, BLANK, BLANK, BLANK };
static const mitem_t m_conv[]  = { FNI(38), FNI(39), FNI(34), FNI(35), FNI(32), FNI(33),
                                   FNI(36), FNI(37), BLANK, BLANK, BLANK, BLANK };
static const mitem_t m_math[]  = { FNI(7), FNI(8), FNI(6), FNI(9), FNI(10), FNI(28),
                                   FNI(19), FNI(20), FNI(27), FNI(30), FNI(40), FNI(41) };
static const mitem_t m_alphaf[]= { FNI(79), FNI(80), FNI(88), FNI(89), FNI(90), FNI(91),
                                   FNI(87), FNI(92), BLANK, BLANK, BLANK, BLANK };
static const mitem_t m_mem[]   = { FNI(106), FNI(107), FNI(108), FNI(110), FNI(104), FNI(111) };
static const mitem_t m_cat[]   = { { MI_CAT, 1, "CAT 1" }, { MI_CAT, 2, "CAT 2" }, { MI_CAT, 3, "CAT 3" }, BLANK, BLANK, BLANK };
static const mitem_t m_alpha[] = {
  FNI(80), FNI(79), FNI(88), FNI(89), { MI_APPEND, 0, "\x81" }, FNI(87),
  { MI_CHAR, ',', "," }, { MI_CHAR, ':', ":" }, { MI_CHAR, ';', ";" }, { MI_CHAR, '!', "!" }, { MI_CHAR, '?', "?" }, { MI_CHAR, '\'', "'" },
  { MI_CHAR, '(', "(" }, { MI_CHAR, ')', ")" }, { MI_CHAR, '<', "<" }, { MI_CHAR, '>', ">" }, { MI_CHAR, '=', "=" }, { MI_CHAR, '#', "#" },
  { MI_CHAR, '$', "$" }, { MI_CHAR, '&', "&" }, { MI_CHAR, '@', "@" }, { MI_CHAR, '%', "%" }, { MI_CHAR, '*', "*" }, { MI_CHAR, CH_SIGMA, "\x82" },
  { MI_CHAR, '"', "\"" }, { MI_CHAR, '^', "^" }, { MI_CHAR, '_', "_" }, { MI_CHAR, CH_NE, "\x83" }, { MI_CHAR, CH_ANGLE, "\x85" }, { MI_CHAR, '/', "/" } };
static const mitem_t m_stk[]   = { { MI_STK, ARG_ST_X, "ST X" }, { MI_STK, ARG_ST_Y, "ST Y" }, { MI_STK, ARG_ST_Z, "ST Z" },
                                   { MI_STK, ARG_ST_T, "ST T" }, { MI_STK, ARG_ST_L, "ST L" }, BLANK };

static const mitem_t m_time[]  = {
  FNI(112), FNI(113), FNI(114), FNI(115), FNI(116), FNI(121),
  FNI(124), FNI(125), FNI(126), FNI(117), FNI(118), FNI(119),
  FNI(120), FNI(122), FNI(123), FNI(127), FNI(128), FNI(129),
  FNI(130), FNI(131), FNI(132), FNI(133), FNI(134), FNI(135),
  FNI(136), FNI(137), FNI(139), FNI(138), BLANK, BLANK };

static const struct { const mitem_t *items; int n; const char *title; } MENUS[M_COUNT] = {
  { 0, 0, 0 },
  { m_disp, 6, "DISP" }, { m_modes, 6, "MODES" }, { m_clear, 6, "CLEAR" }, { m_flags, 6, "FLAGS" },
  { m_pgm, 30, "PGM.FCN" }, { m_stat, 6, "STAT" }, { m_prob, 6, "PROB" }, { m_base, 6, "BASE" },
  { m_conv, 12, "CONVERT" }, { m_math, 12, "MATH" }, { m_alphaf, 12, "ALPHA FNS" }, { m_mem, 6, "MEMORY" },
  { m_cat, 6, "CATALOG" }, { m_alpha, 30, "ALPHA" }, { m_stk, 6, "STACK" }, { m_time, 30, "TIME" },
};

static int cur_menu(void) {
  if (U.pr && U.pr_stk) return M_STK;
  if (U.menu) return U.menu;
  if (FL_GET(FLG_ALPHA) && !U.pr) return M_ALPHA;
  return M_NONE;
}
static int menu_pages(int m) { return (MENUS[m].n + 5) / 6; }
static void open_menu(int m) { U.menu = m; U.page = 0; }

/* ======================= prompts ======================= */
static int prompt_need(int argt) {
  switch (argt) {
    case A_DIG: case A_CAT: return 1;
    case A_SIZE: case A_DEL: return 3;
    default: return 2;
  }
}

static void prompt_open(int op) {
  terminate_entries();
  U.pr = 1; U.pr_op = op; U.pr_argt = FN[op].argt;
  U.pr_need = prompt_need(U.pr_argt);
  U.pr_nlen = 0; U.pr_alpha = 0; U.pr_dot = 0; U.pr_dotdig = 0; U.pr_asn = 0; U.pr_clp = 0;
  U.pr_ind = 0; U.pr_stk = 0; U.pr_ndig = 0; U.pr_val = 0;
  if (op == O_ASN) { U.pr_asn = 1; U.pr_alpha = 1; }
  if (op == O_CLP) { U.pr_clp = 1; U.pr_alpha = 1; }
}
void ui_prompt_fn(int op) { prompt_open(op); }

static void prompt_close(void) { U.pr = 0; U.pr_alpha = 0; U.pr_asn = 0; U.pr_clp = 0; U.pr_stk = 0; }

static void cat_start(int type);

static void preview_set(const uint8_t *s, int n);
static void prompt_complete_exec(int op, int arg);
/* the last key of a prompt: show the complete instruction, execute on release */
static void prompt_complete(int arg) {
  int op = U.pr_op;
  prompt_close();
  uint8_t b[32]; int n = 0;
  const char *nm = FN[op].name;
  while (*nm) b[n++] = (uint8_t)*nm++;
  b[n++] = ' ';
  if (FN[op].argt == A_SIZE || FN[op].argt == A_DEL) {
    b[n++] = (uint8_t)('0' + arg / 100); b[n++] = (uint8_t)('0' + (arg / 10) % 10); b[n++] = (uint8_t)('0' + arg % 10);
  } else if (FN[op].argt == A_CAT) b[n++] = (uint8_t)('0' + arg);
  else n += arg_text(FN[op].argt, arg, b + n);
  preview_set(b, n);
  U.pend = PEND_CMPL; U.pend_op = op; U.pend_arg = arg; U.pend_t = now_ms(); U.pend_null = 0;
}

static void prompt_complete_exec(int op, int arg) {
  switch (FN[op].argt) {
    case A_SIZE:
      err = 0; FN[op].fn(arg); ui_after_error(); return;
    case A_DEL:
      if (prgm_mode()) { FN[op].fn(arg); }
      return;
    case A_CAT:
      cat_start(arg); return;
  }
  if (op == O_GTO && !prgm_mode() && !(arg & ARG_IND)) {
    /* GTO from keyboard only positions the pointer */
  }
  do_instr(op, arg, 0, 0);
}

static void prompt_complete_alpha(void) {
  int op = U.pr_op;
  uint8_t name[16]; int len = U.pr_nlen;
  memcpy(name, U.pr_name, len);
  if (U.pr_clp) { prompt_close(); err = 0; clp_name(name, len); ui_after_error(); return; }
  if (U.pr_asn) { U.pr_asn = 2; U.pr_alpha = 0; return; }
  if (U.pr_dot) { /* GTO . "name" : position only */
    prompt_close();
    int off = find_global_label(name, len);
    if (off < 0) { show_message_str("NONEXISTENT"); return; }
    S->rsp = 0; goto_off(off);
    return;
  }
  prompt_close();
  if (len == 0) return;
  /* show e.g. XEQ "NAME" while the key is held, run on release */
  {
    uint8_t b[32]; int n = 0;
    const char *nm = FN[op].name;
    while (*nm) b[n++] = (uint8_t)*nm++;
    b[n++] = op == O_LBL ? 0 : ' ';
    if (op == O_LBL) n--;
    b[n++] = CH_TEE;
    memcpy(b + n, name, len); n += len;
    preview_set(b, n);
    memcpy(U.pend_str, name, len); U.pend_slen = len;
    U.pend = PEND_ACMPL; U.pend_op = op; U.pend_t = now_ms(); U.pend_null = 0;
  }
}

static void alpha_complete_exec(int op, const uint8_t *name, int len) {
  if (op == O_LBL) { do_instr(OP_LBLA, 0, name, len); return; }
  if (op == O_GTO) { do_instr(OP_GTOA, 0, name, len); return; }
  if (op == O_XEQ) {
    if (prgm_mode()) {
      int fop = lookup_fn(name, len);
      if (fop > 0 && find_global_label(name, len) < 0) {
        if (FN[fop].argt != A_NONE && !(FN[fop].flags & F_NP)) { prompt_open(fop); return; }
        if (FN[fop].flags & F_NP) {   /* non-programmable: execute */
          if (FN[fop].argt != A_NONE) { prompt_open(fop); return; }
          err = 0; exec_op(fop, 0, 0, 0); ui_after_error(); return;
        }
        do_instr(fop, 0, 0, 0);
        return;
      }
      do_instr(OP_XEQA, 0, name, len);
      return;
    }
    err = 0;
    S->rsp = 0;
    xeq_name(name, len, 0);
    if (err) ui_after_error();
    return;
  }
}

/* DM42 printed letters for alpha typing */
static int alpha_char(int key, int shift) {
  static const char un[44] = {
    0, 'A','B','C','D','E','F',  'G','H','I','J','K','L',
    0, 'M','N','O', 0,  0,'P','Q','R','S',  0,'T','U','V','W',
    0,'X','Y','Z','-',  0,' ','.', 0,'+' };
  static const char sh[44] = {
    0, 0,'^',0,0,0,0,  0,'%',0,0,0,0,
    0,0,0,'E',0,  0,'7','8','9','/',  0,'4','5','6','*',
    0,'1','2','3','-',  0,'0','.', 0,'+' };
  if (key < 1 || key > 37) return 0;
  return (unsigned char)(shift ? sh[key] : un[key]);
}

/* 41-style label letters on the DM42: A..E on the top row, F..J on x<>y RDN SIN COS TAN */
static int label_letter(int key, int shift) {
  switch (key) {
    case K_SIGMA: return shift ? ARG_LBL_a + 0 : ARG_LBL_A + 0;
    case K_INV:   return shift ? ARG_LBL_a + 1 : ARG_LBL_A + 1;
    case K_SQRT:  return shift ? ARG_LBL_a + 2 : ARG_LBL_A + 2;
    case K_LOG:   return shift ? ARG_LBL_a + 3 : ARG_LBL_A + 3;
    case K_LN:    return shift ? ARG_LBL_a + 4 : ARG_LBL_A + 4;
    case K_SWAP:  return shift ? -1 : ARG_LBL_A + 5;
    case K_RDN:   return shift ? -1 : ARG_LBL_A + 6;
    case K_SIN:   return shift ? -1 : ARG_LBL_A + 7;
    case K_COS:   return shift ? -1 : ARG_LBL_A + 8;
    case K_TAN:   return shift ? -1 : ARG_LBL_A + 9;
  }
  return -1;
}

static int key_code41(int key, int shift) {
  int row, col;
  if (key >= 1 && key <= 12) { row = (key - 1) / 6 + 1; col = (key - 1) % 6 + 1; }
  else if (key >= 13 && key <= 17) { row = 3; col = key - 12; }
  else if (key >= 18 && key <= 37) { row = (key - 18) / 5 + 4; col = (key - 18) % 5 + 1; }
  else return 0;
  int c = row * 10 + col;
  return shift ? -c : c;
}

static void prompt_key(int key) {
  int shift = U.shift; U.shift = 0;
  /* ---- ASN: waiting for the key ---- */
  if (U.pr_asn == 2) {
    if (key == K_SHIFT) return;
    if (key == K_EXIT || (key == K_BSP && !shift)) { prompt_close(); return; }
    int code = key_code41(key, shift);
    if (!code || key == K_SHIFT || key == K_EXIT) { prompt_close(); return; }
    /* remove existing assignment of the key */
    for (int i = 0; i < S->nasn; i++)
      if (S->asn[i].key == code) { S->asn[i] = S->asn[--S->nasn]; break; }
    if (U.pr_nlen > 0) {
      asn_t a; memset(&a, 0, sizeof a);
      a.key = (int16_t)code;
      int fop = lookup_fn(U.pr_name, U.pr_nlen);
      if (find_global_label(U.pr_name, U.pr_nlen) >= 0) { a.kind = 2; }
      else if (fop > 0) { a.kind = 1; a.op = (uint8_t)fop; }
      else { prompt_close(); show_message_str("NONEXISTENT"); return; }
      memcpy(a.name, U.pr_name, U.pr_nlen > 7 ? 7 : U.pr_nlen);
      if (S->nasn >= MAXASN || ((S->nasn % 2) == 0 && free_regs() < 1)) { prompt_close(); show_message_str("NO ROOM"); return; }
      S->asn[S->nasn++] = a;
    }
    prompt_close();
    return;
  }

  /* ---- alpha name entry ---- */
  if (U.pr_alpha) {
    if (key == K_SHIFT) { U.shift = !shift; return; }
    if (key == K_ENTER && shift) { prompt_complete_alpha(); return; }   /* ALPHA */
    if (key == K_ENTER && !shift && (U.pr_asn || U.pr_clp)) { prompt_complete_alpha(); return; }
    if (key == K_EXIT) { prompt_close(); return; }
    if (key == K_BSP) {
      if (U.pr_nlen > 0) U.pr_nlen--;
      else if (U.pr_asn || U.pr_clp) prompt_close();
      else U.pr_alpha = 0;
      return;
    }
    int c = alpha_char(key, shift);
    if (key >= K_F1 && key <= K_F6) {
      const mitem_t *it = &m_alpha[U.page * 6 + (key - K_F1)];
      if (it->kind == MI_CHAR) c = it->val; else c = 0;
    }
    if (key == K_UP || key == K_DOWN) {
      int np = menu_pages(M_ALPHA);
      U.page = (U.page + (key == K_DOWN ? 1 : np - 1)) % np;
      return;
    }
    if (c && U.pr_nlen < 14) U.pr_name[U.pr_nlen++] = (uint8_t)c;
    return;
  }

  if (key == K_EXIT) { prompt_close(); return; }

  /* ---- stack register selection after "." ---- */
  if (U.pr_stk) {
    int a = -1;
    if (key >= K_F1 && key <= K_F5) a = m_stk[key - K_F1].val;
    else switch (key) {
      case K_1:   a = ARG_ST_X; break;
      case K_2:   a = ARG_ST_Y; break;
      case K_3:   a = ARG_ST_Z; break;
      case K_4:   a = ARG_ST_T; break;
      case K_TAN: a = ARG_ST_L; break;
      case K_BSP: U.pr_stk = 0; return;
    }
    if (a >= 0) prompt_complete(a | (U.pr_ind ? ARG_IND : 0));
    return;
  }

  /* ---- GTO . nnn ---- */
  if (U.pr_dot) {
    if (key == K_SHIFT) { U.shift = !shift; return; }
    if (key == K_DOT && U.pr_dotdig == 0) {   /* GTO .. */
      prompt_close();
      terminate_entries();
      gto_dotdot();
      return;
    }
    if (key == K_ENTER && shift) { U.pr_alpha = 1; U.pr_nlen = 0; return; }
    if (key == K_BSP) { if (U.pr_dotdig) { U.pr_dotdig--; U.pr_val /= 10; } else U.pr_dot = 0; return; }
    int d = -1;
    switch (key) { case K_0: d = 0; break; case K_1: d = 1; break; case K_2: d = 2; break; case K_3: d = 3; break;
      case K_4: d = 4; break; case K_5: d = 5; break; case K_6: d = 6; break; case K_7: d = 7; break;
      case K_8: d = 8; break; case K_9: d = 9; break; }
    if (d < 0) return;
    U.pr_val = U.pr_val * 10 + d;
    if (++U.pr_dotdig == 3) {
      int line = U.pr_val;
      prompt_close();
      int maxl = prog_lines(S->cur_start);
      if (line > maxl) line = maxl;
      S->rsp = 0;
      S->pc_line = line;
      S->pc_off = line_to_off(S->cur_start, line < 1 ? 1 : line);
    }
    return;
  }

  int argt = U.pr_argt;
  int ind_ok = (argt == A_REG || argt == A_DIG || argt == A_FLAG || argt == A_GOTO);

  if (key == K_SHIFT) {
    if (ind_ok && U.pr_ndig == 0) { U.pr_ind = !U.pr_ind; return; }
    U.shift = !shift;
    return;
  }
  if (key == K_BSP) {
    if (U.pr_ndig > 0) { U.pr_ndig--; U.pr_val /= 10; }
    else if (U.pr_ind) U.pr_ind = 0;
    else prompt_close();
    return;
  }
  if (key == K_ENTER && (shift || U.pr_ind)) {   /* ALPHA (shift ENTER): label names */
    if ((argt == A_LBL || argt == A_GOTO) && U.pr_ndig == 0) { U.pr_ind = 0; U.pr_alpha = 1; U.pr_nlen = 0; }
    return;
  }
  /* STO arithmetic */
  if (U.pr_op == O_STO && U.pr_ndig == 0 && !U.pr_ind) {
    int nop = 0;
    if (key == K_ADD) nop = 71; else if (key == K_SUB) nop = 72;
    else if (key == K_MUL) nop = 73; else if (key == K_DIV) nop = 74;
    if (nop) { U.pr_op = nop; return; }
  }
  if (key == K_DOT && U.pr_ndig == 0) {
    if (U.pr_op == O_GTO && !U.pr_ind) { U.pr_dot = 1; U.pr_dotdig = 0; U.pr_val = 0; return; }
    if (argt == A_REG || U.pr_ind) { U.pr_stk = 1; return; }
    return;
  }
  /* label letters */
  if ((argt == A_LBL || argt == A_GOTO) && U.pr_ndig == 0) {
    int l = label_letter(key, shift || U.pr_ind);
    if (l >= 0) {
      if (U.pr_ind && !shift) l = label_letter(key, 1);
      U.pr_ind = 0;
      if (l >= 0) { prompt_complete(l); return; }
    }
  }
  int d = -1;
  switch (key) { case K_0: d = 0; break; case K_1: d = 1; break; case K_2: d = 2; break; case K_3: d = 3; break;
    case K_4: d = 4; break; case K_5: d = 5; break; case K_6: d = 6; break; case K_7: d = 7; break;
    case K_8: d = 8; break; case K_9: d = 9; break; }
  if (d < 0) return;
  U.pr_val = U.pr_val * 10 + d;
  U.pr_ndig++;
  int need = U.pr_ind ? 2 : U.pr_need;
  if (U.pr_ndig >= need) {
    int a = U.pr_val;
    if (argt == A_CAT && (a < 1 || a > 3)) { prompt_close(); return; }
    prompt_complete(U.pr_ind ? (a | ARG_IND) : a);
  }
}

static int prompt_text(uint8_t *o) {
  int n = 0;
  const char *nm = FN[U.pr_op].name;
  if (U.pr_asn == 2) {
    n += (memcpy(o, "ASN ", 4), 4);
    memcpy(o + n, U.pr_name, U.pr_nlen); n += U.pr_nlen;
    o[n++] = ' ';
    if (U.shift) o[n++] = '-';
    o[n++] = '_';
    return n;
  }
  while (*nm) o[n++] = (uint8_t)*nm++;
  o[n++] = ' ';
  if (U.pr_dot) {
    o[n++] = '.';
    if (U.pr_alpha) { o[n++] = CH_TEE; memcpy(o + n, U.pr_name, U.pr_nlen); n += U.pr_nlen; o[n++] = '_'; return n; }
    for (int i = 0; i < 3; i++) {
      if (i < U.pr_dotdig) { int p = 1; for (int j = 0; j < U.pr_dotdig - 1 - i; j++) p *= 10; o[n++] = (uint8_t)('0' + (U.pr_val / p) % 10); }
      else o[n++] = '_';
    }
    return n;
  }
  if (U.pr_alpha) {
    if (!U.pr_asn && !U.pr_clp) o[n++] = CH_TEE;
    memcpy(o + n, U.pr_name, U.pr_nlen); n += U.pr_nlen;
    o[n++] = '_';
    return n;
  }
  if (U.pr_ind) { memcpy(o + n, "IND ", 4); n += 4; }
  if (U.pr_stk) { memcpy(o + n, "ST _", 4); n += 4; return n; }
  int need = U.pr_ind ? 2 : U.pr_need;
  for (int i = 0; i < need; i++) {
    if (i < U.pr_ndig) {
      int p = 1; for (int j = 0; j < U.pr_ndig - 1 - i; j++) p *= 10;
      o[n++] = (uint8_t)('0' + (U.pr_val / p) % 10);
    } else o[n++] = '_';
  }
  return n;
}

/* ======================= catalog ======================= */
static int cat_item(int idx, uint8_t *o) {
  if (U.cat_type == 4) return tm_alarm_text(idx, o);
  if (U.cat_type == 3) {
    int k = 0;
    for (int i = 1; i < FN_COUNT; i++) {
      if (FN[i].flags & F_HIDE) continue;
      if (k++ == idx) { int n = (int)strlen(FN[i].name); memcpy(o, FN[i].name, n); return n; }
    }
    return -1;
  }
  if (U.cat_type == 1) {
    int k = 0;
    for (int off = 0; off < S->plen; off += ilen(off)) {
      int op = S->prog[off];
      if (op != OP_LBLA && op != OP_END) continue;
      if (k++ == idx) return instr_text(off, o);
    }
    if (k == idx) {
      int n = 0; memcpy(o, ".END. REG ", 10); n = 10;
      int f = free_regs();
      if (f >= 100) o[n++] = (uint8_t)('0' + f / 100);
      o[n++] = (uint8_t)('0' + (f / 10) % 10); o[n++] = (uint8_t)('0' + f % 10);
      return n;
    }
    return -1;
  }
  return -1;
}
static void cat_start(int type) {
  if (type == 2) { show_message_str("CAT 2 EMPTY"); return; }
  U.cat = 1; U.cat_type = type; U.cat_idx = 0; U.cat_run = 1; U.cat_t = now_ms();
}
static void cat_key(int key) {
  U.shift = 0;
  uint8_t b[40];
  switch (key) {
    case K_RUN: U.cat_run = !U.cat_run; U.cat_t = now_ms(); break;
    case K_DOWN: U.cat_run = 0; if (cat_item(U.cat_idx + 1, b) >= 0) U.cat_idx++; U.scroll_t = now_ms(); break;
    case K_UP: U.cat_run = 0; if (U.cat_idx > 0) U.cat_idx--; U.scroll_t = now_ms(); break;
    case K_EXIT: case K_BSP: U.cat = 0; break;
    default: U.cat_run = 0; break;
  }
}

void ui_clock_mode(int on) { U.clock = on; }
void ui_sw_mode(int on) { U.swd = on; }
void ui_almcat(void) {
  if (!S->nalm) { show_message_str("NO ALARMS"); return; }
  U.cat = 1; U.cat_type = 4; U.cat_idx = 0; U.cat_run = 0; U.scroll_t = now_ms();
}

/* ======================= key tables ======================= */
enum {
  A_NO, A_FN, A_PR, A_DIGIT, A_DOT, A_EEX, A_CHS, A_BSP, A_MENU, A_ALPHA_T, A_PRGM,
  A_RS, A_SST, A_BST, A_UP, A_DOWN, A_EXIT, A_OFF, A_USER, A_ASNK, A_SHOW, A_CATM
};
typedef struct { uint8_t act, val; } kact_t;

static const kact_t KT[38][2] = {
  /* 0 */ {{A_NO,0},{A_NO,0}},
  /* SIGMA */ {{A_FN,O_SIGP},{A_FN,O_SIGM}},
  /* INV   */ {{A_FN,O_INV},{A_FN,O_YX}},
  /* SQRT  */ {{A_FN,O_SQRT},{A_FN,O_X2}},
  /* LOG   */ {{A_FN,O_LOG},{A_FN,O_10X}},
  /* LN    */ {{A_FN,O_LN},{A_FN,O_EX}},
  /* XEQ   */ {{A_PR,O_XEQ},{A_PR,O_GTO}},
  /* STO   */ {{A_PR,O_STO},{A_PR,O_XEX}},
  /* RCL   */ {{A_PR,O_RCL},{A_FN,O_PCT}},
  /* RDN   */ {{A_FN,O_RDN},{A_FN,O_PI}},
  /* SIN   */ {{A_FN,O_SIN},{A_FN,O_ASIN}},
  /* COS   */ {{A_FN,O_COS},{A_FN,O_ACOS}},
  /* TAN   */ {{A_FN,O_TAN},{A_FN,O_ATAN}},
  /* ENTER */ {{A_FN,O_ENTER},{A_ALPHA_T,0}},
  /* SWAP  */ {{A_FN,O_SWAP},{A_FN,O_LASTX}},
  /* CHS   */ {{A_CHS,0},{A_MENU,M_MODES}},
  /* E     */ {{A_EEX,0},{A_MENU,M_DISP}},
  /* BSP   */ {{A_BSP,0},{A_MENU,M_CLEAR}},
  /* UP    */ {{A_UP,0},{A_BST,0}},
  /* 7     */ {{A_DIGIT,'7'},{A_MENU,M_ALPHAF}},
  /* 8     */ {{A_DIGIT,'8'},{A_MENU,M_MEM}},
  /* 9     */ {{A_DIGIT,'9'},{A_MENU,M_MATH}},
  /* DIV   */ {{A_FN,O_DIV},{A_MENU,M_STAT}},
  /* DOWN  */ {{A_DOWN,0},{A_SST,0}},
  /* 4     */ {{A_DIGIT,'4'},{A_MENU,M_BASE}},
  /* 5     */ {{A_DIGIT,'5'},{A_MENU,M_CONV}},
  /* 6     */ {{A_DIGIT,'6'},{A_MENU,M_FLAGS}},
  /* MUL   */ {{A_FN,O_MUL},{A_MENU,M_PROB}},
  /* SHIFT */ {{A_NO,0},{A_NO,0}},
  /* 1     */ {{A_DIGIT,'1'},{A_ASNK,0}},
  /* 2     */ {{A_DIGIT,'2'},{A_USER,0}},
  /* 3     */ {{A_DIGIT,'3'},{A_MENU,M_PGM}},
  /* SUB   */ {{A_FN,O_SUB},{A_MENU,M_TIME}},
  /* EXIT  */ {{A_EXIT,0},{A_OFF,0}},
  /* 0     */ {{A_DIGIT,'0'},{A_NO,0}},
  /* DOT   */ {{A_DOT,0},{A_SHOW,0}},
  /* RUN   */ {{A_RS,0},{A_PRGM,0}},
  /* ADD   */ {{A_FN,O_ADD},{A_CATM,0}},
};

/* ======================= executing a function key ======================= */
static void preview_set(const uint8_t *s, int n) {
  if (n > (int)sizeof U.pend_txt) n = sizeof U.pend_txt;
  memcpy(U.pend_txt, s, n); U.pend_len = n;
}

static void fn_key(int op) {
  if (op == O_ENTER || op == O_CHS) { /* fall through to normal handling */ }
  if (FN[op].argt != A_NONE) { prompt_open(op); return; }
  terminate_entries();
  /* execute (or record in PRGM mode) on release, show the name while held */
  U.pend = PEND_FN; U.pend_op = op; U.pend_arg = 0; U.pend_t = now_ms(); U.pend_null = 0;
  preview_set((const uint8_t *)FN[op].name, (int)strlen(FN[op].name));
}

static void run_label_key(int lbl) {
  terminate_entries();
  U.pend = PEND_FN; U.pend_op = O_XEQ; U.pend_arg = lbl; U.pend_t = now_ms(); U.pend_null = 0;
  uint8_t b[16]; int n = 0;
  memcpy(b, "XEQ ", 4); n = 4;
  n += arg_text(A_LBL, lbl, b + n);
  preview_set(b, n);
}

static void line_preview(void) {
  uint8_t b[40]; int n = 0;
  int line = S->pc_line;
  if (line >= 100) b[n++] = (uint8_t)('0' + (line / 100) % 10);
  b[n++] = (uint8_t)('0' + (line / 10) % 10);
  b[n++] = (uint8_t)('0' + line % 10);
  b[n++] = ' ';
  if (line == 0) { /* top of program */ }
  else n += instr_text(S->pc_off, b + n);
  preview_set(b, n);
}

static void move_line(int dir) {
  int lines = prog_lines(S->cur_start);
  int l = S->pc_line + dir;
  if (l < 0) l = lines;
  if (l > lines) l = 0;
  S->pc_line = l;
  S->pc_off = line_to_off(S->cur_start, l < 1 ? 1 : l);
}

static void do_menu_item(const mitem_t *it) {
  switch (it->kind) {
    case MI_FN:
      if (it->val == O_CLP) { prompt_open(O_CLP); return; }
      if (it->val == O_SIZE || it->val == O_DEL || it->val == O_ASN || FN[it->val].argt != A_NONE) { prompt_open(it->val); return; }
      if (FN[it->val].flags & F_NP) { terminate_entries(); err = 0; exec_op(it->val, 0, 0, 0); ui_after_error(); return; }
      fn_key(it->val);
      return;
    case MI_FLAGT: {
      terminate_entries();
      int f = it->val & 127;
      if (f == 29) { if (FL_GET(29)) FL_CLR(29); else FL_SET(29); }
      else if (it->val & 128) FL_SET(f); else FL_CLR(f);
      return;
    }
    case MI_CAT: terminate_entries(); U.menu = 0; cat_start(it->val); return;
    case MI_APPEND:
      if (prgm_mode()) { if (!U.tact) { U.tact = 1; U.tlen = 0; } if (U.tlen == 0) U.tbuf[U.tlen++] = CH_APPEND; }
      else U.aent = 1;
      return;
    case MI_CHAR: {
      uint8_t c = it->val;
      if (prgm_mode()) { if (!U.tact) { U.tact = 1; U.tlen = 0; } if (U.tlen < 15) U.tbuf[U.tlen++] = c; }
      else { if (!U.aent) { alpha_clear(); U.aent = 1; } alpha_append(&c, 1); }
      return;
    }
  }
}

static int user_key(int key, int shift) {
  if (!FL_GET(FLG_USER)) return 0;
  int code = key_code41(key, shift);
  for (int i = 0; i < S->nasn; i++) {
    if (S->asn[i].key != code) continue;
    asn_t *a = &S->asn[i];
    int len = (int)strnlen(a->name, 7);
    if (a->kind == 1) { fn_key(a->op); return 1; }
    terminate_entries();
    if (prgm_mode()) { do_instr(OP_XEQA, 0, (const uint8_t *)a->name, len); return 1; }
    err = 0; S->rsp = 0;
    xeq_name((const uint8_t *)a->name, len, 0);
    ui_after_error();
    return 1;
  }
  /* local labels A-J / a-e on the 41 positions */
  int l = label_letter(key, shift);
  if (l >= 0 && find_local_label(l) >= 0) { run_label_key(l); return 1; }
  return 0;
}

static void normal_key(int key) {
  int shift = U.shift;
  U.shift = 0;
  int m = cur_menu();

  if (key >= K_F1 && key <= K_F6) {
    if (m) {
      int idx = U.page * 6 + (key - K_F1);
      if (idx < MENUS[m].n) do_menu_item(&MENUS[m].items[idx]);
      return;
    }
    if (FL_GET(FLG_USER)) {
      int l = shift ? (key - K_F1 < 5 ? ARG_LBL_a + key - K_F1 : -1) : ARG_LBL_A + key - K_F1;
      if (l >= 0) run_label_key(l);
    }
    return;
  }
  if (user_key(key, shift)) return;

  kact_t a = KT[key][shift ? 1 : 0];
  switch (a.act) {
    case A_FN: fn_key(a.val); return;
    case A_PR: prompt_open(a.val); return;
    case A_DIGIT: text_finish(); entry_key_digit((char)a.val); return;
    case A_DOT: text_finish(); entry_key_digit('.'); return;
    case A_EEX: text_finish(); entry_eex(); return;
    case A_CHS:
      if (U.ent) { entry_chs(); return; }
      fn_key(O_CHS); return;
    case A_BSP:
      if (U.ent) { entry_bsp(); return; }
      if (prgm_mode()) { delete_line(); return; }
      fn_key(O_CLX); return;
    case A_MENU: terminate_entries(); open_menu(a.val); return;
    case A_CATM: terminate_entries(); open_menu(M_CAT); return;
    case A_ALPHA_T:
      terminate_entries();
      FL_SET(FLG_ALPHA); U.aent = 0; U.page = 0; U.menu = 0;
      return;
    case A_PRGM:
      terminate_entries();
      if (FL_GET(FLG_PRGM)) FL_CLR(FLG_PRGM); else FL_SET(FLG_PRGM);
      return;
    case A_RS:
      terminate_entries();
      if (prgm_mode()) { do_instr(O_STOP, 0, 0, 0); return; }
      err = 0;
      start_running();
      return;
    case A_UP:
      if (m && menu_pages(m) > 1) { U.page = (U.page + menu_pages(m) - 1) % menu_pages(m); return; }
      /* fall through */
    case A_BST:
      terminate_entries();
      move_line(-1);
      if (!prgm_mode()) { U.pend = PEND_BST; line_preview(); }
      return;
    case A_DOWN:
      if (m && menu_pages(m) > 1) { U.page = (U.page + 1) % menu_pages(m); return; }
      /* fall through */
    case A_SST:
      terminate_entries();
      if (prgm_mode()) { move_line(1); return; }
      if (S->pc_line == 0) { S->pc_line = 1; S->pc_off = S->cur_start; }
      U.pend = PEND_SST; U.pend_t = now_ms(); U.pend_null = 0;
      line_preview();
      return;
    case A_EXIT:
      if (U.ent) { entry_finish(); }
      if (U.menu) { U.menu = 0; return; }
      if (FL_GET(FLG_PRGM)) { terminate_entries(); FL_CLR(FLG_PRGM); return; }
      return;
    case A_OFF: terminate_entries(); ui_off(); return;
    case A_USER:
      terminate_entries();
      if (FL_GET(FLG_USER)) FL_CLR(FLG_USER); else FL_SET(FLG_USER);
      return;
    case A_ASNK: prompt_open(O_ASN); return;
    case A_SHOW:
      terminate_entries();
      U.pend = PEND_SHOW;
      if (S->stk[SX].isalpha) preview_set(S->stk[SX].s, S->stk[SX].len);
      else { uint8_t b[24]; int n = num_format_full(S->stk[SX].x, b); preview_set(b, n); }
      return;
  }
}

static void alpha_key(int key) {
  int shift = U.shift;
  if (key == K_ENTER && shift) {        /* leave ALPHA */
    U.shift = 0;
    terminate_entries();
    FL_CLR(FLG_ALPHA);
    U.page = 0;
    return;
  }
  if (key == K_EXIT && !shift) {
    U.shift = 0;
    terminate_entries();
    FL_CLR(FLG_ALPHA); U.page = 0;
    return;
  }
  if ((key == K_RUN) || (key == K_EXIT && shift) || (key >= K_F1 && key <= K_F6) || key == K_UP || key == K_DOWN) {
    if (key == K_RUN && !shift) {
      text_finish();
      if (!prgm_mode()) { U.shift = 0; FL_CLR(FLG_ALPHA); U.aent = 0; err = 0; start_running(); return; }
    }
    if (key == K_UP || key == K_DOWN) {
      if (!U.menu) {
        U.shift = 0;
        if (shift) { normal_key(key); return; }
        int np = menu_pages(M_ALPHA);
        U.page = (U.page + (key == K_DOWN ? 1 : np - 1)) % np;
        return;
      }
    }
    normal_key(key);
    return;
  }
  U.shift = 0;
  if (key == K_BSP) {
    if (shift) { text_finish(); alpha_clear(); return; }
    if (prgm_mode()) {
      if (U.tact) { if (U.tlen > 0) U.tlen--; if (U.tlen == 0) U.tact = 0; return; }
      delete_line();
      return;
    }
    if (U.aent) { if (S->alen > 0) S->alen--; if (S->alen == 0) U.aent = 0; return; }
    alpha_clear();
    return;
  }
  int c = alpha_char(key, shift);
  if (!c) return;
  if (prgm_mode()) {
    if (!U.tact) { U.tact = 1; U.tlen = 0; }
    if (U.tlen < 15) U.tbuf[U.tlen++] = (uint8_t)c;
    return;
  }
  if (!U.aent) { alpha_clear(); U.aent = 1; }
  uint8_t ch = (uint8_t)c;
  alpha_append(&ch, 1);
}

/* ======================= public key entry ======================= */
static void key_release(void) {
  int p = U.pend;
  U.pend = PEND_NONE;
  if (U.pend_null) { U.pend_null = 0; return; }
  switch (p) {
    case PEND_FN:
      do_instr(U.pend_op, U.pend_arg, 0, 0);
      break;
    case PEND_CMPL:
      prompt_complete_exec(U.pend_op, U.pend_arg);
      break;
    case PEND_ACMPL:
      alpha_complete_exec(U.pend_op, U.pend_str, U.pend_slen);
      break;
    case PEND_LBL:
      err = 0; S->rsp = 0;
      exec_op(O_XEQ, U.pend_arg, 0, 0);
      if (err) ui_after_error();
      break;
    case PEND_SST:
      err = 0;
      running = 1; sst_mode = 1;
      exec_step();
      running = 0; sst_mode = 0;
      U.paused = 0;
      if (err) ui_after_error();
      break;
  }
}

void hp41_key(int key) {
  static int down_key;
  if (!S) return;
  if (key == 0) { down_key = 0; key_release(); return; }
  if (key == down_key) return;     /* auto-repeat of a held key: the 41 has none */
  down_key = key;
  if (U.pend) key_release();   /* safety: missed release */
  if (key > K_LAST) return;

  if (U.off_req) U.off_req = 0;

  if (running) {
    if (key == K_RUN || key == K_EXIT) { stop_running(); U.paused = 0; }
    return;
  }
  if (key == K_SHIFT && !U.pr) { U.shift = !U.shift; return; }

  if (U.clock) { U.clock = 0; U.shift = 0; return; }
  if (U.swd) {
    if (key == K_RUN) { if (S->sw_run) { S->sw_acc = tm_sw_value(); S->sw_run = 0; } else { S->sw_start = tm_now_cs(); S->sw_run = 1; } }
    else if (key == K_BSP && !S->sw_run) S->sw_acc = 0;
    else if (key == K_EXIT) U.swd = 0;
    return;
  }
  if (msg_active) {
    msg_active = 0; FL_CLR(FLG_MSG);
    if ((key == K_BSP || key == K_EXIT) && !U.shift) return;
  }
  if (U.cat) { cat_key(key); return; }
  if (U.pr) { prompt_key(key); return; }
  if (FL_GET(FLG_ALPHA)) { alpha_key(key); return; }
  normal_key(key);
}

void hp41_tick(void) {
  if (!S) return;
  uint32_t t = now_ms();
  if ((U.pend == PEND_FN || U.pend == PEND_LBL || U.pend == PEND_SST || U.pend == PEND_CMPL || U.pend == PEND_ACMPL)
      && !U.pend_null && t - U.pend_t >= 1000) {
    U.pend_null = 1;
    preview_set((const uint8_t *)"NULL", 4);
  }
  if (U.paused && (int32_t)(t - U.pause_until) >= 0) U.paused = 0;
  if (t - U.alm_t >= 1000 || t < U.alm_t) { U.alm_t = t; if (!U.pr) tm_check_alarms(); }
  if (U.cat && U.cat_run && t - U.cat_t > 400) {
    uint8_t b[40];
    U.cat_t = t;
    if (cat_item(U.cat_idx + 1, b) >= 0) U.cat_idx++;
    else U.cat = 0;
  }
}

int hp41_busy(void) {
  return (running ? 1 : 0) | (U.paused ? 2 : 0) | ((U.cat && U.cat_run) ? 4 : 0) | (U.pend != PEND_NONE ? 8 : 0)
       | (U.clock ? 16 : 0) | (U.scrolling ? 32 : 0) | ((U.swd && S->sw_run) ? 64 : 0);
}

void hp41_run_slice(int steps) {
  if (!S) return;
  while (steps-- > 0 && running && !U.paused) {
    exec_step();
  }
  if (!running) {
    U.paused = 0;
    if (FL_GET(FLG_ALPHA)) U.aent = 0;
  }
}

/* ======================= display ======================= */
static void line_text(uint8_t *b, int *pn) {
  int n = 0;
  int line = S->pc_line;
  if (line >= 100) b[n++] = (uint8_t)('0' + (line / 100) % 10);
  b[n++] = (uint8_t)('0' + (line / 10) % 10);
  b[n++] = (uint8_t)('0' + line % 10);
  b[n++] = ' ';
  if (line > 0) n += instr_text(S->pc_off, b + n);
  else if (S->pc_line == 0) {
    /* line 00 shows the bytes free on the 41; show REG count */
    memcpy(b + n, "REG ", 4); n += 4;
    int f = free_regs();
    if (f >= 100) b[n++] = (uint8_t)('0' + f / 100);
    b[n++] = (uint8_t)('0' + (f / 10) % 10); b[n++] = (uint8_t)('0' + f % 10);
  }
  *pn = n;
}

static const char *menu_label(const mitem_t *it) {
  switch (it->kind) {
    case MI_FN: return FN[it->val].name;
    case MI_NONE: return 0;
    default: return it->label;
  }
}

void hp41_get_display(hp41_disp_t *d) {
  memset(d, 0, sizeof *d);
  uint8_t b[96]; int n = 0;
  int start = 0;
  if (!S) return;

  /* a function key shows its name only when held for a moment (300 ms), so quick
     key presses don't flash the name; prompts, SST/BST and SHOW show at once */
  int show_pend = U.pend && U.pend_len &&
                  (U.pend != PEND_FN || U.pend_null || now_ms() - U.pend_t >= 300);
  if (show_pend) {
    memcpy(b, U.pend_txt, U.pend_len); n = U.pend_len;
  } else if (U.clock) {
    n = tm_clock_text(b);
  } else if (U.swd) {
    n = tm_sw_text(b);
  } else if (U.cat) {
    n = cat_item(U.cat_idx, b); if (n < 0) n = 0;
    if (U.cat_type == 4) start = -2;
  } else if (U.pr) {
    n = prompt_text(b); start = -1;
    if (cells_len(b, n) <= HP41_CELLS) start = 0;
  } else if (msg_active) {
    memcpy(b, U.msg, U.msglen); n = U.msglen;
    start = -2;
  } else if (running) {
    int pos = (int)((now_ms() / 120) % HP41_CELLS);
    for (int i = 0; i < HP41_CELLS; i++) b[i] = ' ';
    b[pos] = CH_GOOSE;
    n = HP41_CELLS;
  } else if (prgm_mode()) {
    if (U.ent) {
      int l = 0;
      line_text(b, &l);
      /* show the number being entered as a new line */
      n = 0;
      int line = S->pc_line + 1;
      if (line >= 100) b[n++] = (uint8_t)('0' + (line / 100) % 10);
      b[n++] = (uint8_t)('0' + (line / 10) % 10); b[n++] = (uint8_t)('0' + line % 10); b[n++] = ' ';
      int k; entry_text(b + n, &k, 1); n += k;
      start = -1; if (cells_len(b, n) <= HP41_CELLS) start = 0;
    } else if (U.tact) {
      b[n++] = CH_TEE;
      memcpy(b + n, U.tbuf, U.tlen); n += U.tlen;
      b[n++] = '_';
      start = -1; if (cells_len(b, n) <= HP41_CELLS) start = 0;
    } else line_text(b, &n);
  } else if (FL_GET(FLG_ALPHA)) {
    memcpy(b, S->alpha, S->alen); n = S->alen;
    if (U.aent) b[n++] = '_';
    start = -1; if (cells_len(b, n) <= HP41_CELLS) start = 0;
  } else if (U.ent) {
    entry_text(b, &n, 1);
  } else {
    n = val_format(&S->stk[SX], b);
  }
  U.scrolling = 0;
  if (start == -2) {          /* long message: scroll left after a short pause, like the 41 */
    int over = cells_len(b, n) - HP41_CELLS;
    start = 0;
    if (over > 0) {
      int32_t el = (int32_t)(now_ms() - U.scroll_t) - 800;
      int st = el > 0 ? el / 220 : 0;
      if (st >= over) st = over; else U.scrolling = 1;
      start = st;
    }
  }
  cells_from_text(b, n, d, start);

  uint32_t an = 0;
  if (FL_GET(FLG_USER)) an |= AN_USER;
  if (FL_GET(FLG_GRAD)) an |= AN_G | AN_RAD;
  else if (FL_GET(FLG_RAD)) an |= AN_RAD;
  if (U.shift) an |= AN_SHIFT;
  if (FL_GET(0)) an |= AN_0;
  if (FL_GET(1)) an |= AN_1;
  if (FL_GET(2)) an |= AN_2;
  if (FL_GET(3)) an |= AN_3;
  if (FL_GET(4)) an |= AN_4;
  if (FL_GET(FLG_PRGM)) an |= AN_PRGM;
  if (FL_GET(FLG_ALPHA) || (U.pr && U.pr_alpha)) an |= AN_ALPHA;
  if (FL_GET(FLG_LOWBAT)) an |= AN_BAT;
  d->annun = an;

  int m = cur_menu();
  if (U.pr && U.pr_alpha) m = M_ALPHA;
  if (U.pr && !U.pr_alpha && !U.pr_stk) m = 0;
  if (m) {
    d->has_menu = 1;
    int pg = (m == M_STK) ? 0 : U.page;
    if (pg >= menu_pages(m)) pg = 0;
    for (int i = 0; i < 6; i++) {
      int idx = pg * 6 + i;
      d->menu[i] = idx < MENUS[m].n ? menu_label(&MENUS[m].items[idx]) : 0;
    }
    d->menu_more = menu_pages(m) > 1;
    d->info = MENUS[m].title;
  }
}

int hp41_display_text(char *buf, int len) {
  hp41_disp_t d;
  hp41_get_display(&d);
  int n = 0;
  for (int i = 0; i < HP41_CELLS && n < len - 3; i++) {
    uint8_t c = d.ch[i];
    switch (c) {
      case CH_TEE: c = 'T'; break;     /* rendered as T-bar */
      case CH_APPEND: c = '|'; break;
      case CH_SIGMA: c = 'S'; break;
      case CH_NE: c = '#'; break;
      case CH_LE: c = '{'; break;
      case CH_GOOSE: c = '>'; break;
      case CH_ANGLE: c = '<'; break;
    }
    buf[n++] = (char)c;
    if (d.punct[i]) buf[n++] = (char)d.punct[i];
  }
  while (n > 0 && buf[n-1] == ' ') n--;
  buf[n] = 0;
  return n;
}

/* ======================= memory ======================= */
uint32_t hp41_state_size(void) { return sizeof(state_t); }
void *hp41_state_ptr(void) { return S; }
void hp41_set_platform(const hp41_platform_t *p) { PLAT = p; }

void hp41_master_clear(void) {
  memset(S, 0, sizeof *S);
  S->magic = STATE_MAGIC;
  S->version = STATE_VERSION;
  S->size_check = sizeof(state_t);
  S->size = 100;
  S->sreg = 11;
  FL_SET(FLG_AUDIO);
  FL_SET(FLG_RADIX);
  FL_SET(FLG_GROUP);
  FL_SET(FLG_FIX);
  FL_SET(37);            /* FIX 4 */
  S->lift = 1;
  memset(&U, 0, sizeof U);
  running = 0;
  show_message_str("MEMORY LOST");
}

int hp41_attach_state(void *mem, int restore) {
  S = (state_t *)mem;
  memset(&U, 0, sizeof U);
  running = 0;
  if (restore && S->magic == STATE_MAGIC && S->version == STATE_VERSION && S->size_check == sizeof(state_t)
      && S->plen >= 0 && S->plen <= PROGMAX && S->size >= 0 && S->size <= NREGS) {
    if (S->pc_off > S->plen) goto_off(S->plen);
    return 1;
  }
  hp41_master_clear();
  return 0;
}

void hp41_wakeup(void) {
  if (!S) return;
  U.off_req = 0;
  if (FL_GET(FLG_AUTOEXEC)) {   /* flag 11: run at power on */
    FL_CLR(FLG_AUTOEXEC);
    start_running();
  }
}

/* ======================= text import / export ======================= */
static int parse_arg(int argt, const char *s, int *arg) {
  int ind = 0;
  while (*s == ' ') s++;
  if (!strncmp(s, "IND ", 4)) { ind = ARG_IND; s += 4; while (*s == ' ') s++; }
  if (!strncmp(s, "ST ", 3)) s += 3;
  if (s[0] && !s[1] && strchr("TZYXL", s[0]) && argt != A_DIG) {
    *arg = (int)(strchr("TZYXL", s[0]) - "TZYXL") + ARG_ST_T + ind; return 1;
  }
  if ((argt == A_LBL || argt == A_GOTO) && s[0] && !s[1]) {
    if (s[0] >= 'A' && s[0] <= 'J') { *arg = ARG_LBL_A + s[0] - 'A'; return 1; }
    if (s[0] >= 'a' && s[0] <= 'e') { *arg = ARG_LBL_a + s[0] - 'a'; return 1; }
  }
  int v = 0, nd = 0;
  while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); nd++; }
  while (*s == ' ') s++;
  if (!nd || *s || v > 111) return 0;
  *arg = v | ind;
  return 1;
}

static int quoted(const char *s, uint8_t *o, int max) {
  /* "text" -> o; supports |- as append via leading '|-' or '>' */
  const char *e = strrchr(s, '"');
  if (*s != '"' || e == s) return -1;
  int n = 0;
  for (const char *p = s + 1; p < e && n < max; p++) o[n++] = (uint8_t)*p;
  return n;
}

int hp41_import_line(const char *line, char *errbuf, int errlen) {
  char t[80];
  int n = 0;
  while (*line == ' ' || *line == '\t') line++;
  /* skip leading line number */
  const char *p = line;
  while (*p >= '0' && *p <= '9') p++;
  if (p != line && (*p == ' ' || *p == '\t')) { line = p; while (*line == ' ' || *line == '\t') line++; }
  while (line[n] && line[n] != '\n' && line[n] != '\r' && n < 79) { t[n] = line[n]; n++; }
  t[n] = 0;
  while (n > 0 && (t[n-1] == ' ' || t[n-1] == '\t')) t[--n] = 0;
  if (n == 0 || t[0] == '#' || t[0] == ';') return 1;
  if (!S) return 0;
  uint8_t code[40]; int clen = 0;
  uint8_t str[24]; int sl;
  err = 0;
  if (!strcmp(t, ".END.")) return 1;
  if (t[0] == '"' || (t[0] == '|' && t[1] == '-' && t[2] == '"') || (t[0] == '>' && t[1] == '"')) {
    int app = t[0] != '"';
    const char *q = t + (t[0] == '|' ? 2 : app ? 1 : 0);
    sl = quoted(q, str + app, 15);
    if (sl < 0) { snprintf(errbuf, errlen, "bad text"); return 0; }
    if (!app && sl >= 2 && str[0] == '|' && str[1] == '-') { str[0] = CH_APPEND; memmove(str + 1, str + 2, sl - 2); sl--; app = 0; }
    if (app) { str[0] = CH_APPEND; sl++; }
    code[0] = OP_TEXT; code[1] = (uint8_t)sl; memcpy(code + 2, str, sl); clen = sl + 2;
  } else if (num_parse(t, &(double){0})) {
    code[0] = OP_NUM; code[1] = (uint8_t)n; memcpy(code + 2, t, n); clen = n + 2;
  } else if (!strcmp(t, "END")) {
    code[0] = OP_END; clen = 1;
  } else {
    char name[16]; int k = 0;
    const char *q = t;
    while (*q && *q != ' ' && k < 15) name[k++] = *q++;
    name[k] = 0;
    while (*q == ' ') q++;
    /* names with Greek letters written in ASCII */
    uint8_t nm[16]; int nl = 0;
    for (int i = 0; name[i]; i++) nm[nl++] = (uint8_t)name[i];
    int op = lookup_fn(nm, nl);
    if (op <= 0) {
      if (!strcmp(name, "SUM+") || !strcmp(name, "S+")) op = O_SIGP;
      else { snprintf(errbuf, errlen, "unknown: %s", name); return 0; }
    }
    if ((op == O_LBL || op == O_GTO || op == O_XEQ) && *q == '"') {
      sl = quoted(q, str, 7);
      if (sl <= 0) { snprintf(errbuf, errlen, "bad label"); return 0; }
      code[0] = op == O_LBL ? OP_LBLA : op == O_GTO ? OP_GTOA : OP_XEQA;
      code[1] = (uint8_t)sl; memcpy(code + 2, str, sl); clen = sl + 2;
    } else if (FN[op].argt != A_NONE) {
      int a;
      if (FN[op].flags & F_NP) { snprintf(errbuf, errlen, "not programmable: %s", name); return 0; }
      if (!parse_arg(FN[op].argt, q, &a)) { snprintf(errbuf, errlen, "bad argument: %s", t); return 0; }
      code[0] = (uint8_t)op; code[1] = (uint8_t)a; clen = 2;
    } else {
      if (*q) { snprintf(errbuf, errlen, "unexpected: %s", q); return 0; }
      if (FN[op].flags & F_NP) { snprintf(errbuf, errlen, "not programmable: %s", name); return 0; }
      code[0] = (uint8_t)op; clen = 1;
      if (op == O_END) code[0] = OP_END;
    }
  }
  if (!insert_instr(code, clen)) { snprintf(errbuf, errlen, "memory full"); err = 0; return 0; }
  return 1;
}

int hp41_export_program(int (*out)(void *ctx, const char *line), void *ctx) {
  int start = S->cur_start;
  int end = prog_end_of(start);
  int line = 1;
  for (int off = start; off <= end && off <= S->plen; off += ilen(off), line++) {
    uint8_t b[48]; int n;
    char t[80];
    int op = off < S->plen ? S->prog[off] : OP_END;
    if (off >= S->plen) { n = 0; t[0] = 0; strcpy(t, "END"); }
    else if (op == OP_TEXT || op == OP_LBLA || op == OP_GTOA || op == OP_XEQA) {
      int len = S->prog[off + 1];
      const uint8_t *s = S->prog + off + 2;
      const char *pre = op == OP_TEXT ? "" : op == OP_LBLA ? "LBL " : op == OP_GTOA ? "GTO " : "XEQ ";
      int k = 0;
      k += snprintf(t, sizeof t, "%s", pre);
      if (op == OP_TEXT && len && s[0] == CH_APPEND) { t[k++] = '|'; t[k++] = '-'; s++; len--; }
      t[k++] = '"';
      for (int i = 0; i < len && k < 70; i++) t[k++] = (char)s[i];
      t[k++] = '"'; t[k] = 0;
    } else {
      n = instr_text(off, b);
      int k = 0;
      for (int i = 0; i < n && k < 70; i++) {
        uint8_t c = b[i];
        if (c == CH_SIGMA) { t[k++] = 'S'; t[k++] = 'U'; t[k++] = 'M'; }
        else if (c == CH_NE) { t[k++] = '#'; }
        else if (c == CH_LE) { t[k++] = '<'; t[k++] = '='; }
        else t[k++] = (char)c;
      }
      t[k] = 0;
    }
    char l[96];
    snprintf(l, sizeof l, "%02d %s", line, t);
    if (!out(ctx, l)) return 0;
    if (off >= S->plen || op == OP_END) break;
  }
  return 1;
}

/* table entries for CAT and ASN when executed with an argument */
void f_cat(int a) { cat_start(a); }
void f_asn(int a) { (void)a; prompt_open(O_ASN); }

int  hp41_shift_active(void) { return U.shift; }
void hp41_clear_shift(void) { U.shift = 0; }
void hp41_set_lowbat(int low) { if (!S) return; if (low) FL_SET(FLG_LOWBAT); else FL_CLR(FLG_LOWBAT); }
int  hp41_wants_off(void) { int r = U.off_req; U.off_req = 0; return r; }
void hp41_goto_new_program(void) { terminate_entries(); prompt_close(); gto_dotdot(); }
