// With no pass, smr-opt prints the module it reads, so it must know every
// dialect that shows up in the modules smr emits.
//
// RUN: smr-opt %s | FileCheck %s
// RUN: smr-opt %s | smr-opt | FileCheck %s

!s32i = !cir.int<s, 32>

// CHECK: module attributes {cir.lang = #cir.lang<c>, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr = dense<64> : vector<4xi64>
module attributes {cir.lang = #cir.lang<c>, dlti.dl_spec = #dlti.dl_spec<!llvm.ptr = dense<64> : vector<4xi64>, "dlti.endianness" = "little">} {
  // CHECK-LABEL: cir.func dso_local @sum(
  // CHECK:         cir.for : cond {
  // CHECK:         } body {
  // CHECK:         } step {
  cir.func dso_local @sum(%arg0: !s32i, %arg1: !cir.ptr<!s32i>) -> !s32i {
    %0 = cir.alloca !s32i, !cir.ptr<!s32i>, ["n", init] {alignment = 4 : i64}
    %1 = cir.alloca !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>>, ["a", init] {alignment = 8 : i64}
    %2 = cir.alloca !s32i, !cir.ptr<!s32i>, ["s", init] {alignment = 4 : i64}
    %3 = cir.alloca !s32i, !cir.ptr<!s32i>, ["i", init] {alignment = 4 : i64}
    cir.store %arg0, %0 : !s32i, !cir.ptr<!s32i>
    cir.store %arg1, %1 : !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>>
    %4 = cir.const #cir.int<0> : !s32i
    cir.store %4, %2 : !s32i, !cir.ptr<!s32i>
    cir.store %4, %3 : !s32i, !cir.ptr<!s32i>
    cir.for : cond {
      %6 = cir.load %3 : !cir.ptr<!s32i>, !s32i
      %7 = cir.load %0 : !cir.ptr<!s32i>, !s32i
      %8 = cir.cmp(lt, %6, %7) : !s32i, !cir.bool
      cir.condition(%8)
    } body {
      %6 = cir.load %1 : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
      %7 = cir.load %3 : !cir.ptr<!s32i>, !s32i
      %8 = cir.ptr_stride %6, %7 : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
      %9 = cir.load %8 : !cir.ptr<!s32i>, !s32i
      %10 = cir.load %2 : !cir.ptr<!s32i>, !s32i
      %11 = cir.binop(add, %10, %9) nsw : !s32i
      cir.store %11, %2 : !s32i, !cir.ptr<!s32i>
      cir.yield
    } step {
      %6 = cir.load %3 : !cir.ptr<!s32i>, !s32i
      %7 = cir.unary(inc, %6) nsw : !s32i, !s32i
      cir.store %7, %3 : !s32i, !cir.ptr<!s32i>
      cir.yield
    }
    %5 = cir.load %2 : !cir.ptr<!s32i>, !s32i
    cir.return %5 : !s32i
  }
}
