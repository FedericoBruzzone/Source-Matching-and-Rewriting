$hoist = [licm(exp, log)]

c {
double exp(double);
double log(double);
void f(double *val, double x, double y, int n)
{
  for (int i = 0; i < n; ++i)
    val[i] += (exp(x) + log(y)) * i;
}
} [$hoist]
