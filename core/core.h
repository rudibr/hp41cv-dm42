/* HP-41CV clean-room core - internal definitions */
#ifndef HP41_CORE_H
#define HP41_CORE_H

#include <stdint.h>
#include <string.h>
#include "hp41.h"

#define NREGS      319      /* 41CV: 319 registers shared by data, programs, key assignments */
#define PROGMAX    2400     /* bytes of program storage (more than 319*7 HP bytes) */
#define MAXASN     80
#define ALPHA_MAX  24
#define RSTK_MAX   6
#define STATE_MAGIC   0x34314356u  /* "41CV" */
#define STATE_VERSION 3

/* stack indices */
enum { SX = 0, SY, SZ, ST, SL };

typedef struct {
  double x;
  uint8_t isalpha;
  uint8_t len;
  uint8_t s[6];
} val_t;

typedef struct {
  int16_t key;     /* 41-style keycode: row*10+col, negative = shifted */
  uint8_t kind;    /* 1 = built-in function, 2 = global label */
  uint8_t op;
  char name[8];
} asn_t;

/* ---- argument encoding (one byte, like the 41) ---- */
#define ARG_IND     0x80
#define ARG_ST_T    112
#define ARG_ST_Z    113
#define ARG_ST_Y    114
#define ARG_ST_X    115
#define ARG_ST_L    116
#define ARG_LBL_A   102   /* A..J = 102..111 */
#define ARG_LBL_a   123   /* a..e = 123..127 */

/* ---- argument types ---- */
enum {
  A_NONE = 0,
  A_REG,     /* 2 digits / IND / stack */
  A_DIG,     /* 1 digit, IND allowed */
  A_FLAG,    /* 2 digits, IND */
  A_LBL,     /* LBL nn / A-J / alpha */
  A_GOTO,    /* GTO/XEQ nn / letter / IND / alpha */
  A_SIZE,    /* 3 digits */
  A_DEL,     /* 3 digits */
  A_CAT,     /* 1 digit */
  A_ASN
};

/* function flags */
#define F_NP    1   /* not programmable */
#define F_HIDE  2   /* not shown in CAT 3 */

typedef struct {
  const char *name;   /* 41 name (with special chars as \x82 etc.) */
  uint8_t argt;
  uint8_t flags;
  void (*fn)(int arg);
} fdef_t;

/* indices into FN[] used by the code (checked by the self test) */
enum {
  O_ADD = 1, O_SUB = 2, O_MUL = 3, O_DIV = 4, O_CHS = 5,
  O_INV = 11, O_SQRT = 12, O_X2 = 13, O_YX = 14, O_LN = 15, O_LOG = 16, O_EX = 17, O_10X = 18,
  O_SIN = 21, O_COS = 22, O_TAN = 23, O_ASIN = 24, O_ACOS = 25, O_ATAN = 26,
  O_PCT = 29, O_PI = 31, O_ENTER = 42, O_SWAP = 43, O_RDN = 44, O_CLX = 45, O_LASTX = 46,
  O_SIGP = 47, O_SIGM = 48, O_STO = 69, O_RCL = 70, O_STADD = 71, O_XEX = 75,
  O_LBL = 93, O_GTO = 94, O_XEQ = 95, O_RTN = 96, O_STOP = 97,
  O_END = 105, O_SIZE = 106, O_DEL = 108, O_CAT = 109, O_CLP = 110, O_ASN = 111
};

/* special opcodes (not in function table lookup by name) */
enum {
  OP_NULL = 0,
  OP_NUM = 250,    /* number literal:  OP_NUM len chars */
  OP_TEXT = 251,   /* text literal:    OP_TEXT len chars (first may be CH_APPEND) */
  OP_LBLA = 252,   /* global label:    OP_LBLA len chars */
  OP_GTOA = 253,   /* GTO alpha */
  OP_XEQA = 254,   /* XEQ alpha */
  OP_END = 255
};

typedef struct {
  uint32_t magic, version, size_check;
  val_t stk[5];
  val_t reg[NREGS];
  int32_t size;                /* number of data registers */
  uint8_t alpha[ALPHA_MAX];
  int32_t alen;
  uint8_t flags[8];
  uint8_t prog[PROGMAX];
  int32_t plen;
  int32_t cur_start;           /* offset of first instruction of current program */
  int32_t pc_line;             /* current line number, 0 = top */
  int32_t pc_off;              /* offset of instruction at pc_line (line 1 if pc_line==0) */
  struct { int32_t start, off, line; } rstk[RSTK_MAX];
  int32_t rsp;
  asn_t asn[MAXASN];
  int32_t nasn;
  uint8_t lift;                /* stack lift enabled */
  int32_t sreg;                /* SIGMA REG base */
} state_t;

