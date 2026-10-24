/* Same program written by hand in C, compiled with the same flags. */
/* noyau: couche */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double sigmoid(double v) { return 1.0 / (1.0 + exp(-v)); }

static void couche(const double *w, const double *b, const double *x, double *y, long m, long n) {
    for (long i = 0; i < m; i++) {
        double s = b[i];
        for (long j = 0; j < n; j++) s = s + w[i * n + j] * x[j];
        y[i] = sigmoid(s);
    }
}

int main(void) {
    long n = 256;
    double *w = malloc(n * n * sizeof(double)), *b = malloc(n * sizeof(double));
    double *x = malloc(n * sizeof(double)), *y = malloc(n * sizeof(double));
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) w[i * n + j] = (double)((i * 7 + j * 3) % 17) / 17.0 - 0.5;
        b[i] = (double)(i % 5) / 10.0;
        x[i] = (double)(i % 3) / 3.0;
        y[i] = 0.0;
    }
    for (long r = 0; r < 4000; r++) {
        couche(w, b, x, y, n, n);
        x[r % n] = y[(r * 7) % n];
    }
    printf("%.15g\n", y[0] + y[n - 1]);
    return 0;
}
