# Generates examples/SELFTEST.txt: an HP-41 program that tests the built-in
# functions and stops with "<name> ERR" when a result is wrong.
#
# Test kinds
#   num(name, setup, expected)      setup leaves the result in X; checked with a
#                                   relative tolerance of 1E-8 (absolute 1E-9)
#   cond(name, setup, test, truth)  setup builds the stack, test is a conditional
#                                   instruction; truth = expected outcome
#   flag(name, setup, truth)        setup ends with a test of a condition (FS? etc.)
#   alpha(name, setup, text)        setup leaves ALPHA; its first 6 chars are compared
#   error(name, setup)              setup must raise an error (caught with flag 25)
#
# Registers used: R20-R29, statistics in R30-R35 (SIZE >= 036). Flags 01, 02, 08,
# 10 are used and cleared. Display mode, angle mode, SIGMA REG and DMY/MDY are
# restored at the end.

import os

def footer():
    L("FIX 0", "CF 29", "DEG", "CF 10",
      "FS?C 08", "DMY",
      "RCL 23", "X=0?", "GTO 02",
      '"FAILS: "', "ARCL 23", "GTO 03",
      "LBL 02",
      '"ALL OK "', "ARCL 22",
      "LBL 03", "FIX 4", "FS?C 09", "SF 29", "RCL 23", "X#0?", "BEEP", "AVIEW", "RTN",
      # ---- helpers ----
      "LBL 99",                 # Y = computed, X = expected
      "ISG 28", "STO 21", "RDN", "STO 20", "RCL 21", "-", "ABS",
      "RCL 21", "ABS", "1E-8", "*", "1E-9", "+",
      "X<Y?", "GTO 98",
      "LBL 97", "1", "ST+ 22", "RTN",
      "LBL 98", "1", "ST+ 23", '"ERR "', "RCL 28", "INT", "FIX 0", "CF 29", "ARCL X", "FIX 4", "FS? 09", "SF 29",
      "TONE 9", "PROMPT", "RTN",
      "LBL 96", "ISG 28", "FS?C 10", "GTO 97", "GTO 98",
      "LBL 95", "ISG 28", "FS?C 10", "GTO 98", "GTO 97",
      "LBL 94", "ISG 28", "FS?C 25", "GTO 98", "GTO 97",
      "LBL A", "8", "RTN",
      "LBL 50", "7", "RTN",
      "LBL 52", "GTO IND 24", "LBL 51", "9", "RTN",
      "END")



lines = []
tests = 0
index = []          # (number, name) for the README / listing header

def T(name):
    global tests
    tests += 1
    index.append((tests, name))
    lines.append(f"; {tests:3d} {name}")

def L(*xs):
    lines.extend(xs)

def num(name, setup, expected):
    T(name)
    L(*setup.split(";"), expected, "XEQ 99")

def cond(name, setup, test, truth):
    T(name)
    L("CF 10", *setup.split(";"), test, "SF 10", "XEQ 96" if truth else "XEQ 95")

def flag(name, setup, truth):
    T(name)
    L("CF 10", *setup.split(";"), "SF 10", "XEQ 96" if truth else "XEQ 95")

def alpha(name, setup, text):
    T(name)
    L(*setup.split(";"), "ASTO 26", f'"{text}"', "ASTO 27", "CF 10", "RCL 26", "RCL 27", "X=Y?", "SF 10", "XEQ 96")

def error(name, setup):
    T(name)
    L("SF 25", *setup.split(";"), "XEQ 94")

def header(label, first, title):
    L(f'; HP-41CV self test, part {label[-1]}: {title}',
      f'; XEQ "{label}". Needs SIZE 036 or more. Uses R20-R35 and flags 01 02 08 09 10.',
      '; A failed test stops with "ERR nnn": look up nnn in the comments',
      '; below, then press R/S to continue. At the end the display shows',
      '; "ALL OK n" (tests passed) or "FAILS: n".',
      f'LBL "{label}"',
      "SF 25", "RCL 35", "FS?C 25", "GTO 01", '"SIZE >= 036"', "PROMPT", "RTN",
      "LBL 01",
      "0", "STO 22", "STO 23", f"{first - 1}.999", "STO 28",
      "CF 08", "FS? 31", "SF 08",          # remember DMY
      "CF 09", "FS? 29", "SF 09",          # remember digit grouping
      "CF 01", "CF 02", "CF 10", "CF 24", "CF 25",
      "FIX 4", "DEG", "MDY")

header("TEST1", 1, "arithmetic, math, conversions, stack, registers")

