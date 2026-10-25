/* matmul (PLB2, attractivechaos/plb2), the same algorithm as plbmatmul.cl, in C. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static double *remplit(int64_t n) {
    double *m = malloc((size_t)(n * n) * sizeof(double));
    double t = 1.0 / (double)n / (double)n;
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++) m[i * n + j] = t * (double)(i - j) * (double)(i + j);
    return m;
}

/* noyau: produit */
static void produit(const double *a, const double *b, double *c, int64_t n) {
    double *bt = malloc((size_t)(n * n) * sizeof(double));
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++) bt[j * n + i] = b[i * n + j];
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++) {
            double s = 0.0;
            for (int64_t k = 0; k < n; k++) s += a[i * n + k] * bt[j * n + k];
            c[i * n + j] = s;
        }
    free(bt);
}

int main(void) {
    long long n = 0;
    if (scanf("%lld", &n) != 1 || n < 1) return 1;
    double *a = remplit(n), *b = remplit(n), *c = calloc((size_t)(n * n), sizeof(double));
    produit(a, b, c, n);
    printf("%f\n", c[(n / 2) * n + n / 2]);
    free(a); free(b); free(c);
    return 0;
}
