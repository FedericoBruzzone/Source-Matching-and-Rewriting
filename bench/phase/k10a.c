#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 2048

__attribute__((noinline)) void kernel(double (*m)[N], double *d, double *p) {
  for (int i = 0; i < N; ++i)
    for (int j = 0; j < N; ++j)
      m[i][j] += d[i] * p[j];
}

int main(void) {
  double (*m)[N] = calloc(N, sizeof *m);
  double *d = malloc(N * sizeof(double)), *p = malloc(N * sizeof(double));
  for (int i = 0; i < N; ++i) { d[i] = (i % 7) * 0.5; p[i] = (i % 11) * 0.25; }
  double best = 1e9, s = 0;
  for (int r = 0; r < 9; ++r) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    kernel(m, d, p);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (t < best) best = t;
  }
  for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) s += m[i][j];
  printf("%.6e %.6f\n", s, best);
  return 0;
}