# ---------------- arithmetic ----------------
num("+", "2;ENTER^;3;+", "5")
num("-", "2;ENTER^;3;-", "-1")
num("*", "6;ENTER^;7;*", "42")
num("/", "1;ENTER^;8;/", ".125")
num("CHS", "5;CHS", "-5")
num("ABS", "-3;ABS", "3")
num("INT", "-2.7;INT", "-2")
num("FRC", "-2.75;FRC", "-.75")
num("RND", "FIX 2;3.14159;RND;FIX 4", "3.14")
num("RND SCI", "SCI 2;123456;RND;FIX 4", "123000")
num("SIGN", "-7;SIGN", "-1")
num("SIGN 0", "0;SIGN", "1")
num("1/X", "4;1/X", ".25")
num("SQRT", "2;SQRT", "1.414213562")
num("X^2", "12;X^2", "144")
num("Y^X", "2;ENTER^;10;Y^X", "1024")
num("Y^X NEG", "-2;ENTER^;3;Y^X", "-8")
num("LN", "10;LN", "2.302585093")
num("LOG", "1000;LOG", "3")
num("E^X", "1;E^X", "2.718281828")
num("10^X", "2.5;10^X", "316.2277660")
num("LN1+X", "1E-5;LN1+X", "9.999950000E-6")
num("E^X-1", "1E-5;E^X-1", "1.000005000E-5")
num("FACT", "10;FACT", "3628800")
num("FACT 0", "0;FACT", "1")
num("MOD", "7;ENTER^;3;MOD", "1")
num("MOD NEG", "-7;ENTER^;3;MOD", "2")
num("%", "200;ENTER^;15;%", "30")
num("%CH", "50;ENTER^;75;%CH", "50")
num("PI", "PI", "3.141592654")

# ---------------- trigonometry ----------------
num("SIN", "30;SIN", ".5")
num("COS", "60;COS", ".5")
num("TAN", "45;TAN", "1")
num("SIN 180", "180;SIN", "0")
num("ASIN", ".5;ASIN", "30")
num("ACOS", ".5;ACOS", "60")
num("ATAN", "1;ATAN", "45")
num("SIN RAD", "RAD;PI;6;/;SIN;DEG", ".5")
num("ATAN RAD", "RAD;1;ATAN;DEG", ".7853981634")
num("SIN GRAD", "GRAD;100;SIN;DEG", "1")
num("ACOS GRAD", "GRAD;0;ACOS;DEG", "100")

# ---------------- conversions ----------------
num("D-R", "180;D-R", "3.141592654")
num("R-D", "PI;R-D", "180")
num("HMS", "1.5;HMS", "1.3")
num("HR", "1.3;HR", "1.5")
num("HMS+", "1.3;ENTER^;2.45;HMS+", "4.15")
num("HMS-", "4.15;ENTER^;2.45;HMS-", "1.3")
num("P-R X", "30;ENTER^;2;P-R;STO 24;RDN;STO 25;RCL 24", "1.732050808")
num("P-R Y", "RCL 25", "1")
num("R-P X", "3;ENTER^;4;R-P;STO 24;RDN;STO 25;RCL 24", "5")
num("R-P Y", "RCL 25", "36.86989765")
num("OCT", "64;OCT", "100")
num("DEC", "100;DEC", "64")

# ---------------- stack ----------------
num("ENTER^", "5;ENTER^;+", "10")
num("X<>Y", "1;ENTER^;2;X<>Y", "1")
num("RDN", "1;ENTER^;2;ENTER^;3;ENTER^;4;RDN", "3")
num("CLX", "3;ENTER^;5;CLX;7;+", "10")
num("LASTX", "3;ENTER^;4;+;LASTX", "4")
num("RCL Z", "1;ENTER^;2;ENTER^;3;RCL Z", "1")
num("ST+ X", "9;ST+ X", "18")

# ---------------- registers ----------------
num("STO RCL", "42;STO 24;CLX;RCL 24", "42")
num("ST+", "5;STO 24;3;ST+ 24;RCL 24", "8")
num("ST-", "5;STO 24;3;ST- 24;RCL 24", "2")
num("ST*", "5;STO 24;3;ST* 24;RCL 24", "15")
num("ST/", "5;STO 24;4;ST/ 24;RCL 24", "1.25")
num("X<>", "1;STO 24;2;X<> 24", "1")
num("X<> REG", "RCL 24", "2")
num("STO IND", "24;STO 25;77;STO IND 25;RCL 24", "77")
num("RCL IND", "RCL IND 25", "77")
num("ISG VALUE", "1.002;STO 24;ISG 24;CLD;RCL 24", "2.002")
flag("ISG NOSKIP", "1.002;STO 24;ISG 24", True)
flag("ISG SKIP", "2.002;STO 24;ISG 24", False)
flag("DSE NOSKIP", "3.001;STO 24;DSE 24", True)
flag("DSE SKIP", "2.001;STO 24;DSE 24", False)
num("VIEW", "13;STO 24;VIEW 24;CLD;RCL 24", "13")

footer()
part1 = lines; lines = []
header("TEST2", tests + 1, "flags, tests, alpha, statistics, errors, time")

