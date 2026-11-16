// Times the three gemm_nn builds on one shape: ./gemm K M N. Prints the best
// of five runs of each, in milliseconds, and whether the outputs are equal.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void (*Fn)(int, int, int, float, float *, int, float *, int, float *, int);
void nn_base(int, int, int, float, float *, int, float *, int, float *, int);
void nn_ours(int, int, int, float, float *, int, float *, int, float *, int);
void nn_hand(int, int, int, float, float *, int, float *, int, float *, int);

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  int K = atoi(argv[1]), M = atoi(argv[2]), N = atoi(argv[3]);
  float *A = malloc((size_t)M * K * 4), *B = malloc((size_t)K * N * 4);
  float *C = malloc((size_t)M * N * 4), *R = malloc((size_t)M * N * 4);
  for (size_t i = 0; i < (size_t)M * K; ++i)
    A[i] = (i % 13) * 0.25f;
  for (size_t i = 0; i < (size_t)K * N; ++i)
    B[i] = (i % 7) * 0.5f;

  Fn fns[] = {nn_base, nn_ours, nn_hand};
  double t[3];
  int equal = 1;
  for (int v = 0; v < 3; ++v) {
    double best = 1e9;
    for (int r = 0; r < 5; ++r) {
      memset(C, 0, (size_t)M * N * 4);
      double start = now();
      fns[v](M, N, K, 1.0f, A, K, B, N, C, N);
      double elapsed = now() - start;
      if (elapsed < best)
        best = elapsed;
    }
    t[v] = best;
    if (v == 0)
      memcpy(R, C, (size_t)M * N * 4);
    else if (memcmp(R, C, (size_t)M * N * 4))
      equal = 0;
  }
  printf("%5d %5d %7d %9.2f %9.2f %9.2f %9.2f %9.2f  %s\n", K, M, N, t[0] * 1000,
         t[1] * 1000, t[2] * 1000, t[0] / t[1], t[0] / t[2], equal ? "yes" : "NO");
  return 0;
}
