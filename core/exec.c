/* Program execution, labels, GTO/XEQ/RTN */
#include <math.h>
#include "core.h"

int running;
int sst_mode;
extern int skip_next;
void ui_prompt_fn(int op);  /* ui.c: open the prompt of a function that needs an argument */
int  ui_in_prgm_keyboard(void);

static int name_eq(const char *fname, const uint8_t *name, int len) {
  int n = (int)strlen(fname);
  return n == len && !memcmp(fname, name, len);
}

int lookup_fn(const uint8_t *name, int len) {
  for (int i = 1; i < FN_COUNT; i++)
    if (name_eq(FN[i].name, name, len)) return i;
  /* a few friendly aliases */
  static const struct { const char *a; int op; } al[] = {
    { "ENTER", O_ENTER }, { "X<>Y", O_SWAP }, { "R/S", O_STOP }, { "SUM+", O_SIGP }, { "SUM-", O_SIGM },
    { "S+", O_SIGP }, { "S-", O_SIGM }, { "CLS", 51 }, { "SREG", 52 }, { "X#Y?", 54 }, { "X<=Y?", 56 },
    { "X#0?", 59 }, { "CLSUM", 51 }, { "SUMREG", 52 }, { "X<=0?", 61 }, { "X!=Y?", 54 }, { "X!=0?", 59 },
  };
  for (unsigned i = 0; i < sizeof al / sizeof al[0]; i++)
    if (name_eq(al[i].a, name, len)) return al[i].op;
  return -1;
}

static void push_return(void) {
  if (S->rsp == RSTK_MAX) {
    memmove(&S->rstk[0], &S->rstk[1], sizeof(S->rstk[0]) * (RSTK_MAX - 1));
    S->rsp--;
  }
  S->rstk[S->rsp].start = S->cur_start;
  S->rstk[S->rsp].off = S->pc_off;
  S->rstk[S->rsp].line = S->pc_line;
  S->rsp++;
}

void start_running(void) { running = 1; }
void stop_running(void) { running = 0; }

/* jump (GTO) or call (XEQ) to the label at offset off */
static void transfer(int off, int call) {
  if (running) {
    if (call) push_return();
    goto_off(off);
    return;
  }
  /* keyboard */
  S->rsp = 0;
  goto_off(off);
  if (call) start_running();
}

static int label_target(int arg, int *off) {
  if (arg & ARG_IND) {
    int ok;
    val_t *p = reg_ref(arg & 0x7f, &ok);
    if (!ok) return 0;
    if (p->isalpha) {
      int o = find_global_label(p->s, p->len);
      if (o < 0) { raise_err(E_NONEX); return 0; }
      *off = o; return 1;
    }
    double v = fabs(p->x);
    if (v >= 100) { raise_err(E_NONEX); return 0; }
    arg = (int)v;
  }
  int o = find_local_label(arg);
  if (o < 0) { raise_err(E_NONEX); return 0; }
  *off = o;
  return 1;
}

void f_lbl(int a) { (void)a; }
void f_gto(int a) { int off; if (label_target(a, &off)) transfer(off, 0); }
void f_xeq(int a) { int off; if (label_target(a, &off)) transfer(off, 1); }

int xeq_name(const uint8_t *name, int len, int is_gto) {
  int off = find_global_label(name, len);
  if (off >= 0) { transfer(off, !is_gto); return 1; }
  if (!is_gto) {
    int op = lookup_fn(name, len);
    if (op > 0) {
      if (FN[op].argt != A_NONE) {
        if (running) { raise_err(E_NONEX); return 0; }
        ui_prompt_fn(op);
        return 1;
      }
      exec_op(op, 0, 0, 0);
      return 1;
    }
  }
  raise_err(E_NONEX);
  return 0;
}

void f_rtn(int a) {
  (void)a;
  if (!running) { S->rsp = 0; S->pc_line = 0; S->pc_off = S->cur_start; return; }
  if (S->rsp > 0) {
    S->rsp--;
    S->cur_start = S->rstk[S->rsp].start;
    S->pc_off = S->rstk[S->rsp].off;
    S->pc_line = S->rstk[S->rsp].line;
    return;
  }
  stop_running();
}

