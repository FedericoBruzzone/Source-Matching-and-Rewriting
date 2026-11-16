#define BM 64
#define BN 64
#define BK 64
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
__attribute__((noinline)) void nn_hand(int M, int N, int K, float ALPHA, float *A, int lda,
                                       float *B, int ldb, float *C, int ldc) {
  for (int i0 = 0; i0 < M; i0 += BM)
    for (int j0 = 0; j0 < N; j0 += BN)
      for (int k0 = 0; k0 < K; k0 += BK) {
        int i_end = MIN(i0 + BM, M);
        int k_end = MIN(k0 + BK, K);
        int j_end = MIN(j0 + BN, N);
        for (int i = i0; i < i_end; ++i)
          for (int k = k0; k < k_end; ++k) {
            register float A_PART = ALPHA * A[i * lda + k];
            for (int j = j0; j < j_end; ++j)
              C[i * ldc + j] += A_PART * B[k * ldb + j];
          }
      }
}
