// A directive that transforms loops warns when it changes nothing, which is
// how a pattern that selects no loop for it shows up.
//
// The replacement has no loop, so unroll, tiling and licm have nothing to do.
// dce does not count loops and stays quiet.
//
// RUN: smr %s %S/Inputs/no-effect.pat -o %t.mlir 2>&1 | FileCheck %s
//
// CHECK:      Applying optimization: unroll(2)
// CHECK-NEXT: Applying optimization: tiling(8)
// CHECK-NEXT: Applying optimization: licm
// CHECK-NEXT: Applying optimization: dce
// CHECK-NEXT: [WARN] Optimization 'unroll(2)' changed nothing: no loop it could transform.
// CHECK-NEXT: [WARN] Optimization 'tiling(8)' changed nothing: no loop it could transform.
// CHECK-NEXT: [WARN] Optimization 'licm' changed nothing: no loop it could transform.
// CHECK-NOT:  'dce' changed nothing
//
// When the replacement has a loop they transform, nothing is reported.
//
// RUN: smr %s %S/Inputs/dot.pat -o %t.dot.mlir 2>&1 | FileCheck %s --check-prefix=LOOP
//
// LOOP-NOT: changed nothing

void fill(int *v, int n) {
  for (int i = 0; i < n; ++i) {
    v[i] = 0;
  }
}

double dot(int n, double *x, double *y) {
  double t = 0.0;
  for (int i = 0; i < n; ++i) {
    t += x[i] * y[i];
  }
  return t;
}
