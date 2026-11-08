/* spectral-norm (The Computer Language Benchmarks Game), the same algorithm as spectral.cl, in C. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static double a(int64_t i, int64_t j) {
    return 1.0 / (double)((i + j) * (i + j + 1) / 2 + i + 1);
}

/* noyau: produit_a */
static void produit_a(const double *u, double *v, int64_t n) {
    for (int64_t i = 0; i < n; i++) {
        double s = 0.0;
        for (int64_t j = 0; j < n; j++) s += a(i, j) * u[j];
        v[i] = s;
    }
}

static void produit_at(const double *u, double *v, int64_t n) {
    for (int64_t i = 0; i < n; i++) {
        double s = 0.0;
        for (int64_t j = 0; j < n; j++) s += a(j, i) * u[j];
        v[i] = s;
    }
}

static void produit_ata(const double *u, double *v, double *w, int64_t n) {
    produit_a(u, w, n);
    produit_at(w, v, n);
}

int main(void) {
    long long n = 0;
    if (scanf("%lld", &n) != 1 || n < 1) return 1;
    double *u = malloc(n * sizeof(double)), *v = malloc(n * sizeof(double)), *w = malloc(n * sizeof(double));
    for (long long i = 0; i < n; i++) { u[i] = 1.0; v[i] = 0.0; w[i] = 0.0; }
    for (int k = 0; k < 10; k++) {
        produit_ata(u, v, w, n);
        produit_ata(v, u, w, n);
    }
    double vbv = 0.0, vv = 0.0;
    for (long long i = 0; i < n; i++) { vbv += u[i] * v[i]; vv += v[i] * v[i]; }
    printf("%.9f\n", sqrt(vbv / vv));
    free(u); free(v); free(w);
    return 0;
}
