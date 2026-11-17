c {
float dot_product(float *a, float *b, int n)
{
  float sum = 0.0f;
  for (int i = 0; i < n; ++i)
    sum += a[i] * b[i];
  return sum;
}
} [fast_math, licm, vectorize(vf=4, ic=1)]
