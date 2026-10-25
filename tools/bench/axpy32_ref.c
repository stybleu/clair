/* Same program written by hand in C, compiled with the same flags. */
/* noyau: axpy */
#include <stdio.h>
#include <stdlib.h>

static void axpy(float alpha, const float *x, float *y, long n) {
    for (long i = 0; i < n; i++) y[i] = alpha * x[i] + y[i];
}

int main(void) {
    long n = 4096;
    float *x = malloc(n * sizeof(float)), *y = malloc(n * sizeof(float));
    for (long i = 0; i < n; i++) { x[i] = 1.0f; y[i] = 0.0f; }
    for (int r = 0; r < 400000; r++) axpy(0.000001f, x, y, n);
    char text[32];
    for (int digits = 1; digits <= 9; digits++) {     /* shortest form that reads back the same */
        snprintf(text, sizeof(text), "%.*g", digits, (double)y[4095]);
        if (strtof(text, NULL) == y[4095]) break;
    }
    puts(text);
    return 0;
}
