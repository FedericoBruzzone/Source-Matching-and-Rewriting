// smr-cc compiles a C file through smr. Without patterns the program is what
// the CIR path gives, with a pattern it is rewritten and the log says so.
// Anything that is not the compilation of one C file goes to clang.
//
// RUN: clang -O3 -ffp-contract=off %s -o %t.ref
// RUN: %t.ref | FileCheck %s --check-prefix=RESULT
//
// RUN: env SMR_CC_LOG=%t.plain.log %smr_cc -O3 -c %s -o %t.plain.o
// RUN: %smr_cc %t.plain.o -o %t.plain
// RUN: %t.plain | FileCheck %s --check-prefix=RESULT
// RUN: FileCheck %s --input-file=%t.plain.log --check-prefix=PLAIN
//
// RUN: env SMR_CC_LOG=%t.rewrite.log SMR_PATS=%S/../Optimization/Inputs/dot.pat %smr_cc -O3 -c %s -o %t.rewrite.o
// RUN: %smr_cc %t.rewrite.o -o %t.rewrite
// RUN: %t.rewrite | FileCheck %s --check-prefix=RESULT
// RUN: FileCheck %s --input-file=%t.rewrite.log --check-prefix=REWRITE
//
// Include paths and macros reach the preprocessor, smr itself takes none.
// RUN: env SMR_CC_LOG=%t.flags.log %smr_cc -O3 -I %S/Inputs -DSCALE=3 -c %S/Inputs/smr-cc-flags.c -o %t.flags.o
// RUN: %smr_cc %t.flags.o -o %t.flags
// RUN: %t.flags | FileCheck %s --check-prefix=FLAGS
//
// RESULT: 64.0
// PLAIN:   OK {{.*}}smr-cc.c rewrites=0
// REWRITE: OK {{.*}}smr-cc.c rewrites=1
// FLAGS:   21

#include <stdio.h>

double scale_dot(int n, double *x, double *y) {
  double out = 0;
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
  return 2 * out;
}

int main(void) {
  double x[5] = {1, 2, 3, 4, 5};
  double y[5] = {1, 1, 2, 2, 3};
  printf("%.1f\n", scale_dot(5, x, y));
  return 0;
}
