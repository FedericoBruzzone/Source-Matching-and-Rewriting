#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) void kernel_a(double *val, double x, double y, int n) {
  for (int i = 0; i < n; ++i)
    val[i] += (exp(x) + log(y)) * i;
}

__attribute__((noinline)) int kernel_b(double *in, int n) {
  int bad = 0;
  for (int i = 0; i < n; ++i) {
    errno = 0;
    double r = log(in[i]);
    (void)r;
    if (errno)
      ++bad;
  }
  return bad;
}

int main(void) {
  int n = 4000000;
  double *v = calloc(n, sizeof(double));
  double best = 1e9, s = 0;
  for (int r = 0; r < 7; ++r) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    kernel_a(v, 1.5, 2.5, n);
    clock_gettime(CLOCK_MONOTONIC, &b);
    double t = (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9;
    if (t < best) best = t;
  }
  for (int i = 0; i < n; ++i) s += v[i];
  double in[8] = {1.0, -1.0, 2.0, -3.0, 0.5, -0.5, 4.0, -8.0};
  int bad = kernel_b(in, 8);
  printf("sum=%.6e bad=%d time_ms=%.3f\n", s, bad, best * 1000);
  return 0;
}
