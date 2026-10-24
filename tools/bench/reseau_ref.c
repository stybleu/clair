/* Same program written by hand in C, compiled with the same flags. */
/* noyau: train */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    long hidden;
    double *w1;   /* hidden x 2, row-major */
    double *b1, *w2, *h;
    double b2;
} Mlp;

static double sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

static double rnd(int64_t *state) {
    *state = (*state * 1103515245 + 12345) % 2147483648;
    return (double)*state / 2147483648.0 * 2.0 - 1.0;
}

static double predict(Mlp *net, double x0, double x1) {
    for (long i = 0; i < net->hidden; i++)
        net->h[i] = sigmoid(net->b1[i] + net->w1[i * 2] * x0 + net->w1[i * 2 + 1] * x1);
    double s = net->b2;
    for (long i = 0; i < net->hidden; i++) s = s + net->w2[i] * net->h[i];
    return sigmoid(s);
}

static double train(Mlp *net, double x0, double x1, double target, double lr) {
    double o = predict(net, x0, x1);
    double err = o - target;
    double d_out = err * o * (1.0 - o);
    for (long i = 0; i < net->hidden; i++) {
        double hi = net->h[i];
        double dh = d_out * net->w2[i] * hi * (1.0 - hi);
        net->w2[i] = net->w2[i] - lr * d_out * hi;
        net->b1[i] = net->b1[i] - lr * dh;
        net->w1[i * 2] = net->w1[i * 2] - lr * dh * x0;
        net->w1[i * 2 + 1] = net->w1[i * 2 + 1] - lr * dh * x1;
    }
    net->b2 = net->b2 - lr * d_out;
    return err * err;
}

int main(void) {
    int64_t state = 42;
    long hidden = 16;
    Mlp net = { hidden, calloc(hidden * 2, sizeof(double)), calloc(hidden, sizeof(double)),
                calloc(hidden, sizeof(double)), calloc(hidden, sizeof(double)), 0.0 };
    for (long i = 0; i < hidden; i++) {
        net.w1[i * 2] = rnd(&state);
        net.w1[i * 2 + 1] = rnd(&state);
    }
    for (long i = 0; i < hidden; i++) net.w2[i] = rnd(&state);

    double x0[4] = { 0.0, 0.0, 1.0, 1.0 }, x1[4] = { 0.0, 1.0, 0.0, 1.0 }, y[4] = { 0.0, 1.0, 1.0, 0.0 };
    double loss = 0.0;
    for (int epoch = 0; epoch < 200000; epoch++) {
        loss = 0.0;
        for (int n = 0; n < 4; n++) loss = loss + train(&net, x0[n], x1[n], y[n], 0.5);
    }
    for (int n = 0; n < 4; n++) printf("%lld\n", (long long)(predict(&net, x0[n], x1[n]) * 100.0 + 0.5));
    printf("%d\n", loss / 4.0 < 0.002);
    return 0;
}
