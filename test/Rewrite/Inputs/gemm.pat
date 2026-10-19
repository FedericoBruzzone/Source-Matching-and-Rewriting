c {
void dgemm_pat(int n, double a[3][3], double b[3][3], double c[3][3]) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}
}={
#include <cblas.h>

void dgemm_pat(int n, double a[3][3], double b[3][3], double c[3][3]) {
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1.0, (double*)a, n, (double*)b, n, 1.0, (double*)c, 3);
}
}
