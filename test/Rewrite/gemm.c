// The README example: the GEMM loop nest is replaced by a cblas_dgemm call.
//
// RUN: smr %s %S/Inputs/gemm.pat -o %t.mlir | FileCheck %s --check-prefix=LOG
// RUN: FileCheck %s --input-file=%t.mlir
//
// The serialized pattern gives the same rewrite.
//
// RUN: smr %S/Inputs/gemm.pat --serialize=%t.opat
// RUN: smr %s %t.opat -o %t.serialized.mlir | FileCheck %s --check-prefix=LOG
// RUN: FileCheck %s --input-file=%t.serialized.mlir

// LOG: Got 1 Matches.
// LOG: Applying rewrite 0...

// CHECK-LABEL: cir.func {{.*}}@gemm_double(
// CHECK-NOT:     cir.for
// CHECK:         cir.call @cblas_dgemm(
// CHECK:       cir.func private @cblas_dgemm(

void gemm_double(int n, double a[3][3], double b[3][3], double c[3][3]) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}
