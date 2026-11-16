#include <stdio.h>
#include <stdlib.h>
#include <time.h>

__attribute__((noinline)) float kernel(float *a, float *b, int n) {
  float sum = 0.0f;
  for (int i = 0; i < n; ++i)
    sum += a[i] * b[i];
  return sum;
}

float (*volatile fp)(float *, float *, int) = kernel;

int main(void) {
  int n = 1 << 14;
  float *a = malloc(n * sizeof(float)), *b = malloc(n * sizeof(float));
  for (int i = 0; i < n; ++i) { a[i] = 0.1f * (i % 13) + 0.013f; b[i] = 0.37f * (i % 7) + 0.0071f; }
  double best = 1e9;
  float s = 0;
  for (int r = 0; r < 30; ++r) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int q = 0; q < 200; ++q) s = fp(a, b, n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double t = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
    if (t / 200 < best) best = t / 200;
  }
  printf("%a %.9f\n", s, best);
  return 0;
}
