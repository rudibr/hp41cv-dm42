/* keystroke tests; expected strings are the 12-cell display with punctuation inline */
static void run_tests(void) {
  /* --- basics and display formatting --- */
  fresh();
  T("", "0.0000");
  T("2 ENTER 3 +", "5.0000");
  T("1 2 3 4 5 6 7", "1,234,567_");
  T("ENTER", "1,234,567.000");
  T("SH E F1 2 EXIT", "1,234,567.00");
  T("SH E F1 4 EXIT", "1,234,567.000");
  T("0 INV", "DATA ERROR");
  T("BSP", "0.0000");
  T("1 2 3 4 5 SH E F2 3 EXIT", "1.235 04");
  T("SH E F3 2 EXIT", "12.3 03");
  T("SH E F1 4 EXIT", "12,345.0000");
  T("1 2 CHS", "-12_");
  T("ENTER", "-12.0000");
  T("1 E 5", "1 5_");
  T("0", "1 50");
  T("ENTER", "1.0000 50");
  T("1 E CHS 3 ENTER", "0.0010");
  T("SH E F2 4 EXIT", "1.0000-03");
  T("SH E F1 4 EXIT", "0.0010");
  T("BSP 2 SQRT", "1.4142");
  T("SH . ", "1.4142");            /* SHOW released -> normal display */
  T("2 ENTER 3 SH INV", "8.0000");
  T("3 0 SIN", "0.5000");
  T("1 8 0 SIN", "0.0000");
  T("9 0 COS", "0.0000");
  T("SH CHS F2 SH RDN", "3.1416");
  T("SH CHS F1 EXIT", "3.1416");
  T("1 SH SIN", "90.0000");
  T("0 . 0 0 0 0 1", "0.00001_");
  T("ENTER", "1.0000-05");
  T("1 E 9 9 ENTER 1 0 MUL", "OUT OF RANGE");
  T("BSP", "10.0000");
  T("RDN", "1.0000 99");
  T("1 2 . 3 4 5 6 7 8 9 SH E F1 9 EXIT", "12.34567890");
  T("SH E F1 4 EXIT", "12.3457");
  T("5 SH 9 F4", "5.0000");   /* RND */
  T("EXIT 7 ENTER 2 SH 9 F6 EXIT", "1.0000"); /* MOD */
  T("1 0 SH 9 F2 EXIT", "0.0000");  /* FRC */
  T("6 9 SH MUL F1 EXIT", "1.7112 98"); /* FACT 69 */
  T("1 . 3 0 SH 5 F4 EXIT", "1.5000");  /* HR */
  T("SH 5 F3 EXIT", "1.3000");          /* HMS */

  /* --- stack --- */
  fresh();
  T("1 ENTER 2 ENTER 3 ENTER 4", "4_");
  T("RDN", "3.0000");
  T("RDN", "2.0000");
  T("RDN", "1.0000");
  T("RDN", "4.0000");
  T("SWAP", "3.0000");
  T("BSP", "0.0000");
  T("5 +", "9.0000");   /* CLX disables lift: 5 replaces 0 */
  T("SH SWAP", "5.0000"); /* LASTX */

  /* --- registers --- */
  fresh();
  T("4 2 STO 0 5", "42.0000");
  T("BSP RCL 0 5", "42.0000");
  T("2 STO + 0 5 RCL 0 5", "44.0000");
  T("RCL 9 9", "0.0000");
  T("RCL 9 9 5", "5_");   /* SIZE 100: R99 exists, '5' starts entry */
  T("ENTER STO . MUL", "5.0000");
  T("7 STO 0 1 1 STO 0 2 RCL SH 0 2", "7.0000"); /* RCL IND 02 -> R01 */

  /* --- conditionals and flags --- */
  fresh();
  T("1 ENTER SH 3 DOWN F1", "YES");
  T("BSP 2 SH 3 DOWN F1", "NO");
  T("EXIT EXIT SH 6 F1 0 5 F3 0 5", "YES");
  T("SH 6 F2 0 5 F3 0 5", "NO");

  /* --- alpha --- */
  fresh();
  T("SH ENTER RCL LN TAN TAN CHS", "HELLO_");
  T("SH ENTER", "0.0000");
  T("SH ENTER", "HELLO");
  T("SH ENTER 1 2 ENTER SH ENTER F5 F1 . MUL", "HELLO12.0000");
  T("F2 0 0", "HELLO12.0000"); /* ASTO 00 */
  T("SH ENTER RCL 0 0", "HELLO1");

  /* --- programs --- */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("LBL \"SQ\"\nX^2\nRTN\n");
  T("5 XEQ SH ENTER 9 7 SH ENTER", "25.0000");
  T("SH XEQ . .", "25.0000");
  import("LBL \"LOOP\"\n0\nSTO 01\n1.005\nSTO 00\nLBL 01\nRCL 00\nINT\nST+ 01\nISG 00\nGTO 01\nRCL 01\nEND\n");
  T("XEQ SH ENTER TAN CHS CHS E SH ENTER", "15.0000");
  T("SH RS", "00 REG 213");
  T("DOWN", "01 LBLTLOOP");
  T("DOWN", "02 0");
  T("DOWN DOWN", "04 1.005");
  T("SH RS", "15.0000");

  /* program entered by keystrokes, local label E on the LN key */
  fresh();
  T("SH XEQ . . SH RS", "00 REG 219");
  T("SH 3 F1 LN", "01 LBL E");
  T("2 *", "03 *");
  T("1 +", "05 +");
  T("SH 3 F2", "06 RTN");
  T("EXIT EXIT", "0.0000");
  T("4 XEQ LN", "9.0000");
  T("SH 2", "9.0000");   /* USER on */
  T("3 S+", "1.0000");   /* no label A: normal SIGMA+ */
  T("3 LN", "7.0000");   /* label E */
  T("SH 2 3 LN", "1.0986");

  /* error in program, flag 25 */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("LBL \"E\"\n0\n1/X\n5\nEND\n");
  T("XEQ SH ENTER LN SH ENTER", "DATA ERROR");
  T("SH RS", "03 1/X");
  T("SH RS", "0.0000");
  T("SH 6 F1 2 5 XEQ SH ENTER LN SH ENTER", "5.0000");   /* flag 25 ignores the error */
  T("SH 6 F3 2 5", "NO");
  T("BSP", "5.0000");

  /* subroutines and PSE/VIEW/PROMPT */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("LBL \"A\"\n3\nXEQ 05\nXEQ 05\nRTN\nLBL 05\nX^2\nRTN\n");
  T("XEQ SH ENTER S+ SH ENTER", "81.0000");
  T("SH XEQ . .", "81.0000");
  import("LBL \"P\"\n\"HI\"\nPROMPT\n\"|-X\"\nAVIEW\nEND\n");
  T("XEQ SH ENTER E SH ENTER", "HI");
  T("RS", "HIX");

  /* catalogs */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("LBL \"AB\"\nEND\n");
  T("SH ADD F1 RS", "LBLTAB");
  T("DOWN", "END");
  T("DOWN", " .END. REG 217");
  T("EXIT", "0.0000");

  /* SIZE / ASN */
  fresh();
  T("SH 8 F1 0 1 0 EXIT", "0.0000");
  T("RCL 1 0", "NONEXISTENT");
  T("BSP SH 1", "ASN _");
  T("E RDN", "ASN PI_");
  T("SH ENTER", "ASN PI _");
  T("SIN", "0.0000");
  T("SH 2 SIN", "3.1416");
  T("SH 2 9 0 SIN", "1.0000");

  /* PRGM editing: text, numbers, delete, GTO . nnn, SST/BST in run mode */
  fresh();
  T("SH XEQ . . SH RS", "00 REG 219");
  T("SH 3 F1 SH ENTER INV SH ENTER", "01 LBLTB");
  T("SH ENTER RCL", "TH_");
  T("XEQ", "THF_");
  T("SH ENTER", "02 THF");
  T("1 . 5", "03 1.5_");
  T("E CHS 3", "03 1.5 -3_");
  T("ENTER", "04 ENTER^");
  T("BSP", "03 1.5E-3");
  T("SH XEQ . 0 0 2", "02 THF");
  T("SH UP", "01 LBLTB");
  T("SH DOWN SH DOWN SH DOWN", "04 .END.");
  T("SH DOWN", "00 REG 217");
  T("SH RS", "0.0000");
  T("XEQ SH ENTER INV SH ENTER", "0.0015");
  T("SH ENTER", "HF");
  T("SH ENTER SH XEQ SH ENTER INV SH ENTER", "0.0015");

  /* statistics and conversions */
  fresh();
  T("2 ENTER 1 S+ 4 ENTER 3 S+ 6 ENTER 5 S+", "3.0000");
  T("SH DIV F3", "3.0000");
  T("SWAP", "4.0000");
  T("SH DIV F4", "2.0000");
  T("EXIT 3 ENTER 4 SH 5 F2", "5.0000");
  T("SWAP", "36.8699");
  T("SWAP F1", "4.0000");
  T("SWAP", "3.0000");
  T("EXIT 6 4 SH 4 F1", "100.0000");
  T("F2 EXIT", "64.0000");
  T("1 2 3 4 5 CHS SH E F3 3 EXIT", "-12.35 03");
  T("SH E F1 4 EXIT 7 STO 0 1 BSP 3 SH STO 0 1", "7.0000");
  T("RCL 0 1", "3.0000");

  /* ISG skip and VIEW in program, export listing */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("LBL \"CNT\"\n1.003\nSTO 00\nLBL 02\nVIEW 00\nISG 00\nGTO 02\n\"DONE\"\nAVIEW\nEND\n");
  T("XEQ SH ENTER SQRT SWAP DIV SH ENTER", "DONE");
  T("RCL 0 0", "4.0030");
  hp41_export_program(pr, 0);

  /* example program: x^2 - 3x + 2 = 0 -> roots 1 and 2 */
  fresh();
  T("SH XEQ . .", "0.0000");
  import("; Quadratic solver: enter a, b, c (a ENTER b ENTER c), XEQ \"QUAD\"\n; real roots are left in X and Y, complex -> \"COMPLEX\"\nLBL \"QUAD\"\nSTO 03\nRDN\nSTO 02\nRDN\nSTO 01\nRCL 02\nX^2\n4\nRCL 01\n*\nRCL 03\n*\n-\nX<0?\nGTO 01\nSQRT\nSTO 04\nRCL 02\nCHS\nRCL 04\n-\n2\n/\nRCL 01\n/\nRCL 02\nCHS\nRCL 04\n+\n2\n/\nRCL 01\n/\nRTN\nLBL 01\n\"COMPLEX\"\nAVIEW\nEND\n");
  T("1 ENTER 3 CHS ENTER 2 XEQ SH ENTER 7 4 S+ LOG SH ENTER", "2.0000");
  T("SWAP", "1.0000");
  T("1 ENTER 0 ENTER 1 XEQ SH ENTER 7 4 S+ LOG SH ENTER", "COMPLEX");
}
