/* Same program written by hand in C, compiled with the same flags. */
/* noyau: matmul */
#include <stdio.h>
#include <stdlib.h>

static void matmul(const float *a, const float *b, float *c, long n) {
    for (long i = 0; i < n; i++)
        for (long k = 0; k < n; k++) {
            float aik = a[i * n + k];
            for (long j = 0; j < n; j++) c[i * n + j] = c[i * n + j] + aik * b[k * n + j];
        }
}

int main(void) {
    long n = 256;
    float *a = malloc(n * n * sizeof(float)), *b = malloc(n * n * sizeof(float)), *c = malloc(n * n * sizeof(float));
    for (long i = 0; i < n * n; i++) { a[i] = 1.0f; b[i] = 0.5f; c[i] = 0.0f; }
    for (int r = 0; r < 128; r++) matmul(a, b, c, n);
    char text[32];
    for (int digits = 1; digits <= 9; digits++) {     /* shortest form that reads back the same */
        snprintf(text, sizeof(text), "%.*g", digits, (double)c[n * n - 1]);
        if (strtof(text, NULL) == c[n * n - 1]) break;
    }
    puts(text);
    return 0;
}
