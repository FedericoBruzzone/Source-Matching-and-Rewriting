$hoist = [licm(twice)]

c {
void fill(int *a, int x, int y, int n)
{
  for (int i = 0; i < n; ++i) {
    a[i] = x + y * 3;
  }
}
} [$hoist]
