/* Same program written by hand in C, compiled with the same flags. */
/* noyau: axpy */
#include <stdio.h>
#include <stdlib.h>

static void axpy(double alpha, const double *x, double *y, long n) {
    for (long i = 0; i < n; i++) y[i] = alpha * x[i] + y[i];
}

int main(void) {
    long n = 4096;
    double *x = malloc(n * sizeof(double)), *y = malloc(n * sizeof(double));
    for (long i = 0; i < n; i++) { x[i] = 1.0; y[i] = 0.0; }
    for (int r = 0; r < 100000; r++) axpy(0.000001, x, y, n);
    printf("%.15g\n", y[4095]);
    return 0;
}
