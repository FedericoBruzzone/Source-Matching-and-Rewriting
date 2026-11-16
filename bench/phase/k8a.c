#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) void kernel(double *val, double x, double y, int n) {
  for (int i = 0; i < n; ++i)
    val[i] += (exp(x) + log(y)) * i;
}

int main(void) {
  int n = 4000000;
  double *v = calloc(n, sizeof(double));
  double best = 1e9, s = 0;
  for (int r = 0; r < 7; ++r) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    kernel(v, 1.5, 2.5, n);
    clock_gettime(CLOCK_MONOTONIC, &b);
    double t = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9;
    if (t < best) best = t;
  }
  for (int i = 0; i < n; ++i) s += v[i];
  printf("%.6e %.6f\n", s, best);
  return 0;
}
