// A translation unit smr cannot handle (inline asm) is compiled by plain clang
// instead, and the log says so. With SMR_CC_ALL it is clang -fclangir.
//
// RUN: rm -f %t.log %t.all.log
// RUN: env SMR_CC_LOG=%t.log SMR_PATS=%S/../Optimization/Inputs/dot.pat %smr_cc -O3 -c %s -o %t.o
// RUN: %smr_cc %t.o -o %t
// RUN: %t | FileCheck %s
// RUN: FileCheck %s --input-file=%t.log --check-prefix=LOG
//
// RUN: env SMR_CC_ALL=1 SMR_CC_LOG=%t.all.log %smr_cc -O3 -c %s -o %t.all.o
// RUN: %smr_cc %t.all.o -o %t.all
// RUN: %t.all | FileCheck %s
// RUN: FileCheck %s --input-file=%t.all.log --check-prefix=LOG
//
// CHECK: 4
// LOG: FALLBACK {{.*}}smr-cc-fallback.c (smr wrote nothing)

#include <stdio.h>

int f(int x) {
  asm volatile("nop");
  return x + 1;
}

int main(void) {
  printf("%d\n", f(3));
  return 0;
}
