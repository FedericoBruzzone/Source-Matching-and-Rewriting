// --smr-unroll duplicates loop body operations and adjusts loop steps.
// When the trip count is unknown or not a multiple of the unroll factor,
// an epilogue (residual) loop is generated to process the remaining iterations.
//
// RUN: smr-opt --smr-unroll=factor=3 %s | FileCheck %s

!s32i = !cir.int<s, 32>

module {
  // CHECK-LABEL: cir.func @loop_unroll_epilogue(
  //
  // Ciclo Principal Desenrolado (fator = 3, 2 loads por iteração = 6 loads):
  // CHECK:         cir.for : cond {
  // CHECK:         } body {
  // CHECK-COUNT-6:   cir.load
  // CHECK:         } step {
  // CHECK:           cir.binop(add
  //
  // Ciclo Residual / Epílogo (1 iteração por ciclo = 2 loads):
  // CHECK:         cir.for : cond {
  // CHECK:         } body {
  // CHECK:           cir.load
  // CHECK:           cir.load
  // CHECK:         } step {
  // CHECK:           cir.unary(inc
  cir.func @loop_unroll_epilogue(%arg0: !cir.ptr<!s32i>, %arg1: !s32i) {
    %0 = cir.alloca !s32i, !cir.ptr<!s32i>, ["i", init] {alignment = 4 : i64}
    %1 = cir.const #cir.int<0> : !s32i
    cir.store %1, %0 : !s32i, !cir.ptr<!s32i>
    cir.for : cond {
      %2 = cir.load %0 : !cir.ptr<!s32i>, !s32i
      %3 = cir.cmp(lt, %2, %arg1) : !s32i, !cir.bool
      cir.condition(%3)
    } body {
      %2 = cir.load %0 : !cir.ptr<!s32i>, !s32i
      %3 = cir.ptr_stride %arg0, %2 : (!cir.ptr<!s32i>, !s32i) -> !cir.ptr<!s32i>
      %4 = cir.load %3 : !cir.ptr<!s32i>, !s32i
      %5 = cir.unary(inc, %4) nsw : !s32i, !s32i
      cir.store %5, %3 : !s32i, !cir.ptr<!s32i>
      cir.yield
    } step {
      %2 = cir.load %0 : !cir.ptr<!s32i>, !s32i
      %3 = cir.unary(inc, %2) nsw : !s32i, !s32i
      cir.store %3, %0 : !s32i, !cir.ptr<!s32i>
      cir.yield
    }
    cir.return
  }
}