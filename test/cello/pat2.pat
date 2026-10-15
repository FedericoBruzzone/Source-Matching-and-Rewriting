$routine_opt_1 = [$unroll(4), $dce]

c {
void ddot0(int N, double *x, double *y, double out)
{
  for (int i = 0; i < N; ++i) {
    out += x[i] * y[i];
  }
}
} [$routine_opt_1] 