/* Program memory: encoding, navigation, labels, listing */
#include "core.h"

static int is_str_op(int op) { return op == OP_NUM || op == OP_TEXT || op == OP_LBLA || op == OP_GTOA || op == OP_XEQA; }

int is_dot_end(int off) { return off >= S->plen; }

int ilen(int off) {
  if (off >= S->plen) return 0;
  int op = S->prog[off];
  if (is_str_op(op)) return 2 + S->prog[off + 1];
  if (op == OP_END) return 1;
  if (op < FN_COUNT && FN[op].argt != A_NONE) return 2;
  return 1;
}

int prog_end_of(int start) {
  int off = start;
  while (off < S->plen && S->prog[off] != OP_END) off += ilen(off);
  return off;
}

int prog_start_of(int off) {
  int start = 0, p = 0;
  while (p < S->plen && p < off) {
    if (S->prog[p] == OP_END) start = p + 1;
    p += ilen(p);
  }
  if (off > S->plen) return start;
  return start;
}

int line_to_off(int start, int line) {
  int off = start;
  for (int i = 1; i < line && off < S->plen; i++) {
    if (S->prog[off] == OP_END) break;
    off += ilen(off);
  }
  return off;
}

int off_to_line(int start, int off) {
  int line = 1, p = start;
  while (p < off && p < S->plen) { p += ilen(p); line++; }
  return line;
}

int prog_lines(int start) {
  return off_to_line(start, prog_end_of(start));
}

int hp_bytes(int off) {
  if (off >= S->plen) return 0;
  int op = S->prog[off];
  int a = off + 1 < S->plen ? S->prog[off + 1] : 0;
  switch (op) {
    case OP_NUM:  return a;
    case OP_TEXT: return 1 + a;
    case OP_LBLA: return 4 + a;
    case OP_GTOA: case OP_XEQA: return 2 + a;
    case OP_END:  return 3;
  }
  if (op >= FN_COUNT) return 1;
  if (FN[op].argt == A_NONE) return 1;
  if ((op == O_STO || op == O_RCL) && a < 16) return 1;
  if (op == O_LBL && a < 15) return 1;
  if (op == O_GTO) return (a & ARG_IND) ? 2 : (a < 15 ? 2 : 3);
  if (op == O_XEQ) return (a & ARG_IND) ? 2 : 3;
  return 2;
}

static int prog_bytes_total(void) {
  int t = 0;
  for (int off = 0; off < S->plen; off += ilen(off)) t += hp_bytes(off);
  return t;
}

int regs_used_by_programs(void) {
  return (prog_bytes_total() + 6) / 7 + (S->nasn + 1) / 2;
}

int free_regs(void) {
  int f = NREGS - S->size - regs_used_by_programs();
  return f < 0 ? 0 : f;
}

void goto_off(int off) {
  if (off > S->plen) off = S->plen;
  S->cur_start = prog_start_of(off);
  S->pc_off = off;
  S->pc_line = off_to_line(S->cur_start, off);
}

/* offset where a new instruction goes, and the line it gets */
static int insert_pos(int *newline) {
  int off;
  if (S->pc_line == 0) { off = S->cur_start; *newline = 1; return off; }
  off = S->pc_off;
  if (off >= S->plen || S->prog[off] == OP_END) { *newline = S->pc_line; return off; }
  *newline = S->pc_line + 1;
  return off + ilen(off);
}

int insert_instr(const uint8_t *code, int n) {
  if (S->plen + n > PROGMAX) { raise_err(E_TRYAGAIN); return 0; }
  int nl;
  int pos = insert_pos(&nl);
  memmove(S->prog + pos + n, S->prog + pos, S->plen - pos);
  memcpy(S->prog + pos, code, n);
  S->plen += n;
  if (NREGS - S->size - regs_used_by_programs() < 0) {
    /* no room: undo */
    memmove(S->prog + pos, S->prog + pos + n, S->plen - pos - n);
    S->plen -= n;
    raise_err(E_TRYAGAIN);
    return 0;
  }
  S->rsp = 0;
  S->pc_off = pos;
  S->pc_line = nl;
  return 1;
}

