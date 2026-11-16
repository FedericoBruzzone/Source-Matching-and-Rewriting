#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 2048

__attribute__((noinline)) double calculate_factor(double c) {
  return 1.0 + 1e-9 * sqrt(c) + 1e-12 * sin(c);
}

__attribute__((noinline)) void kernel(double (*m)[N], double cfg) {
  for (int i = 0; i < N; ++i) {
    double factor = calculate_factor(cfg);
    for (int j = 0; j < N; ++j)
      m[i][j] *= factor;
  }
}

int main(void) {
  double (*m)[N] = malloc(N * sizeof *m);
  for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) m[i][j] = 1 + (i + j) % 5;
  double best = 1e9, s = 0;
  for (int r = 0; r < 9; ++r) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    kernel(m, 2.0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (t < best) best = t;
  }
  for (int i = 0; i < N; ++i) for (int j = 0; j < N; ++j) s += m[i][j];
  printf("%.6e %.6f\n", s, best);
  return 0;
}