# ---------------- flags ----------------
flag("SF FS?", "SF 01;FS? 01", True)
flag("CF FC?", "CF 01;FC? 01", True)
flag("FS? CLR", "CF 01;FS? 01", False)
flag("FS?C", "SF 01;FS?C 01", True)
flag("FS?C CLRS", "FS? 01", False)
flag("FC?C", "SF 02;FC?C 02", False)
flag("FC?C CLRS", "FC? 02", True)
flag("SF IND", "2;STO 24;SF IND 24;FS?C 02", True)

# ---------------- conditionals ----------------
for name, test, a, b, truth in [
    ("X=Y? T", "X=Y?", 2, 2, True), ("X=Y? F", "X=Y?", 2, 3, False),
    ("X#Y? T", "X#Y?", 2, 3, True), ("X#Y? F", "X#Y?", 2, 2, False),
    ("X<Y? T", "X<Y?", 3, 2, True), ("X<Y? F", "X<Y?", 2, 3, False),
    ("X<=Y? T", "X<=Y?", 2, 2, True), ("X<=Y? F", "X<=Y?", 2, 3, False),
    ("X>Y? T", "X>Y?", 2, 3, True), ("X>Y? F", "X>Y?", 3, 2, False)]:
    cond(name, f"{a};ENTER^;{b}", test, truth)
for name, test, x, truth in [
    ("X=0? T", "X=0?", 0, True), ("X=0? F", "X=0?", 1, False),
    ("X#0? T", "X#0?", 1, True), ("X#0? F", "X#0?", 0, False),
    ("X<0? T", "X<0?", -1, True), ("X<0? F", "X<0?", 0, False),
    ("X<=0? T", "X<=0?", 0, True), ("X<=0? F", "X<=0?", 1, False),
    ("X>0? T", "X>0?", 1, True), ("X>0? F", "X>0?", 0, False)]:
    cond(name, f"{x}", test, truth)

# ---------------- alpha ----------------
alpha("ARCL", "CLA;12;ARCL X", "12.000")
alpha("APPEND", '"AB";|-"CD"', "ABCD")
alpha("ASHF", '"ABCDEFGH";ASHF', "GH")
alpha("CLA", 'CLA;|-"XY"', "XY")
flag("AON", "AON;FS? 48", True)
flag("AOFF", "AOFF;FS? 48", False)
cond("ALPHA X=Y?", '"ABC";ASTO 26;RCL 26;RCL 26', "X=Y?", True)

# ---------------- statistics ----------------
L("SUMREG 30", "CLSUM", "2", "ENTER^", "1", "SUM+", "4", "ENTER^", "3", "SUM+", "6", "ENTER^", "5", "SUM+")
num("SUM+ N", "RCL 35", "3")
num("MEAN X", "MEAN", "3")
num("MEAN Y", "MEAN;X<>Y", "4")
num("SDEV X", "SDEV", "2")
num("SDEV Y", "SDEV;X<>Y", "2")
num("SUM-", "6;ENTER^;5;SUM-;RCL 35", "2")
num("CLSUM", "CLSUM;RCL 30", "0")
L("SUMREG 11")

# ---------------- program control ----------------
num("XEQ LOCAL", "XEQ A", "8")
num("XEQ IND", "50;STO 24;XEQ IND 24", "7")
num("GTO IND", "51;STO 24;XEQ 52", "9")

# ---------------- errors (flag 25) ----------------
error("ERR 1/X", "0;1/X")
error("ERR /0", "1;ENTER^;0;/")
error("ERR LN", "0;LN")
error("ERR SQRT", "-1;SQRT")
error("ERR ASIN", "2;ASIN")
error("ERR FACT", "70;FACT")
error("ERR RANGE", "1E99;ENTER^;10;*")
error("ERR ALPHA", '"AB";ASTO 26;RCL 26;1;+')
error("ERR NONEX", "XEQ 77")
error("ERR SF 30", "SF 30")
num("RANGE IGN", "SF 24;1E99;ENTER^;10;*;CF 24", "9.999999999E99")

# ---------------- CX time module ----------------
num("DATE+", "11.102026;ENTER^;50;DATE+", "12.302026")
num("DDAYS", "1.012026;ENTER^;11.102026;DDAYS", "313")
num("DOW", "11.102026;DOW", "2")
alpha("ADATE", "CLA;11.102026;ADATE", "11/10/")
alpha("ATIME24", "CLA;13.4512;ATIME24", "13:45:")
num("DMY DATE+", "DMY;10.112026;ENTER^;50;DATE+;MDY", "30.122026")

footer()
part2 = lines
for name, part in (("SELFTST1.txt", part1), ("SELFTST2.txt", part2)):
    with open(os.path.join(os.path.dirname(__file__), "..", "examples", name), "w") as f:
        f.write("\n".join(part) + "\n")
    print(name, len([l for l in part if not l.startswith(";")]), "lines")
print(tests, "tests")