void delete_line(void) {
  if (S->pc_line == 0 || S->pc_off >= S->plen) return;
  int off = S->pc_off, n = ilen(off);
  memmove(S->prog + off, S->prog + off + n, S->plen - off - n);
  S->plen -= n;
  S->rsp = 0;
  /* move to previous line */
  int line = S->pc_line - 1;
  S->pc_line = line;
  S->pc_off = line_to_off(S->cur_start, line < 1 ? 1 : line);
}

int find_local_label(int arg) {
  int end = prog_end_of(S->cur_start);
  int from = S->pc_line == 0 ? S->cur_start : S->pc_off;
  /* search from current line forward to end, then wrap */
  for (int pass = 0; pass < 2; pass++) {
    int off = pass ? S->cur_start : from;
    int stop = pass ? from : end;
    while (off < stop) {
      if (S->prog[off] == O_LBL && S->prog[off + 1] == arg) return off;
      off += ilen(off);
    }
  }
  return -1;
}

int find_global_label(const uint8_t *name, int len) {
  int found = -1;
  for (int off = 0; off < S->plen; off += ilen(off)) {
    if (S->prog[off] == OP_LBLA && S->prog[off + 1] == len && !memcmp(S->prog + off + 2, name, len))
      found = off;
  }
  return found;
}

void gto_dotdot(void) {
  if (S->plen > 0) {
    int ls = prog_start_of(S->plen);
    if (ls < S->plen) {               /* last program not empty: close it */
      if (S->plen + 1 <= PROGMAX) S->prog[S->plen++] = OP_END;
    }
  }
  S->cur_start = S->plen;
  S->pc_off = S->plen;
  S->pc_line = 0;
  S->rsp = 0;
}

void pack_memory(void) { /* memory is always packed */ }

/* ---- listing ---- */
static int put_s(uint8_t *o, const char *s) { int n = 0; while (s[n]) { o[n] = (uint8_t)s[n]; n++; } return n; }

int arg_text(int argt, int a, uint8_t *o) {
  int n = 0;
  if (a & ARG_IND) { n += put_s(o, "IND "); a &= 0x7f; }
  if (a >= ARG_ST_T && a <= ARG_ST_L && argt != A_DIG) {
    o[n++] = (uint8_t)"TZYXL"[a - ARG_ST_T];
    return n;
  }
  if (argt == A_LBL || argt == A_GOTO) {
    if (a >= ARG_LBL_A && a < ARG_LBL_A + 10) { o[n++] = (uint8_t)('A' + a - ARG_LBL_A); return n; }
    if (a >= ARG_LBL_a && a < ARG_LBL_a + 5) { o[n++] = (uint8_t)('a' + a - ARG_LBL_a); return n; }
  }
  if (argt == A_DIG && !(n)) { o[n++] = (uint8_t)('0' + a % 10); return n; }
  o[n++] = (uint8_t)('0' + (a / 10) % 10);
  o[n++] = (uint8_t)('0' + a % 10);
  return n;
}

int instr_text(int off, uint8_t *o) {
  if (off >= S->plen) return put_s(o, ".END.");
  int op = S->prog[off];
  int n = 0;
  if (is_str_op(op)) {
    int len = S->prog[off + 1];
    const uint8_t *str = S->prog + off + 2;
    switch (op) {
      case OP_NUM:  memcpy(o, str, len); return len;
      case OP_TEXT: o[n++] = CH_TEE; break;
      case OP_LBLA: n += put_s(o, "LBL"); o[n++] = CH_TEE; break;
      case OP_GTOA: n += put_s(o, "GTO "); o[n++] = CH_TEE; break;
      case OP_XEQA: n += put_s(o, "XEQ "); o[n++] = CH_TEE; break;
    }
    memcpy(o + n, str, len);
    return n + len;
  }
  if (op == OP_END) return put_s(o, "END");
  if (op >= FN_COUNT) return put_s(o, "???");
  n = put_s(o, FN[op].name);
  if (FN[op].argt != A_NONE) {
    o[n++] = ' ';
    n += arg_text(FN[op].argt, S->prog[off + 1], o + n);
  }
  return n;
}
