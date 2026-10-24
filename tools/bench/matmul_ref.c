/* Same program written by hand in C, compiled with the same flags. */
/* noyau: matmul */
#include <stdio.h>
#include <stdlib.h>

static void matmul(const double *a, const double *b, double *c, long n) {
    for (long i = 0; i < n; i++)
        for (long k = 0; k < n; k++) {
            double aik = a[i * n + k];
            for (long j = 0; j < n; j++) c[i * n + j] = c[i * n + j] + aik * b[k * n + j];
        }
}

int main(void) {
    long n = 256;
    double *a = malloc(n * n * sizeof(double)), *b = malloc(n * n * sizeof(double)), *c = malloc(n * n * sizeof(double));
    for (long i = 0; i < n * n; i++) { a[i] = 1.0; b[i] = 0.5; c[i] = 0.0; }
    for (int r = 0; r < 32; r++) matmul(a, b, c, n);
    printf("%.15g\n", c[n * n - 1]);
    return 0;
}