void f_end(int a) {
  (void)a;
  if (running && S->rsp > 0) { f_rtn(0); return; }
  stop_running();
  S->pc_line = 0;
  S->pc_off = S->cur_start;
}

void f_stop(int a) { (void)a; stop_running(); }

void f_prompt(int a) {
  (void)a;
  show_message(S->alpha, S->alen);
  stop_running();
}

void f_del(int n) {
  /* delete n lines starting at current line (PRGM mode) */
  if (S->pc_line == 0) { S->pc_line = 1; S->pc_off = S->cur_start; }
  int line = S->pc_line;
  for (int i = 0; i < n; i++) {
    if (S->pc_off >= S->plen || S->prog[S->pc_off] == OP_END) break;
    int l = ilen(S->pc_off);
    memmove(S->prog + S->pc_off, S->prog + S->pc_off + l, S->plen - S->pc_off - l);
    S->plen -= l;
  }
  S->rsp = 0;
  S->pc_line = line - 1;
  S->pc_off = line_to_off(S->cur_start, line - 1 < 1 ? 1 : line - 1);
}

void clp_name(const uint8_t *name, int len) {
  int start;
  if (len == 0) start = S->cur_start;
  else {
    int off = find_global_label(name, len);
    if (off < 0) { raise_err(E_NONEX); return; }
    start = prog_start_of(off);
  }
  int end = prog_end_of(start);
  if (end < S->plen) end++;            /* remove its END too */
  else if (start > 0) start--;         /* last program: remove previous END */
  memmove(S->prog + start, S->prog + end, S->plen - end);
  S->plen -= end - start;
  S->rsp = 0;
  goto_off(start > S->plen ? S->plen : start);
  S->pc_line = 0;
  S->pc_off = S->cur_start;
}
void f_clp(int a) { (void)a; clp_name(0, 0); }

void exec_op(int op, int arg, const uint8_t *str, int slen) {
  switch (op) {
    case OP_NUM: {
      char t[32];
      int n = slen < 31 ? slen : 31;
      memcpy(t, str, n); t[n] = 0;
      double v;
      if (!num_parse(t, &v)) { raise_err(E_DATA); return; }
      push_num(v);
      return;
    }
    case OP_TEXT:
      if (slen > 0 && str[0] == CH_APPEND) alpha_append(str + 1, slen - 1);
      else { alpha_clear(); alpha_append(str, slen); }
      return;
    case OP_LBLA: return;
    case OP_GTOA: xeq_name(str, slen, 1); return;
    case OP_XEQA: xeq_name(str, slen, 0); return;
    case OP_END:  f_end(0); return;
  }
  if (op > 0 && op < FN_COUNT) FN[op].fn(arg);
}

/* execute one program step at the program pointer (running context) */
void exec_step(void) {
  if (S->pc_line == 0) { S->pc_line = 1; S->pc_off = S->cur_start; }
  int off = S->pc_off;
  if (off >= S->plen || S->prog[off] == OP_END) {
    f_end(0);
    return;
  }
  int op = S->prog[off];
  int arg = 0;
  const uint8_t *str = 0; int slen = 0;
  if (op == OP_NUM || op == OP_TEXT || op == OP_LBLA || op == OP_GTOA || op == OP_XEQA) {
    slen = S->prog[off + 1]; str = S->prog + off + 2;
  } else if (op < FN_COUNT && FN[op].argt != A_NONE) arg = S->prog[off + 1];
  /* advance first so that XEQ pushes the following line */
  S->pc_off = off + ilen(off);
  S->pc_line++;
  int saved_line = S->pc_line - 1, saved_start = S->cur_start;
  skip_next = 0;
  err = 0;
  exec_op(op, arg, str, slen);
  if (err) {
    if (FL_GET(FLG_ERRIGN)) { FL_CLR(FLG_ERRIGN); err = 0; skip_next = 0; return; }
    stop_running();
    S->cur_start = saved_start;
    S->pc_line = saved_line;
    S->pc_off = off;
    ui_after_error();
    return;
  }
  if (skip_next) {
    skip_next = 0;
    if (S->pc_off < S->plen && S->prog[S->pc_off] != OP_END) {
      S->pc_off += ilen(S->pc_off);
      S->pc_line++;
    }
  }
}
