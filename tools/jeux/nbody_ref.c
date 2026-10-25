/* n-body (The Computer Language Benchmarks Game), the same algorithm as nbody.cl, in C. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define NB 5

/* noyau: avance */
static void avance(double *x, double *y, double *z, double *vx, double *vy, double *vz,
                   const double *m, int64_t pas, double dt) {
    for (int64_t p = 0; p < pas; p++) {
        for (int i = 0; i < NB; i++) {
            for (int j = i + 1; j < NB; j++) {
                double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
                double d2 = dx * dx + dy * dy + dz * dz;
                double mag = dt / (d2 * sqrt(d2));
                double mi = m[i] * mag, mj = m[j] * mag;
                vx[i] -= dx * mj; vy[i] -= dy * mj; vz[i] -= dz * mj;
                vx[j] += dx * mi; vy[j] += dy * mi; vz[j] += dz * mi;
            }
        }
        for (int i = 0; i < NB; i++) {
            x[i] += dt * vx[i]; y[i] += dt * vy[i]; z[i] += dt * vz[i];
        }
    }
}

static double energie(const double *x, const double *y, const double *z, const double *vx,
                      const double *vy, const double *vz, const double *m) {
    double e = 0.0;
    for (int i = 0; i < NB; i++) {
        e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
        for (int j = i + 1; j < NB; j++) {
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
            e -= m[i] * m[j] / sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    return e;
}

int main(void) {
    long long n = 0;
    if (scanf("%lld", &n) != 1) return 1;
    const double pi = 3.141592653589793, masse_soleil = 4.0 * pi * pi, jours = 365.24;
    double x[NB] = {0.0, 4.84143144246472090e+00, 8.34336671824457987e+00, 1.28943695621391310e+01, 1.53796971148509165e+01};
    double y[NB] = {0.0, -1.16032004402742839e+00, 4.12479856412430479e+00, -1.51111514016986312e+01, -2.59193146099879641e+01};
    double z[NB] = {0.0, -1.03622044471123109e-01, -4.03523417114321381e-01, -2.23307578892655734e-01, 1.79258772950371181e-01};
    double vx[NB] = {0.0, 1.66007664274403694e-03 * jours, -2.76742510726862411e-03 * jours, 2.96460137564761618e-03 * jours, 2.68067772490389322e-03 * jours};
    double vy[NB] = {0.0, 7.69901118419740425e-03 * jours, 4.99852801234917238e-03 * jours, 2.37847173959480950e-03 * jours, 1.62824170038242295e-03 * jours};
    double vz[NB] = {0.0, -6.90460016972063023e-05 * jours, 2.30417297573763929e-05 * jours, -2.96589568540237556e-05 * jours, -9.51592254519715870e-05 * jours};
    double m[NB] = {masse_soleil, 9.54791938424326609e-04 * masse_soleil, 2.85885980666130812e-04 * masse_soleil, 4.36624404335156298e-05 * masse_soleil, 5.15138902046611451e-05 * masse_soleil};
    double px = 0.0, py = 0.0, pz = 0.0;
    for (int i = 0; i < NB; i++) { px += vx[i] * m[i]; py += vy[i] * m[i]; pz += vz[i] * m[i]; }
    vx[0] = 0.0 - px / masse_soleil; vy[0] = 0.0 - py / masse_soleil; vz[0] = 0.0 - pz / masse_soleil;
    printf("%.9f\n", energie(x, y, z, vx, vy, vz, m));
    avance(x, y, z, vx, vy, vz, m, n, 0.01);
    printf("%.9f\n", energie(x, y, z, vx, vy, vz, m));
    return 0;
}
