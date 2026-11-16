#include <stdio.h>
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) void kernel(double *a, double *b, double *m, double *c, int n) {
  for (int i = 0; i < n; ++i)
    a[i] = b[i] + m[i] * c[0];
}

int main(void) {
  int n = 4000000;
  double *a = calloc(n, sizeof(double)), *b = calloc(n, sizeof(double));
  double *m = calloc(n, sizeof(double)), c[1] = {1.0001};
  for (int i = 0; i < n; ++i) { b[i] = i % 17; m[i] = i % 5; }
  double best = 1e9, s = 0;
  for (int r = 0; r < 15; ++r) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    kernel(a, b, m, c, n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (t < best) best = t;
  }
  for (int i = 0; i < n; ++i) s += a[i];
  printf("%.6e %.6f\n", s, best);
  return 0;
}
