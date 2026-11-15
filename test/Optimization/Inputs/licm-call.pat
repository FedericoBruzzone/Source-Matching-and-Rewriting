$hoist = [licm(expensive)]

c {
double expensive(double);
void f(double *v, double x, int n)
{
  for (int i = 0; i < n; ++i)
    v[i] += expensive(x);
}
} [$hoist]
