// --smr-fusion merges neighbouring canonical loops that run the same
// iterations into one loop, the second body after the first. Loops that
// differ in bound, start or step, or with a statement between them, stay.
//
// RUN: clang -fclangir -emit-cir %s -o %t.cir
// RUN: smr-opt --smr-fusion %t.cir | FileCheck %s

// CHECK-LABEL: cir.func {{.*}} @same_bound(
// CHECK:         cir.for
// CHECK:           cir.store align(4) {{.*}} : !cir.float
// CHECK:           cir.load align(4)
// CHECK:           cir.store align(4) {{.*}} : !cir.float
// CHECK-NOT:     cir.for
void same_bound(float *a, float *b, float *c, int n) {
  for (int i = 0; i < n; ++i)
    b[i] = a[i] * 2.0f;
  for (int j = 0; j < n; ++j)
    c[j] = b[j] + 1.0f;
}

// CHECK-LABEL: cir.func {{.*}} @same_iv(
// CHECK:         cir.for
// CHECK-NOT:     cir.for
void same_iv(float *a, float *b, float *c, int n) {
  int i;
  for (i = 0; i < n; ++i)
    b[i] = a[i] * 2.0f;
  for (i = 0; i < n; ++i)
    c[i] = b[i] + 1.0f;
}

// CHECK-LABEL: cir.func {{.*}} @three(
// CHECK:         cir.for
// CHECK-NOT:     cir.for
void three(float *a, float *b, float *c, float *d, int n) {
  for (int i = 0; i < n; ++i)
    b[i] = a[i];
  for (int i = 0; i < n; ++i)
    c[i] = b[i];
  for (int i = 0; i < n; ++i)
    d[i] = c[i];
}

// CHECK-LABEL: cir.func {{.*}} @constant_bound(
// CHECK:         cir.for
// CHECK-NOT:     cir.for
void constant_bound(float *a, float *b) {
  for (int i = 0; i < 64; ++i)
    a[i] = 1.0f;
  for (int i = 0; i < 64; ++i)
    b[i] = a[i];
}

// CHECK-LABEL: cir.func {{.*}} @inner(
// CHECK:         cir.for
// CHECK:           cir.for
// CHECK-NOT:       cir.for
// CHECK:         cir.return
void inner(float *a, float *b, int n, int m) {
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < m; ++j)
      a[i * m + j] = 1.0f;
    for (int j = 0; j < m; ++j)
      b[i * m + j] = 2.0f;
  }
}

// CHECK-LABEL: cir.func {{.*}} @other_bound(
// CHECK:         cir.for
// CHECK:         cir.for
void other_bound(float *a, float *b, int n, int m) {
  for (int i = 0; i < n; ++i)
    a[i] = 1.0f;
  for (int i = 0; i < m; ++i)
    b[i] = 2.0f;
}

// CHECK-LABEL: cir.func {{.*}} @other_start(
// CHECK:         cir.for
// CHECK:         cir.for
void other_start(float *a, float *b, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = 1.0f;
  for (int i = 1; i < n; ++i)
    b[i] = 2.0f;
}

// CHECK-LABEL: cir.func {{.*}} @other_step(
// CHECK:         cir.for
// CHECK:         cir.for
void other_step(float *a, float *b, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = 1.0f;
  for (int i = 0; i < n; i += 2)
    b[i] = 2.0f;
}

// CHECK-LABEL: cir.func {{.*}} @statement_between(
// CHECK:         cir.for
// CHECK:         cir.for
void statement_between(float *a, float *b, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = 1.0f;
  a[0] = 5.0f;
  for (int i = 0; i < n; ++i)
    b[i] = 2.0f;
}
