c {
void dot(int n, double *x, double *y, double out)
{
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
}
} [$unroll(0)]