extern state_t *S;
extern const fdef_t FN[];
extern const int FN_COUNT;
extern const hp41_platform_t *PLAT;

/* ---- flags ---- */
#define FL_GET(n) ((S->flags[(n)>>3] >> ((n)&7)) & 1)
#define FL_SET(n) (S->flags[(n)>>3] |= (uint8_t)(1u << ((n)&7)))
#define FL_CLR(n) (S->flags[(n)>>3] &= (uint8_t)~(1u << ((n)&7)))
enum {
  FLG_AUTOEXEC = 11, FLG_PRINTER = 21, FLG_NUMIN = 22, FLG_ALPHAIN = 23,
  FLG_RANGE = 24, FLG_ERRIGN = 25, FLG_AUDIO = 26, FLG_USER = 27,
  FLG_RADIX = 28, FLG_GROUP = 29, FLG_FIX = 40, FLG_ENG = 41,
  FLG_GRAD = 42, FLG_RAD = 43, FLG_CONTON = 44, FLG_SHIFT = 47,
  FLG_ALPHA = 48, FLG_LOWBAT = 49, FLG_MSG = 50, FLG_PRGM = 52, FLG_PAUSE = 54
};

/* ---- errors ---- */
enum { E_NONE = 0, E_DATA, E_RANGE, E_NONEX, E_ALPHA, E_NOROOM, E_TRYAGAIN, E_STOP };
extern int err;            /* set by functions */
void raise_err(int e);

/* ---- numbers (num.c) ---- */
double  num_round(double x);                    /* round to 10 digits; sets E_RANGE / clamps */
void    num_decompose(double x, int nd, int64_t *m, int *e);
int     num_format(double x, uint8_t *out);     /* display format string, returns length */
int     num_format_full(double x, uint8_t *out);/* all 10 digits (SHOW) */
int     num_parse(const char *s, double *x);
int     val_format(const val_t *v, uint8_t *out);
void    cells_from_text(const uint8_t *s, int len, hp41_disp_t *d, int start); /* start<0: right aligned */
int     cells_len(const uint8_t *s, int len);

/* ---- values / stack helpers (ops.c) ---- */
void   lift_stack(void);
void   push_num(double x);
void   set_x(double x);
int    need_num(const val_t *v);
double vx(void);
double vy(void);
void   save_lastx(void);
void   drop_set_x(double x);   /* binary op result: Y,X -> X; T copied down */
val_t *reg_ref(int arg, int *ok);
double to_rad(double a);
double from_rad(double a);

/* ---- programs (prog.c) ---- */
int    ilen(int off);
int    prog_end_of(int start);           /* offset of END (or plen) for program starting at start */
int    prog_start_of(int off);           /* start of program containing off */
int    line_to_off(int start, int line);
int    off_to_line(int start, int off);
int    prog_lines(int start);            /* number of lines incl END */
int    insert_instr(const uint8_t *code, int n);  /* after current line; returns 0 on NO ROOM */
void   delete_line(void);
int    find_local_label(int arg);        /* returns offset or -1 */
int    find_global_label(const uint8_t *name, int len); /* offset or -1 */
void   goto_off(int off);
int    instr_text(int off, uint8_t *out);  /* 41-style listing text, returns length */
int    hp_bytes(int off);                /* bytes used by instruction on a real 41 */
int    regs_used_by_programs(void);
int    free_regs(void);
void   gto_dotdot(void);
void   pack_memory(void);
int    is_dot_end(int off);

/* ---- execution (exec.c) ---- */
void   exec_op(int op, int arg, const uint8_t *str, int slen);
void   exec_at(int off);                 /* execute program instruction */
int    lookup_fn(const uint8_t *name, int len);
int    xeq_name(const uint8_t *name, int len, int is_gto);  /* XEQ "name" */
void   start_running(void);
void   stop_running(void);
extern int running;
extern int sst_mode;

/* ---- ui (ui.c) ---- */
void   show_message(const uint8_t *s, int len);
void   show_message_str(const char *s);
void   show_view(const val_t *v);
void   ui_after_error(void);
void   ui_pause(int ms);
void   ui_beep(int freq, int ms);
void   ui_off(void);
void   ui_show_x(void);
void   alpha_append(const uint8_t *s, int len);
void   alpha_clear(void);
extern int msg_active;

#endif
