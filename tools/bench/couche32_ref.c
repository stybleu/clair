/* Same program written by hand in C, compiled with the same flags. */
/* noyau: couche */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float sigmoid(float v) { return 1.0f / (1.0f + expf(-v)); }

static void couche(const float *w, const float *b, const float *x, float *y, long m, long n) {
    for (long i = 0; i < m; i++) {
        float s = b[i];
        for (long j = 0; j < n; j++) s = s + w[i * n + j] * x[j];
        y[i] = sigmoid(s);
    }
}

int main(void) {
    long n = 256;
    float *w = malloc(n * n * sizeof(float)), *b = malloc(n * sizeof(float));
    float *x = malloc(n * sizeof(float)), *y = malloc(n * sizeof(float));
    for (long i = 0; i < n; i++) {
        for (long j = 0; j < n; j++) w[i * n + j] = (float)((i * 7 + j * 3) % 17) / 17.0f - 0.5f;
        b[i] = (float)(i % 5) / 10.0f;
        x[i] = (float)(i % 3) / 3.0f;
        y[i] = 0.0f;
    }
    for (long r = 0; r < 16000; r++) {
        couche(w, b, x, y, n, n);
        x[r % n] = y[(r * 7) % n];
    }
    float v = y[0] + y[n - 1];
    char text[32];
    for (int digits = 1; digits <= 9; digits++) {     /* shortest form that reads back the same */
        snprintf(text, sizeof(text), "%.*g", digits, (double)v);
        if (strtof(text, NULL) == v) break;
    }
    puts(text);
    return 0;
}
