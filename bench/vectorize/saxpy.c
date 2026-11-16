#include <stdio.h>
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) void kernel(float *y, float *x, float alpha, int n) {
  for (int i = 0; i < n; ++i)
    y[i] = alpha * x[i] + y[i];
}

int main(void) {
  int n = 1 << 14;
  float *x = malloc(n * sizeof(float)), *y = calloc(n, sizeof(float));
  for (int i = 0; i < n; ++i) x[i] = (i % 7) * 0.5f;
  double best = 1e9, s = 0;
  for (int r = 0; r < 30; ++r) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int q = 0; q < 200; ++q) kernel(y, x, 0.001f, n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (t / 200 < best) best = t / 200;
  }
  for (int i = 0; i < n; ++i) s += y[i];
  printf("%.6e %.9f\n", s, best);
  return 0;
}
