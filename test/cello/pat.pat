$GLOBAL = [$inline]

$routine_opt_1 = [$unroll, $dce]

$routine_opt_2 = [$tiling(32)]

c {
void ddot0(int N, double *x, double *y, double out)
{
  for (int i = 0; i < N; ++i) {
    out += x[i] * y[i];
  }
}
} $if(N > 100 && !$is_const(N)) = {
  #include <cblas.h>
  void ddot0(int N, double *x, double *y, double out)
  {
    out += cblas_ddot(N, x, 1, y, 1);
  }
} $elif ($is_const(N)) = {
  #include <cblas.h>
  void ddot1(int N, double *x, double *y, double out)
  {
    out += cblas_ddot(N, x, 1, y, 1);
  }
} [$routine_opt_1]  $else = {
  #include <cblas.h>
  void ddot2(int N, double *x, double *y, double out)
  {
    out += cblas_ddot(N, x, 1, y, 1);
  }
} [$routine_opt_2] 