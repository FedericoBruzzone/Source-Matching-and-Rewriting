// A pattern may declare the function it calls. The declaration comes before
// the pattern function and has no body.
//
// RUN: smr %s %S/Inputs/licm-call.pat -o %t.mlir | FileCheck %s --check-prefix=LOG
// RUN: FileCheck %s --input-file=%t.mlir
//
// LOG: Applying optimization: licm(expensive)

double expensive(double);

// CHECK-LABEL: cir.func {{.*}}@accumulate(
// CHECK:         cir.call @expensive
// CHECK:         cir.for
// CHECK:         } body {
// CHECK-NOT:       cir.call @expensive
// CHECK:         } step {
void accumulate(double *a, double y, int n) {
  for (int i = 0; i < n; ++i)
    a[i] += expensive(y);
}
