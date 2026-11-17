c {
void saxpy(float *y, float *x, float alpha, int n)
{
  for (int i = 0; i < n; ++i)
    y[i] = alpha * x[i] + y[i];
}
} [vectorize(assume-no-alias=false)]
