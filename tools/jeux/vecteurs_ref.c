/* vecteurs, le meme calcul que vecteurs.cl, en C : des structures passees par valeur. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { double x, y, z; } Vec;

static Vec plus(Vec a, Vec o) { return (Vec){ a.x + o.x, a.y + o.y, a.z + o.z }; }
static Vec fois(Vec a, double k) { return (Vec){ a.x * k, a.y * k, a.z * k }; }
static double norme(Vec a) { return sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

/* noyau: vole */
static Vec vole(int64_t n, double dt) {
    Vec p = { 0.0, 0.0, 1.5 };
    Vec v = { 30.0, 20.0, 25.0 };
    Vec g = { 0.0, 0.0, -9.81 };
    double chemin = 0.0;
    for (int64_t i = 0; i < n; i++) {
        Vec a = plus(g, fois(v, -0.02 * norme(v)));
        v = plus(v, fois(a, dt));
        Vec d = fois(v, dt);
        p = plus(p, d);
        chemin += norme(d);
    }
    printf("%.9f\n", chemin);
    return p;
}

int main(void) {
    int64_t n;
    if (scanf("%ld", &n) != 1 || n <= 0) return 1;
    Vec p = vole(n, 4.0 / n);
    printf("%.9f\n%.9f\n%.9f\n", p.x, p.y, p.z);
    return 0;
}
