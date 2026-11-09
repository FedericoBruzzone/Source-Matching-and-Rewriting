c {
void fill(int *a, int n)
{
  for (int i = 0; i < n; ++i) {
    a[i] = 0;
  }
}
} = {
void fill(int *a, int n)
{
  a[0] = 0;
}
} [unroll(2), tiling(8), licm, dce]
