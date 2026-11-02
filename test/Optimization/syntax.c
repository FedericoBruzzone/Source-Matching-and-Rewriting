// Names that the user defines start with '$' and optimizations do not. A
// pipeline is defined and used with its '$', an optimization is never written
// with one.
//
// RUN: smr %s %S/Inputs/dot.pat -o %t.mlir | FileCheck %s --check-prefix=OK
//
// OK: Applying optimization: unroll(2)
//
// The keywords of a conditional rewrite are not user defined either: if, elif,
// else and is_const are written without a dollar. The old spelling is an error.
//
// RUN: smr %s %S/Inputs/syntax-branches.pat 2>&1 | FileCheck %s --check-prefix=BRANCHES
// BRANCHES-NOT: Failed to parse PAT file
// BRANCHES: Starting CDG matching
//
// RUN: smr %s %S/Inputs/syntax-dollar-if.pat 2>&1 | FileCheck %s --check-prefix=DOLLAR-IF
// DOLLAR-IF: Failed to parse PAT file
//
// RUN: smr %s %S/Inputs/syntax-dollar-optimization.pat -o %t.a.mlir 2>&1 | FileCheck %s --check-prefix=DOLLAR
// DOLLAR: undefined pipeline '$unroll'
//
// RUN: smr %s %S/Inputs/syntax-bare-pipeline.pat -o %t.b.mlir 2>&1 | FileCheck %s --check-prefix=BARE
// BARE: unknown optimization 'opts'
//
// RUN: smr %s %S/Inputs/syntax-unknown-optimization.pat -o %t.c.mlir 2>&1 | FileCheck %s --check-prefix=UNKNOWN
// UNKNOWN: unknown optimization 'loopfission'
//
// RUN: smr %s %S/Inputs/syntax-undefined-pipeline.pat -o %t.d.mlir 2>&1 | FileCheck %s --check-prefix=UNDEFINED
// UNDEFINED: undefined pipeline '$missing'

double scale_dot(int n, double *x, double *y) {
  double out = 0;
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
  return 2 * out;
}
