/* Same program written by hand in C, compiled with the same flags. */
#include <stdio.h>
#include <stdlib.h>

static double dot(const double *a, const double *b, long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++) s = s + a[i] * b[i];
    return s;
}

int main(void) {
    long n = 4096;
    double *a = malloc(n * sizeof(double)), *b = malloc(n * sizeof(double));
    for (long i = 0; i < n; i++) { a[i] = 0.5; b[i] = 2.0; }
    double total = 0.0;
    for (int r = 0; r < 100000; r++) total = total + dot(a, b, n);
    printf("%.15g\n", total);
    return 0;
}
