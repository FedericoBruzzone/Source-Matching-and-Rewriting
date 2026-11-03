$opts = [unroll(2)]

c {
void dot(int n, double *x, double *y, double out)
{
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
}
} if (n > 100 && !is_const(n)) = {
void dot(int n, double *x, double *y, double out)
{
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
}
} elif (is_const(n)) = {
void dot(int n, double *x, double *y, double out)
{
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
}
} [$opts] else = {
void dot(int n, double *x, double *y, double out)
{
  for (int i = 0; i < n; ++i) {
    out += x[i] * y[i];
  }
}
}
