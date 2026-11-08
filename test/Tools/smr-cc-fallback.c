// A translation unit smr cannot handle (inline asm) is compiled by
// clang -fclangir instead, and the log says so.
//
// RUN: env SMR_CC_LOG=%t.log %smr_cc -O3 -c %s -o %t.o
// RUN: %smr_cc %t.o -o %t
// RUN: %t | FileCheck %s
// RUN: FileCheck %s --input-file=%t.log --check-prefix=LOG
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
