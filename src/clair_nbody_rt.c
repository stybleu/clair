/*
 * V0.66: the module nbody (import nbody): N bodies pulled by Newton's gravity.
 * V0.68: IAS15 (s.integrator = "ias15"), the orbital elements, the solar system.
 *
 * Copied into the C of the programs that import nbody (tools/embed_file.cmake), after the
 * runtime of gui (nbody imports gui: the window of s.show() is the window of gui).
 *
 * The bodies live in the lists of the Clair object Simulation (x, y, z, vx, vy, vz, m, r);
 * each method first gives them to the C part (__nbody_use), which changes them in place.
 *
 * The steps: the leapfrog (kick, drift, kick: symplectic, the energy does not drift away over
 * millions of steps), or Yoshida's fourth order (three leapfrogs: much more precise for the same
 * time step).  The forces: each pair once (Newton's third law), with a softening length eps:
 * a = G m d / (|d|^2 + eps^2)^(3/2).  A collision (s.merge): two bodies that touch (their radii)
 * become one, with their mass and their momentum; the volume is kept.  The other one gets a
 * mass of 0 and a radius of -1, and stays in the lists (the numbers of the bodies do not change).
 */

static struct {
    ClairList x, y, z, vx, vy, vz, m, r;           /* the lists of the simulation */
    ClairList names, colors;
    double *ax, *ay, *az;
    size_t cap;
    double dt, speed, trail, tscale, lscale, G, soft, t, epsilon;
    int flags;
    char tunit[32], lunit[32];
} clair_nb;

_Noreturn static void clair_nbody_die(const char *what) {
    fprintf(stderr, "Clair runtime error: nbody: %s\n", what);
    exit(EXIT_FAILURE);
}

static int64_t clair_nbody_fail(const char *what) { clair_nbody_die(what); }

static size_t clair_nbody_n(void) { return clair_nb.m ? clair_nb.m->len : 0; }

#define CLAIR_NB(list) ((double *)clair_nb.list->data)

static int64_t clair_nbody_use(ClairList x, ClairList y, ClairList z, ClairList vx, ClairList vy, ClairList vz, ClairList m, ClairList r) {
    size_t n = m ? m->len : 0;
    ClairList all[8] = { x, y, z, vx, vy, vz, m, r };
    for (int i = 0; i < 8; i++)
        if (!all[i] || all[i]->len != n) clair_nbody_die("the lists x, y, z, vx, vy, vz, m and r must have one value per body");
    clair_nb.x = x; clair_nb.y = y; clair_nb.z = z;
    clair_nb.vx = vx; clair_nb.vy = vy; clair_nb.vz = vz;
    clair_nb.m = m; clair_nb.r = r;
    if (n > clair_nb.cap) {
        size_t cap = n < 64 ? 64 : n;
        double *a = realloc(clair_nb.ax, cap * sizeof(double)), *b = a ? realloc(clair_nb.ay, cap * sizeof(double)) : NULL;
        if (a) clair_nb.ax = a;
        if (b) clair_nb.ay = b;
        double *c = b ? realloc(clair_nb.az, cap * sizeof(double)) : NULL;
        if (!a || !b || !c) clair_nbody_die("out of memory");
        clair_nb.az = c;
        clair_nb.cap = cap;
    }
    return (int64_t)n;
}

/* the accelerations of every body (the absorbed ones: none) */
static void clair_nbody_forces(double G, double eps2) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double *ax = clair_nb.ax, *ay = clair_nb.ay, *az = clair_nb.az;
    for (size_t i = 0; i < n; i++) ax[i] = ay[i] = az[i] = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        double xi = x[i], yi = y[i], zi = z[i], gmi = G * m[i];
        double axi = 0, ayi = 0, azi = 0;
        for (size_t j = i + 1; j < n; j++) {
            if (r[j] < 0) continue;
            double dx = x[j] - xi, dy = y[j] - yi, dz = z[j] - zi;
            double d2 = dx * dx + dy * dy + dz * dz + eps2;
            if (d2 == 0) continue;                         /* two bodies at the same place, without softening */
            double inv = 1.0 / (d2 * sqrt(d2));
            double gj = G * m[j] * inv, gi = gmi * inv;
            axi += gj * dx; ayi += gj * dy; azi += gj * dz;
            ax[j] -= gi * dx; ay[j] -= gi * dy; az[j] -= gi * dz;
        }
        ax[i] += axi; ay[i] += ayi; az[i] += azi;
    }
}

static void clair_nbody_drift(double h) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z);
    const double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz), *r = CLAIR_NB(r);
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        x[i] += h * vx[i]; y[i] += h * vy[i]; z[i] += h * vz[i];
    }
}

static void clair_nbody_kick(double h) {
    size_t n = clair_nbody_n();
    double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *r = CLAIR_NB(r);
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        vx[i] += h * clair_nb.ax[i]; vy[i] += h * clair_nb.ay[i]; vz[i] += h * clair_nb.az[i];
    }
}

/* the bodies that touch become one; gives how many collisions */
static int64_t clair_nbody_collide(void) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    int64_t hits = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] <= 0) continue;                           /* a body without a size never touches */
        for (size_t j = i + 1; j < n; j++) {
            if (r[j] <= 0) continue;
            double dx = x[j] - x[i], dy = y[j] - y[i], dz = z[j] - z[i], s = r[i] + r[j];
            if (dx * dx + dy * dy + dz * dz >= s * s) continue;
            double M = m[i] + m[j];
            double a = M > 0 ? m[i] / M : 0.5, b = 1 - a;
            size_t keep = m[j] > m[i] ? j : i, gone = keep == i ? j : i;
            x[keep] = a * x[i] + b * x[j]; y[keep] = a * y[i] + b * y[j]; z[keep] = a * z[i] + b * z[j];
            vx[keep] = a * vx[i] + b * vx[j]; vy[keep] = a * vy[i] + b * vy[j]; vz[keep] = a * vz[i] + b * vz[j];
            m[keep] = M;
            r[keep] = cbrt(r[i] * r[i] * r[i] + r[j] * r[j] * r[j]);
            m[gone] = 0;
            r[gone] = -1;
            vx[gone] = vy[gone] = vz[gone] = 0;
            hits++;
            if (gone == i) break;
        }
    }
    return hits;
}

/* n steps of h; flags: 1 collisions, 2 Yoshida's fourth order. Gives the number of collisions. */
static int64_t clair_nbody_step(double h, int64_t steps, double G, double soft, int64_t flags) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    double eps2 = soft * soft;
    int64_t hits = 0;
    static const double w1 = 1.3512071919596578, w0 = -1.7024143839193153;     /* 1/(2-2^(1/3)), -2^(1/3)/(2-2^(1/3)) */
    if (!(flags & 2)) {
        clair_nbody_forces(G, eps2);
        for (int64_t s = 0; s < steps; s++) {
            clair_nbody_kick(h / 2);
            clair_nbody_drift(h);
            if (flags & 1) {
                int64_t k = clair_nbody_collide();
                hits += k;
            }
            clair_nbody_forces(G, eps2);
            clair_nbody_kick(h / 2);
        }
    } else {
        const double c[4] = { w1 / 2, (w0 + w1) / 2, (w0 + w1) / 2, w1 / 2 }, d[3] = { w1, w0, w1 };
        for (int64_t s = 0; s < steps; s++) {
            for (int k = 0; k < 3; k++) {
                clair_nbody_drift(c[k] * h);
                clair_nbody_forces(G, eps2);
                clair_nbody_kick(d[k] * h);
            }
            clair_nbody_drift(c[3] * h);
            if (flags & 1) hits += clair_nbody_collide();
        }
    }
    return hits;
}

/* 0 the kinetic energy, 1 the potential energy */
static double clair_nbody_energy(double G, double soft, int64_t what) {
    size_t n = clair_nbody_n();
    if (!n) return 0;
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double e = 0;
    if (what == 0) {
        for (size_t i = 0; i < n; i++) if (r[i] >= 0) e += 0.5 * m[i] * (vx[i] * vx[i] + vy[i] * vy[i] + vz[i] * vz[i]);
        return e;
    }
    double eps2 = soft * soft;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0 || m[i] == 0) continue;
        for (size_t j = i + 1; j < n; j++) {
            if (r[j] < 0 || m[j] == 0) continue;
            double dx = x[j] - x[i], dy = y[j] - y[i], dz = z[j] - z[i];
            double d = sqrt(dx * dx + dy * dy + dz * dz + eps2);
            if (d > 0) e -= G * m[i] * m[j] / d;
        }
    }
    return e;
}

/* 0-2 the momentum, 3-5 the angular momentum, 6-8 the center of mass, 9-11 its speed, 12 the total mass */
static double clair_nbody_sum(int64_t what) {
    size_t n = clair_nbody_n();
    if (!n) return 0;
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double s[13] = { 0 };
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        double px = m[i] * vx[i], py = m[i] * vy[i], pz = m[i] * vz[i];
        s[0] += px; s[1] += py; s[2] += pz;
        s[3] += y[i] * pz - z[i] * py; s[4] += z[i] * px - x[i] * pz; s[5] += x[i] * py - y[i] * px;
        s[6] += m[i] * x[i]; s[7] += m[i] * y[i]; s[8] += m[i] * z[i];
        s[12] += m[i];
    }
    if (what >= 6 && what <= 11) {
        if (s[12] == 0) return 0;
        if (what <= 8) return s[what] / s[12];
        return s[what - 9] / s[12];
    }
    return what >= 0 && what <= 12 ? s[what] : 0;
}

/* moves everything so that the center of mass is at 0 and does not move */
static int64_t clair_nbody_center(void) {
    size_t n = clair_nbody_n();
    double c[6];
    for (int k = 0; k < 6; k++) c[k] = clair_nbody_sum(6 + k);
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *r = CLAIR_NB(r);
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        x[i] -= c[0]; y[i] -= c[1]; z[i] -= c[2];
        vx[i] -= c[3]; vy[i] -= c[4]; vz[i] -= c[5];
    }
    return 0;
}

/* the shortest time a pair of bodies takes to fall together: sqrt(d^3 / (G (mi + mj))) */
static double clair_nbody_tau(double G, double soft) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double best = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        for (size_t j = i + 1; j < n; j++) {
            if (r[j] < 0 || m[i] + m[j] <= 0) continue;
            double dx = x[j] - x[i], dy = y[j] - y[i], dz = z[j] - z[i];
            double d2 = dx * dx + dy * dy + dz * dz + soft * soft;
            if (d2 <= 0 || G <= 0) continue;
            double t = sqrt(d2 * sqrt(d2) / (G * (m[i] + m[j])));
            if (best == 0 || t < best) best = t;
        }
    }
    return best;
}

static int64_t clair_nbody_look(ClairList names, ClairList colors) {
    clair_nb.names = names;
    clair_nb.colors = colors;
    return 0;
}

/* 0 dt, 1 speed, 2 trail, 3 time scale, 4 length scale, 5 G, 6 softening, 7 flags (4: IAS15), 8 t, 9 epsilon */
static int64_t clair_nbody_param(int64_t which, double v) {
    switch (which) {
        case 0: clair_nb.dt = v; break;
        case 1: clair_nb.speed = v; break;
        case 2: clair_nb.trail = v; break;
        case 3: clair_nb.tscale = v > 0 ? v : 1; break;
        case 4: clair_nb.lscale = v > 0 ? v : 1; break;
        case 5: clair_nb.G = v; break;
        case 6: clair_nb.soft = v; break;
        case 7: clair_nb.flags = (int)v; break;
        case 8: clair_nb.t = v; break;
        case 9: clair_nb.epsilon = v; break;          /* V0.68: the precision of IAS15 */
        default: break;
    }
    return 0;
}

/* 0 the name of the time unit, 1 the name of the length unit */
static int64_t clair_nbody_unit(int64_t which, const char *name) {
    char *d = which == 0 ? clair_nb.tunit : clair_nb.lunit;
    snprintf(d, sizeof(clair_nb.tunit), "%s", name ? name : "");
    return 0;
}

/* ---- V0.68: IAS15, the integrator of order 15 with its own time step (s.integrator = "ias15") ----
 *
 * After Rein & Spiegel (2015), "IAS15: a fast, adaptive, high-order integrator for gravitational
 * dynamics, accurate to machine precision over a billion orbits", MNRAS 446, 1424, and for the
 * time step, Pham, Rein & Spiegel (2024), "Re-evaluating the timestep criterion of IAS15", OJAp 7.
 * Written here from the articles (REBOUND, where IAS15 comes from, is under the GPL; Clair is MIT).
 *
 * In a step of dt, the acceleration is a polynomial of the fraction s of the step:
 *     a(s) = a0 + b0 s + b1 s^2 + ... + b6 s^7
 * The positions and the velocities are its integrals.  The b are found by iterating (predictor-
 * corrector) on the forces at the 7 Gauss-Radau points of the step, through the same polynomial in
 * Newton's form (the g).  The size of the next step comes from the acceleration, its jerk and its
 * snap at the end of the step (PRS23); a step that wanted to be 4 times smaller is done again.
 * Sums of many small terms keep what rounding loses (Kahan), so the error stays at the level of
 * the doubles.  The constants are computed (tools: Legendre polynomials, see docs/README_V0.68.md).
 */

static const double clair_ias_h[8] = { 0.0000000000000000000000000e+00, 5.6262560536922146465652191e-2, 1.8024069173689236498757994e-1, 3.5262471711316963737390777e-1, 5.4715362633055538300144856e-1, 7.3421017721541053152321061e-1, 8.8532094683909576809035976e-1, 9.7752061356128750189117450e-1 };
static const double clair_ias_rr[28] = { 5.6262560536922146465652191e-2, 1.8024069173689236498757994e-1, 1.2397813119997021852192775e-1, 3.5262471711316963737390777e-1, 2.9636215657624749090825558e-1, 1.7238402537627727238632783e-1, 5.4715362633055538300144856e-1, 4.9089106579363323653579637e-1, 3.6691293459366301801386861e-1, 1.9452890921738574562754079e-1, 7.3421017721541053152321061e-1, 6.7794761667848838505755842e-1, 5.5396948547851816653563067e-1, 3.8158546010224089414930284e-1, 1.8705655088485514852176205e-1, 8.8532094683909576809035976e-1, 8.2905838630217362162470757e-1, 7.0508025510220340310277982e-1, 5.3269622972592613071645199e-1, 3.3816732050854038508891121e-1, 1.5111076962368523656714915e-1, 9.7752061356128750189117450e-1, 9.2125805302436535542552231e-1, 7.9727992182439513690359456e-1, 6.2489589644811786451726673e-1, 4.3036698723073211888972594e-1, 2.4331043634587697036796389e-1, 9.2199666722191733800814737e-2 };
static const double clair_ias_c[21] = { -5.6262560536922146465652191e-2, 1.0140802830063629986481805e-2, -2.3650325227381451145323213e-1, -3.5758977292516175949344589e-3, 9.3537695259462065895748461e-2, -5.8912796938698414882713990e-1, 1.9565654099472210769005671e-3, -5.4755386889068686440808430e-2, 4.1588120008230686168862191e-1, -1.1362815957175395318285885e+0, -1.4365302363708915424459553e-3, 4.2158527721268707707297347e-2, -3.6009959650205681228976646e-1, 1.2501507118406910258505441e+0, -1.8704917729329500633517991e+0, 1.2717903090268677492943116e-3, -3.8760357915906770369904625e-2, 3.6096224345284598322533981e-1, -1.4668842084004269643701553e+0, 2.9061362593084293014237913e+0, -2.7558127197720458314421588e+0 };
static const double clair_ias_d[21] = { 5.6262560536922146465652191e-2, 3.1654757181708292499904800e-3, 2.3650325227381451145323213e-1, 1.7809776922174338811252792e-4, 4.5792985506027918895453873e-2, 5.8912796938698414882713990e-1, 1.0020236522329127209567215e-5, 8.4318571535257015444999742e-3, 2.5353406905456926652146160e-1, 1.1362815957175395318285885e+0, 5.6376416393182076103838501e-7, 1.5297840025004658189490080e-3, 9.7834236532444005365364840e-2, 8.7525466468409109122972459e-1, 1.8704917729329500633517991e+0, 3.1718815401761366475854818e-8, 2.7629309098264765931302264e-4, 3.6028553983736459600387074e-2, 5.7673300027707873135445961e-1, 2.2485887607691597933926895e+0, 2.7558127197720458314421588e+0 };

static struct {
    size_t n3, cap;
    double *mem;
    double *x0, *v0, *a0, *at, *csx, *csv, *xs;
    double *g[7], *b[7], *e[7], *br[7], *er[7], *cb[7];   /* cb: what rounding took from the b */
    double dt_last;                                  /* the last step done (0: none yet) */
    const void *owner;                               /* the simulation the predictions belong to */
    int64_t steps, rejected, unconverged;
    double out[12];                                  /* what __nbody_out(i) gives */
} clair_ias;

static double clair_nbody_out(int64_t i) { return i >= 0 && i < 12 ? clair_ias.out[i] : 0; }

/* 0 the IAS15 steps done, 1 the steps done again, 2 the predictor-corrector that did not settle */
static double clair_nbody_stat(int64_t what) {
    return what == 0 ? (double)clair_ias.steps : what == 1 ? (double)clair_ias.rejected : (double)clair_ias.unconverged;
}

/* the predictions start again from nothing (a collision, other bodies) */
static void clair_ias_forget(void) {
    if (clair_ias.mem) memset(clair_ias.mem, 0, clair_ias.cap * 49 * sizeof(double));
    clair_ias.dt_last = 0;
}

static void clair_ias_ready(void) {
    size_t n3 = 3 * clair_nbody_n();
    if (n3 > clair_ias.cap) {
        double *mem = calloc(n3 * 49, sizeof(double));      /* 7 vectors + 6 polynomials of 7 */
        if (!mem) clair_nbody_die("out of memory");
        free(clair_ias.mem);
        clair_ias.mem = mem;
        clair_ias.cap = n3;
        clair_ias.owner = NULL;
    }
    double *p = clair_ias.mem;
    size_t c = clair_ias.cap;
    clair_ias.x0 = p; clair_ias.v0 = p + c; clair_ias.a0 = p + 2 * c; clair_ias.at = p + 3 * c;
    clair_ias.csx = p + 4 * c; clair_ias.csv = p + 5 * c; clair_ias.xs = p + 6 * c;
    for (int k = 0; k < 7; k++) {
        clair_ias.g[k] = p + (7 + k) * c; clair_ias.b[k] = p + (14 + k) * c; clair_ias.e[k] = p + (21 + k) * c;
        clair_ias.br[k] = p + (28 + k) * c; clair_ias.er[k] = p + (35 + k) * c; clair_ias.cb[k] = p + (42 + k) * c;
    }
    if (clair_ias.owner != (const void *)clair_nb.x || clair_ias.n3 != n3) {   /* another simulation, or other bodies */
        clair_ias_forget();
        clair_ias.owner = clair_nb.x;
        clair_ias.n3 = n3;
        clair_ias.steps = clair_ias.rejected = clair_ias.unconverged = 0;
        const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
        for (size_t i = 0; i < n3 / 3; i++) {
            clair_ias.x0[3 * i] = x[i]; clair_ias.x0[3 * i + 1] = y[i]; clair_ias.x0[3 * i + 2] = z[i];
            clair_ias.v0[3 * i] = vx[i]; clair_ias.v0[3 * i + 1] = vy[i]; clair_ias.v0[3 * i + 2] = vz[i];
        }
    }
}

/* a += b, keeping in *c what the rounding lost (Kahan) */
static void clair_ias_add(double *a, double *c, double b) {
    double y = b - *c, t = *a + y;
    *c = (t - *a) - y;
    *a = t;
}

static void clair_ias_put(const double *pos) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z);
    for (size_t i = 0; i < n; i++) { x[i] = pos[3 * i]; y[i] = pos[3 * i + 1]; z[i] = pos[3 * i + 2]; }
}

static void clair_ias_accel(double G, double eps2, double *out) {
    size_t n = clair_nbody_n();
    clair_nbody_forces(G, eps2);
    for (size_t i = 0; i < n; i++) { out[3 * i] = clair_nb.ax[i]; out[3 * i + 1] = clair_nb.ay[i]; out[3 * i + 2] = clair_nb.az[i]; }
}

/* the b and e of the next step, from those of the last one (eb, bb), for a step ratio times longer */
static void clair_ias_predict(double ratio) {
    size_t n3 = clair_ias.n3;
    if (ratio > 20) {                                 /* too far to guess: start from nothing */
        for (int k = 0; k < 7; k++) { memset(clair_ias.e[k], 0, n3 * sizeof(double)); memset(clair_ias.b[k], 0, n3 * sizeof(double)); }
        return;
    }
    static const double binom[7][7] = {              /* binom[j][i] = C(i+1, j+1) */
        { 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 3, 6, 10, 15, 21 }, { 0, 0, 1, 4, 10, 20, 35 }, { 0, 0, 0, 1, 5, 15, 35 },
        { 0, 0, 0, 0, 1, 6, 21 }, { 0, 0, 0, 0, 0, 1, 7 }, { 0, 0, 0, 0, 0, 0, 1 } };
    double q[7];
    q[0] = ratio;
    for (int j = 1; j < 7; j++) q[j] = q[j - 1] * ratio;
    for (size_t k = 0; k < n3; k++) {
        double bb[7], be[7];
        for (int j = 0; j < 7; j++) { bb[j] = clair_ias.br[j][k]; be[j] = bb[j] - clair_ias.er[j][k]; }
        for (int j = 0; j < 7; j++) {
            double s = 0;
            for (int i = j; i < 7; i++) s += binom[j][i] * bb[i];
            clair_ias.e[j][k] = q[j] * s;
            clair_ias.b[j][k] = q[j] * s + be[j];
        }
    }
}

/* one step of IAS15 from the bodies as they are, wanting *dt; 1: done (*done: its length, *dt: the next one),
   0: refused (*dt: smaller, to try again) */
static int clair_ias_step(double G, double eps2, double epsilon, double *dt, double *done) {
    clair_ias_ready();
    size_t n = clair_nbody_n(), n3 = 3 * n;
    double *x0 = clair_ias.x0, *v0 = clair_ias.v0, *a0 = clair_ias.a0, *at = clair_ias.at, *xs = clair_ias.xs;
    double *csx = clair_ias.csx, *csv = clair_ias.csv;
    double **g = clair_ias.g, **b = clair_ias.b, **cb = clair_ias.cb;
    for (int k = 0; k < 7; k++) memset(cb[k], 0, n3 * sizeof(double));
    {   /* the program may have moved the bodies since the last step: then what was kept no longer counts */
        double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
        for (size_t i = 0; i < n; i++) {
            double now[6] = { x[i], y[i], z[i], vx[i], vy[i], vz[i] };
            for (int k = 0; k < 3; k++) {
                if (x0[3 * i + k] != now[k]) { x0[3 * i + k] = now[k]; csx[3 * i + k] = 0; }
                if (v0[3 * i + k] != now[3 + k]) { v0[3 * i + k] = now[3 + k]; csv[3 * i + k] = 0; }
            }
        }
    }
    double h = *dt;
    clair_ias_accel(G, eps2, a0);
    for (size_t k = 0; k < n3; k++) {                 /* the g from the b (Newton's form) */
        for (int j = 0; j < 7; j++) {
            double s = b[j][k];
            for (int i = j + 1; i < 7; i++) s += clair_ias_d[i * (i - 1) / 2 + j] * b[i][k];
            g[j][k] = s;
        }
    }
    double err = 1e300, err_last = 2;
    int iterations = 0;
    for (;;) {                                        /* predictor-corrector: until the b no longer change */
        if (err < 1e-16) break;
        if (iterations > 2 && err_last <= err) break;
        if (iterations >= 12) { clair_ias.unconverged++; break; }
        err_last = err;
        err = 0;
        iterations++;
        for (int st = 1; st < 8; st++) {
            double hs = clair_ias_h[st], ht = h * hs;
            for (size_t k = 0; k < n3; k++) {             /* where the body is at s = hs: the moved length first (Horner) */
                double d = b[6][k] * (7 * hs / 9) + b[5][k];
                d = d * (3 * hs / 4) + b[4][k];
                d = d * (5 * hs / 7) + b[3][k];
                d = d * (2 * hs / 3) + b[2][k];
                d = d * (3 * hs / 5) + b[1][k];
                d = d * (hs / 2) + b[0][k];
                d = d * (hs / 3) + a0[k];
                d = d * (ht / 2) + v0[k];
                xs[k] = (d * ht - csx[k]) + x0[k];
            }
            clair_ias_put(xs);
            clair_ias_accel(G, eps2, at);
            int gi = st - 1, rbase = st * (st - 1) / 2, cbase = (st - 1) * (st - 2) / 2;
            double maxa = 0, maxdb = 0;
            for (size_t k = 0; k < n3; k++) {
                double dk = at[k], cdk = 0;                 /* at - a0, with what its rounding lost put back */
                clair_ias_add(&dk, &cdk, -a0[k]);
                clair_ias_add(&dk, &cdk, 0);
                double v = dk / clair_ias_rr[rbase];
                for (int m = 1; m < st; m++) v = (v - g[m - 1][k]) / clair_ias_rr[rbase + m];
                double change = v - g[gi][k];
                g[gi][k] = v;
                for (int j = 0; j < gi; j++) clair_ias_add(&b[j][k], &cb[j][k], change * clair_ias_c[cbase + j]);
                clair_ias_add(&b[gi][k], &cb[gi][k], change);
                if (st == 7) {
                    double a = fabs(at[k]), d = fabs(change);
                    if (isnormal(a) && a > maxa) maxa = a;
                    if (isnormal(d) && d > maxdb) maxdb = d;
                }
            }
            if (st == 7) err = maxa > 0 ? maxdb / maxa : 0;
        }
    }
    double dt_done = h, dt_new = h;
    if (epsilon > 0) {                                /* the next step (PRS23): the acceleration, jerk and snap at the end */
        double best = INFINITY;
        for (size_t i = 0; i < n; i++) {
            double a2 = 0, y2 = 0, y3 = 0, y4 = 0;
            for (size_t k = 3 * i; k < 3 * i + 3; k++) {
                a2 += a0[k] * a0[k];
                double t = a0[k] + b[0][k] + b[1][k] + b[2][k] + b[3][k] + b[4][k] + b[5][k] + b[6][k];
                y2 += t * t;
                t = b[0][k] + 2 * b[1][k] + 3 * b[2][k] + 4 * b[3][k] + 5 * b[4][k] + 6 * b[5][k] + 7 * b[6][k];
                y3 += t * t;
                t = 2 * b[1][k] + 6 * b[2][k] + 12 * b[3][k] + 20 * b[4][k] + 30 * b[5][k] + 42 * b[6][k];
                y4 += t * t;
            }
            if (!isnormal(a2)) continue;              /* no force on it */
            double ts2 = 2 * y2 / (y3 + sqrt(y4 * y2));
            if (isnormal(ts2) && ts2 < best) best = ts2;
        }
        dt_new = isnormal(best) ? sqrt(best) * dt_done * pow(epsilon * 5040.0, 1.0 / 7) : dt_done * 4;
        if (fabs(dt_new / dt_done) < 0.25) {          /* much too long: again, shorter */
            clair_ias_put(x0);
            clair_ias.rejected++;
            *dt = dt_new;
            if (clair_ias.dt_last != 0) clair_ias_predict(dt_new / clair_ias.dt_last);
            else for (int k = 0; k < 7; k++) { memset(b[k], 0, n3 * sizeof(double)); memset(clair_ias.e[k], 0, n3 * sizeof(double)); }
            return 0;
        }
        if (dt_new / dt_done > 4) dt_new = dt_done * 4;
    }
    for (size_t k = 0; k < n3; k++) {                 /* the end of the step: the smallest terms first */
        double h2 = dt_done * dt_done;
        clair_ias_add(&x0[k], &csx[k], b[6][k] / 72 * h2);
        clair_ias_add(&x0[k], &csx[k], b[5][k] / 56 * h2);
        clair_ias_add(&x0[k], &csx[k], b[4][k] / 42 * h2);
        clair_ias_add(&x0[k], &csx[k], b[3][k] / 30 * h2);
        clair_ias_add(&x0[k], &csx[k], b[2][k] / 20 * h2);
        clair_ias_add(&x0[k], &csx[k], b[1][k] / 12 * h2);
        clair_ias_add(&x0[k], &csx[k], b[0][k] / 6 * h2);
        clair_ias_add(&x0[k], &csx[k], a0[k] / 2 * h2);
        clair_ias_add(&x0[k], &csx[k], v0[k] * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[6][k] / 8 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[5][k] / 7 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[4][k] / 6 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[3][k] / 5 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[2][k] / 4 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[1][k] / 3 * dt_done);
        clair_ias_add(&v0[k], &csv[k], b[0][k] / 2 * dt_done);
        clair_ias_add(&v0[k], &csv[k], a0[k] * dt_done);
    }
    {
        double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
        clair_ias_put(x0);
        for (size_t i = 0; i < n; i++) { vx[i] = v0[3 * i]; vy[i] = v0[3 * i + 1]; vz[i] = v0[3 * i + 2]; }
    }
    clair_ias.dt_last = dt_done;
    clair_ias.steps++;
    for (int k = 0; k < 7; k++) {
        memcpy(clair_ias.er[k], clair_ias.e[k], n3 * sizeof(double));
        memcpy(clair_ias.br[k], b[k], n3 * sizeof(double));
    }
    clair_ias_predict(dt_new / dt_done);
    *done = dt_done;
    *dt = dt_new;
    return 1;
}

/* IAS15 for the time duration (exactly), or one step when duration < 0; flags & 1: collisions.
   budget_ns > 0: stops when the time is spent (the window).  Gives the collisions;
   out: 0 the next step, 1 the time done, 2 the steps done so far, 3 the steps done again */
static int64_t clair_ias_run(double duration, double G, double soft, int64_t flags, double epsilon, double dt, uint64_t budget_ns) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    if (clair_nbody_n() == 0) {                       /* no body: only the time goes by */
        clair_ias.out[0] = dt;
        clair_ias.out[1] = duration > 0 ? duration : 0;
        return 0;
    }
    clair_ias_ready();
    double eps2 = soft * soft, t = 0, ct = 0;           /* ct: what rounding took from the time (Kahan) */
    int64_t hits = 0;
    if (!(dt > 0) || !isfinite(dt)) dt = clair_nbody_tau(G, soft) / 500;
    if (!(dt > 0) || !isfinite(dt)) dt = duration > 0 ? duration / 100 : 1e-3;
    uint64_t start = budget_ns ? clair_sdl.GetTicksNS() : 0;
    int one = duration < 0;
    while (one || t < duration) {
        double want = dt, done = 0;
        int last = !one && t + want >= duration;
        if (last) want = (duration - t) + ct;
        double trying = want;
        if (!clair_ias_step(G, eps2, epsilon, &trying, &done)) {
            if (!(trying > 0) || t + trying == t) clair_nbody_die("IAS15: the time step became 0 (two bodies on top of each other? s.soft)");
            dt = trying;
            continue;
        }
        if (last) t = duration;                       /* the last step ends exactly there */
        else clair_ias_add(&t, &ct, done);
        if (!last || trying < dt) dt = trying;        /* a last step cut short does not shrink the next ones */
        if (flags & 1) {
            int64_t k = clair_nbody_collide();
            if (k) { hits += k; clair_ias_forget(); }
        }
        if (one) break;
        if (budget_ns && clair_sdl.GetTicksNS() - start > budget_ns) break;
    }
    clair_ias.out[0] = dt;
    clair_ias.out[1] = t;
    clair_ias.out[2] = (double)clair_ias.steps;
    clair_ias.out[3] = (double)clair_ias.rejected;
    return hits;
}

static int64_t clair_nbody_ias15(double duration, double G, double soft, int64_t flags, double epsilon, double dt) {
    return clair_ias_run(duration, G, soft, flags, epsilon, dt, 0);
}

/* ---- V0.68: the orbital elements ---- */

#define CLAIR_NB_DEG (3.141592653589793238 / 180)

/* Kepler's equation: the true anomaly (degrees) for the mean anomaly M (degrees) */
static double clair_nbody_anomaly(double M, double e) {
    if (e < 0) clair_nbody_die("an eccentricity is not negative");
    double m = M * CLAIR_NB_DEG;
    if (e < 1) {
        m = fmod(m, 2 * 3.141592653589793238);
        double E = e < 0.8 ? m : 3.141592653589793238;
        for (int i = 0; i < 100; i++) {
            double d = (E - e * sin(E) - m) / (1 - e * cos(E));
            E -= d;
            if (fabs(d) < 1e-15) break;
        }
        return 2 * atan2(sqrt(1 + e) * sin(E / 2), sqrt(1 - e) * cos(E / 2)) / CLAIR_NB_DEG;
    }
    if (e == 1) clair_nbody_die("an orbit with e = 1 (a parabola) has no mean anomaly here");
    double F = asinh(m / e);
    for (int i = 0; i < 200; i++) {
        double d = (e * sinh(F) - F - m) / (e * cosh(F) - 1);
        F -= d;
        if (fabs(d) < 1e-15 * (1 + fabs(F))) break;
    }
    return 2 * atan(sqrt((e + 1) / (e - 1)) * tanh(F / 2)) / CLAIR_NB_DEG;
}

/* the place and the speed relative to the body turned around (out 0-5), for G (M + m) = mu,
   a, e, the inclination, the node, the pericenter, the true anomaly (degrees) */
static int64_t clair_nbody_orbit(double mu, double a, double e, double inc, double node, double peri, double f) {
    if (!(mu > 0)) clair_nbody_die("add_orbit(): G and the masses must give more than 0");
    if (e < 0) clair_nbody_die("add_orbit(): the eccentricity is not negative");
    if (e == 1) clair_nbody_die("add_orbit(): e = 1 (a parabola): use a slightly different e");
    if ((e < 1 && !(a > 0)) || (e > 1 && !(a < 0))) clair_nbody_die("add_orbit(): an ellipse (e < 1) needs a > 0, a hyperbola (e > 1) a < 0");
    double p = a * (1 - e * e);
    double cf = cos(f * CLAIR_NB_DEG), sf = sin(f * CLAIR_NB_DEG);
    if (1 + e * cf <= 0) clair_nbody_die("add_orbit(): this true anomaly is not on the hyperbola");
    double r = p / (1 + e * cf), s = sqrt(mu / p);
    double ci = cos(inc * CLAIR_NB_DEG), si = sin(inc * CLAIR_NB_DEG);
    double cO = cos(node * CLAIR_NB_DEG), sO = sin(node * CLAIR_NB_DEG);
    double cw = cos(peri * CLAIR_NB_DEG), sw = sin(peri * CLAIR_NB_DEG);
    double P[3] = { cO * cw - sO * sw * ci, sO * cw + cO * sw * ci, sw * si };
    double Q[3] = { -cO * sw - sO * cw * ci, -sO * sw + cO * cw * ci, cw * si };
    for (int k = 0; k < 3; k++) {
        clair_ias.out[k] = r * cf * P[k] + r * sf * Q[k];
        clair_ias.out[3 + k] = -s * sf * P[k] + s * (e + cf) * Q[k];
    }
    return 0;
}

static double clair_nbody_angle(double rad) {
    double d = fmod(rad / CLAIR_NB_DEG, 360);
    return d < 0 ? d + 360 : d;
}

/* the orbit of the body i around the body j (j = -1: the center of mass of the bodies before i):
   out 0 a, 1 e, 2 inclination, 3 node, 4 pericenter, 5 true anomaly, 6 mean anomaly (degrees), 7 period, 8 distance */
static int64_t clair_nbody_elements(int64_t i, int64_t j, double G) {
    size_t n = clair_nbody_n();
    if (i < 0 || (size_t)i >= n || j >= (int64_t)n || j == i || j < -1) clair_nbody_die("elements(): no such body");
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *rad = CLAIR_NB(r);
    double M = 0, c[6] = { 0 };
    if (j >= 0) {
        M = m[j];
        c[0] = x[j]; c[1] = y[j]; c[2] = z[j]; c[3] = vx[j]; c[4] = vy[j]; c[5] = vz[j];
    } else {
        for (int64_t k = 0; k < i; k++) {
            if (rad[k] < 0) continue;
            M += m[k];
            c[0] += m[k] * x[k]; c[1] += m[k] * y[k]; c[2] += m[k] * z[k];
            c[3] += m[k] * vx[k]; c[4] += m[k] * vy[k]; c[5] += m[k] * vz[k];
        }
        if (!(M > 0)) clair_nbody_die("elements(): no mass before this body");
        for (int k = 0; k < 6; k++) c[k] /= M;
    }
    double mu = G * (M + m[i]);
    if (!(mu > 0)) clair_nbody_die("elements(): G and the masses must give more than 0");
    double r[3] = { x[i] - c[0], y[i] - c[1], z[i] - c[2] }, v[3] = { vx[i] - c[3], vy[i] - c[4], vz[i] - c[5] };
    double rn = sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]), v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    double rv = r[0] * v[0] + r[1] * v[1] + r[2] * v[2];
    double hv[3] = { r[1] * v[2] - r[2] * v[1], r[2] * v[0] - r[0] * v[2], r[0] * v[1] - r[1] * v[0] };
    double hn = sqrt(hv[0] * hv[0] + hv[1] * hv[1] + hv[2] * hv[2]);
    if (!(rn > 0) || !(hn > 0)) clair_nbody_die("elements(): the body falls straight (no orbit)");
    double ev[3];
    for (int k = 0; k < 3; k++) ev[k] = ((v2 - mu / rn) * r[k] - rv * v[k]) / mu;
    double e = sqrt(ev[0] * ev[0] + ev[1] * ev[1] + ev[2] * ev[2]);
    double energy = v2 / 2 - mu / rn, a = -mu / (2 * energy);
    double inc = acos(fmax(-1, fmin(1, hv[2] / hn)));
    double nv[3] = { -hv[1], hv[0], 0 }, nn = hypot(nv[0], nv[1]);
    double node = 0;
    if (nn > 1e-15 * hn) node = atan2(hv[0], -hv[1]);
    else { nv[0] = 1; nv[1] = 0; nn = 1; }            /* in the plane z = 0: from the axis x */
    double hu[3] = { hv[0] / hn, hv[1] / hn, hv[2] / hn }, nu[3] = { nv[0] / nn, nv[1] / nn, 0 };
    double w[3] = { hu[1] * nu[2] - hu[2] * nu[1], hu[2] * nu[0] - hu[0] * nu[2], hu[0] * nu[1] - hu[1] * nu[0] };
    double u = atan2(r[0] * w[0] + r[1] * w[1] + r[2] * w[2], r[0] * nu[0] + r[1] * nu[1]);   /* the argument of latitude */
    double f = u, peri = 0;
    if (e > 1e-15) {
        double eu[3] = { ev[0] / e, ev[1] / e, ev[2] / e };
        double q[3] = { hu[1] * eu[2] - hu[2] * eu[1], hu[2] * eu[0] - hu[0] * eu[2], hu[0] * eu[1] - hu[1] * eu[0] };
        f = atan2(r[0] * q[0] + r[1] * q[1] + r[2] * q[2], r[0] * eu[0] + r[1] * eu[1] + r[2] * eu[2]);
        peri = u - f;
    }
    double Mean = 0, period = 0;
    if (e < 1) {
        double E = 2 * atan2(sqrt(1 - e) * sin(f / 2), sqrt(1 + e) * cos(f / 2));
        Mean = E - e * sin(E);
        period = 2 * 3.141592653589793238 * sqrt(a * a * a / mu);
    } else if (e > 1) {
        double F = 2 * atanh(sqrt((e - 1) / (e + 1)) * tan(f / 2));
        Mean = e * sinh(F) - F;
    }
    clair_ias.out[0] = a; clair_ias.out[1] = e; clair_ias.out[2] = inc / CLAIR_NB_DEG;
    clair_ias.out[3] = clair_nbody_angle(node); clair_ias.out[4] = clair_nbody_angle(peri);
    clair_ias.out[5] = clair_nbody_angle(f);
    clair_ias.out[6] = e < 1 ? clair_nbody_angle(Mean) : Mean / CLAIR_NB_DEG;
    clair_ias.out[7] = period; clair_ias.out[8] = rn;
    return 0;
}

/* ---- s.show(): the window ---- */

static void clair_nbody_rgb(size_t i, uint8_t *r, uint8_t *g, uint8_t *b) {
    static const int palette[] = { 0xfdd835, 0x4fc3f7, 0xff7043, 0x81c784, 0xba68c8, 0xf06292, 0x4db6ac, 0xffb74d,
                                   0x9575cd, 0xaed581, 0x64b5f6, 0xe57373 };
    int64_t c = -1;
    if (clair_nb.colors && i < clair_nb.colors->len) c = ((int64_t *)clair_nb.colors->data)[i];
    if (c < 0) c = palette[i % (sizeof(palette) / sizeof(palette[0]))];
    *r = (uint8_t)(c >> 16);
    *g = (uint8_t)(c >> 8);
    *b = (uint8_t)c;
}

static const char *clair_nbody_name(size_t i) {
    if (!clair_nb.names || i >= clair_nb.names->len) return "";
    const char *s = ((const char **)clair_nb.names->data)[i];
    return s ? s : "";
}

/* a number as people write it: 12.5, 0.004, 3.2e+08 */
static void clair_nbody_num(char *out, size_t n, double v) {
    double a = fabs(v);
    if (a != 0 && (a >= 1e6 || a < 1e-3)) snprintf(out, n, "%.2e", v);
    else snprintf(out, n, "%.4g", v);
}

/* a length of 1, 2 or 5 times a power of ten, close to want */
static double clair_nbody_nice(double want) {
    if (!(want > 0)) return 1;
    double p = pow(10, floor(log10(want)));
    double k = want / p;
    return (k < 1.5 ? 1 : k < 3.5 ? 2 : k < 7.5 ? 5 : 10) * p;
}

/* the place of a point of the simulation in the window */
typedef struct { double cx, cy, scale, W, mid; } ClairNbodyView;

static double clair_nbody_sx(const ClairNbodyView *v, double x) { return v->W / 2 + (x - v->cx) * v->scale; }
static double clair_nbody_sy(const ClairNbodyView *v, double y) { return v->mid - (y - v->cy) * v->scale; }

static const double *clair_nbody_masses;           /* for qsort: the lightest first */

static int clair_nbody_lighter(const void *a, const void *b) {
    size_t i = *(const size_t *)a, j = *(const size_t *)b;
    double mi = clair_nbody_masses[i], mj = clair_nbody_masses[j];
    return mi < mj ? -1 : mi > mj ? 1 : (i > j) - (i < j);
}

static double clair_nbody_show(void) {
    size_t n = clair_nbody_n();
    if (!n) clair_nbody_die("s.show(): no body (s.add(...) first)");
    int opened_here = 0;
    if (!clair_gui.open || clair_gui.closing) {
        clair_gui_open("nbody", 0, 0);
        opened_here = 1;
    }
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double G = clair_nb.G, soft = clair_nb.soft, h = clair_nb.dt > 0 ? clair_nb.dt : 1e-3;
    double tau = clair_nbody_tau(G, soft);
    double speed = clair_nb.speed > 0 ? clair_nb.speed : (tau > 0 ? 2 * 3.141592653589793 * tau / 4 : 1);
    double t = clair_nb.t;
    /* the trails: the last places of each body, a point every 2 pixels (at most 40000 points in all) */
    size_t T = clair_nb.trail > 0 ? (size_t)clair_nb.trail : 0;
    if (T > 0 && T * n > 40000) T = 40000 / n;
    if (T == 1) T = 0;
    double *tx = T ? calloc(T * n, sizeof(double)) : NULL, *ty = T ? calloc(T * n, sizeof(double)) : NULL;
    size_t *head = T ? calloc(n, sizeof(size_t)) : NULL, *filled = T ? calloc(n, sizeof(size_t)) : NULL;
    float *px = malloc((T + 1) * sizeof(float)), *py = malloc((T + 1) * sizeof(float));
    float *labels = malloc(4 * 41 * sizeof(float));
    if ((T && (!tx || !ty || !head || !filled)) || !px || !py || !labels) clair_nbody_die("out of memory");
    ClairNbodyView v = { 0, 0, 1, 0, 0 };
    size_t *order = NULL, order_cap = 0;
    int follow = -2;                                       /* -2: the center of mass, -1: nothing, i: the body i */
    int paused = 0, trails = T > 0, slow = 0, fit = 1, quit = 0;
    double acc = 0, e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 1);
    float press_x = 0, press_y = 0, last_x = 0, last_y = 0, moved = 0;
    int pressing = 0, press_on_bar = 0;
    while (!quit && clair_gui_frame()) {
        double W = (double)clair_gui_size(0), H = (double)clair_gui_size(1);
        double ui = (W < H ? W : H) / 420.0;
        if (ui < 1) ui = 1;
        int per_row = W < 0.9 * H ? 4 : 8;                 /* upright: the buttons on two rows */
        double bar_h = 44 * ui, bar_y = H - (8 / per_row) * (bar_h + 4 * ui) - 2 * ui;
        v.W = W;
        v.mid = bar_y / 2;
        n = clair_nbody_n();
        x = CLAIR_NB(x); y = CLAIR_NB(y); m = CLAIR_NB(m); r = CLAIR_NB(r);
        if (fit) {                                         /* everything in the window, around the center of mass */
            double c0 = clair_nbody_sum(6), c1 = clair_nbody_sum(7), far = 0;
            for (size_t i = 0; i < n; i++) {
                if (r[i] < 0) continue;
                double d = hypot(x[i] - c0, y[i] - c1);
                if (d > far) far = d;
            }
            v.scale = far > 0 ? 0.42 * (W < bar_y ? W : bar_y) / far : 1;
            v.cx = c0;
            v.cy = c1;
            follow = -2;
            fit = 0;
        }
        /* the keys */
        bool *kn = clair_gui.keys_now, *kb = clair_gui.keys_before;
        #define CLAIR_NB_KEY(s) (kn[s] && !kb[s])
        if (CLAIR_NB_KEY(44)) paused = !paused;                              /* space */
        if (CLAIR_NB_KEY(41) || CLAIR_NB_KEY(20)) quit = 1;                  /* escape, q */
        if (CLAIR_NB_KEY(46) || CLAIR_NB_KEY(87)) v.scale *= 1.5;            /* = + */
        if (CLAIR_NB_KEY(45) || CLAIR_NB_KEY(86)) v.scale /= 1.5;            /* - */
        if (CLAIR_NB_KEY(79)) speed *= 2;                                    /* right */
        if (CLAIR_NB_KEY(80)) speed /= 2;                                    /* left */
        if (CLAIR_NB_KEY(6)) fit = 1;                                        /* c */
        if (CLAIR_NB_KEY(23)) trails = !trails && T > 0;                     /* t */
        #undef CLAIR_NB_KEY
        /* the wheel: zoom around the mouse */
        if (clair_gui.wheel != 0) {
            double wx = v.cx + (clair_gui.mx - W / 2) / v.scale, wy = v.cy - (clair_gui.my - v.mid) / v.scale;
            v.scale *= pow(1.2, clair_gui.wheel);
            v.cx = wx - (clair_gui.mx - W / 2) / v.scale;
            v.cy = wy + (clair_gui.my - v.mid) / v.scale;
            follow = -1;
        }
        /* a finger (or the mouse): drag to move, touch a body to follow it (touch it again: no more) */
        bool down = (clair_gui.mouse_now & 1) != 0, was = (clair_gui.mouse_before & 1) != 0;
        if (down && !was) {
            pressing = 1;
            press_on_bar = clair_gui.my >= bar_y;
            press_x = last_x = clair_gui.mx;
            press_y = last_y = clair_gui.my;
            moved = 0;
        }
        if (down && pressing && !press_on_bar) {
            float dx = clair_gui.mx - last_x, dy = clair_gui.my - last_y;
            moved += fabsf(dx) + fabsf(dy);
            if (moved > 6 * ui) {
                v.cx -= dx / v.scale;
                v.cy += dy / v.scale;
                follow = -1;
            }
            last_x = clair_gui.mx;
            last_y = clair_gui.my;
        }
        if (!down && was && pressing) {
            if (!press_on_bar && moved <= 6 * ui) {
                int best = -1;
                double bd = 26 * ui;
                for (size_t i = 0; i < n; i++) {
                    if (r[i] < 0) continue;
                    double d = hypot(clair_nbody_sx(&v, x[i]) - press_x, clair_nbody_sy(&v, y[i]) - press_y);
                    if (d < bd) { bd = d; best = (int)i; }
                }
                if (best >= 0) follow = follow == best ? -1 : best;
            }
            pressing = 0;
        }
        if (!(v.scale > 0) || !isfinite(v.scale)) v.scale = 1;
        /* the physics: the time of the simulation for this frame, within 14 ms */
        if (!paused && (clair_nb.flags & 4)) {           /* V0.68: IAS15 chooses its steps; the frame asks for a time */
            double frame = clair_gui.dt > 0.1 ? 0.1 : clair_gui.dt;
            slow = 0;
            if (clair_ias_run(speed * frame, G, soft, clair_nb.flags, clair_nb.epsilon, h, 14000000ull) > 0)
                e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 1);
            h = clair_ias.out[0];
            t += clair_ias.out[1];
            slow = clair_ias.out[1] < speed * frame * 0.999;
        } else if (!paused) {
            double frame = clair_gui.dt > 0.1 ? 0.1 : clair_gui.dt;
            acc += speed * frame;
            int64_t want = (int64_t)(acc / h);
            acc -= (double)want * h;
            uint64_t start = clair_sdl.GetTicksNS();
            int64_t done = 0, chunk = 1;
            slow = 0;
            while (done < want) {
                int64_t k = want - done < chunk ? want - done : chunk;
                if (clair_nbody_step(h, k, G, soft, clair_nb.flags) > 0)
                    e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 1);   /* a collision changes the energy */
                done += k;
                t += (double)k * h;
                uint64_t spent = clair_sdl.GetTicksNS() - start;
                if (spent > 14000000ull && done < want) { slow = 1; acc = 0; break; }
                if (spent < 2000000ull) chunk *= 2;
            }
        }
        if (follow >= 0 && ((size_t)follow >= n || r[follow] < 0)) follow = -1;
        if (follow >= 0) { v.cx = x[follow]; v.cy = y[follow]; }
        else if (follow == -2) { v.cx = clair_nbody_sum(6); v.cy = clair_nbody_sum(7); }
        for (size_t i = 0; T && i < n; i++) {             /* a new point of the trail every 2 pixels */
            if (r[i] < 0) continue;
            size_t last = (head[i] + T - 1) % T;
            if (filled[i] && hypot(x[i] - tx[i * T + last], y[i] - ty[i * T + last]) * v.scale < 2) continue;
            tx[i * T + head[i]] = x[i];
            ty[i * T + head[i]] = y[i];
            head[i] = (head[i] + 1) % T;
            if (filled[i] < T) filled[i]++;
        }
        /* the drawing */
        clair_sdl.SetRenderDrawColor(clair_gui.renderer, 9, 11, 18, 255);
        clair_sdl.RenderClear(clair_gui.renderer);
        double mmax = 0;
        for (size_t i = 0; i < n; i++) if (r[i] >= 0 && m[i] > mmax) mmax = m[i];
        for (size_t i = 0; trails && i < n; i++) {
            if (r[i] < 0 || filled[i] < 2) continue;
            uint8_t cr, cg, cb;
            clair_nbody_rgb(i, &cr, &cg, &cb);
            size_t k = filled[i];
            for (size_t q = 0; q < k; q++) {
                size_t at = (head[i] + T - k + q) % T;
                px[q] = (float)clair_nbody_sx(&v, tx[i * T + at]);
                py[q] = (float)clair_nbody_sy(&v, ty[i * T + at]);
            }
            px[k] = (float)clair_nbody_sx(&v, x[i]);       /* up to the body itself */
            py[k] = (float)clair_nbody_sy(&v, y[i]);
            for (int part = 0; part < 4; part++) {         /* older: fainter */
                size_t a = (k + 1) * (size_t)part / 4, b = (k + 1) * (size_t)(part + 1) / 4 + 1;
                if (b > k + 1) b = k + 1;
                if (b <= a + 1) continue;
                clair_sdl.SetRenderDrawColor(clair_gui.renderer, cr, cg, cb, (uint8_t)(55 + 50 * part));
                clair_gui_path(px + a, py + a, (int)(b - a), false);
            }
        }
        if (n > order_cap) {
            size_t *o = realloc(order, n * sizeof(size_t));
            if (!o) clair_nbody_die("out of memory");
            order = o;
            order_cap = n;
        }
        for (size_t i = 0; i < n; i++) order[i] = i;
        clair_nbody_masses = m;
        qsort(order, n, sizeof(size_t), clair_nbody_lighter);
        size_t nlabels = 0;
        for (size_t q = n; q-- > 0;) {                     /* the names: the heaviest bodies first */
            size_t i = order[q];
            if (r[i] < 0 || n > 40 || nlabels >= 40) continue;
            const char *name = clair_nbody_name(i);
            if (!name[0]) continue;
            double sx = clair_nbody_sx(&v, x[i]), sy = clair_nbody_sy(&v, y[i]);
            double pr = mmax > 0 && m[i] > 0 ? ui * (2.5 + 4 * cbrt(m[i] / mmax)) : 2 * ui;
            if (r[i] * v.scale > pr) pr = r[i] * v.scale;
            double fs = 9 * ui, lx = sx + pr + 4 * ui, ly = sy - fs / 2, lw = clair_gui_text_width(name, fs);
            if (lx > W || ly > H || lx + lw < 0 || ly + fs < 0) continue;
            int hidden = 0;                                /* not on top of another name */
            for (size_t k = 0; k < nlabels && !hidden; k++) {
                const float *b = labels + 4 * k;
                hidden = lx < b[2] && lx + lw > b[0] && ly < b[3] && ly + fs > b[1];
            }
            if (hidden) continue;
            float *b = labels + 4 * nlabels++;
            b[0] = (float)lx; b[1] = (float)ly; b[2] = (float)(lx + lw); b[3] = (float)(ly + fs);
            clair_gui_set_color(200, 205, 220, 255);
            clair_gui_text(name, lx, ly, fs);
        }
        for (size_t q = 0; q < n; q++) {                    /* the bodies: the heaviest on top */
            size_t i = order[q];
            if (r[i] < 0) continue;
            double sx = clair_nbody_sx(&v, x[i]), sy = clair_nbody_sy(&v, y[i]);
            double pr = mmax > 0 && m[i] > 0 ? ui * (2.5 + 4 * cbrt(m[i] / mmax)) : 2 * ui;
            if (r[i] * v.scale > pr) pr = r[i] * v.scale;
            if (sx < -pr || sx > W + pr || sy < -pr || sy > H + pr) continue;
            uint8_t cr, cg, cb;
            clair_nbody_rgb(i, &cr, &cg, &cb);
            if (follow == (int)i) {
                clair_gui_set_color(255, 255, 255, 200);
                clair_gui_circle(sx, sy, pr + 4 * ui, false);
            }
            clair_gui_set_color(cr, cg, cb, 255);
            if (pr < 2) clair_gui_rect(sx - 1, sy - 1, 2, 2, true);
            else clair_gui_circle(sx, sy, pr, true);
        }
        /* what is going on */
        char line[160], a[40], b[40];
        double fs = 10 * ui, lx = 8 * ui, ly = 8 * ui;
        clair_gui_set_color(230, 232, 240, 255);
        clair_nbody_num(a, sizeof(a), t / clair_nb.tscale);
        snprintf(line, sizeof(line), "t = %s%s%s", a, clair_nb.tunit[0] ? " " : "", clair_nb.tunit);
        clair_gui_text(line, lx, ly, fs);
        clair_nbody_num(b, sizeof(b), speed / clair_nb.tscale);
        snprintf(line, sizeof(line), "1 s = %s%s%s%s", b, clair_nb.tunit[0] ? " " : "", clair_nb.tunit, slow ? "  (ralenti)" : "");
        clair_gui_text(line, lx, ly + 1.5 * fs, fs);
        double e = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 1);
        size_t alive = 0;
        for (size_t i = 0; i < n; i++) alive += r[i] >= 0;
        clair_nbody_num(a, sizeof(a), e0 != 0 ? fabs((e - e0) / e0) : fabs(e - e0));
        snprintf(line, sizeof(line), "%zu corps   énergie : écart %s", alive, a);
        clair_gui_text(line, lx, ly + 3 * fs, fs);
        if (follow >= 0) {
            const char *name = clair_nbody_name((size_t)follow);
            snprintf(line, sizeof(line), "suit : %s", name[0] ? name : "un corps");
            clair_gui_text(line, lx, ly + 4.5 * fs, fs);
        }
        if (paused) {
            clair_gui_set_color(253, 216, 53, 255);
            clair_gui_text("PAUSE", W - lx - clair_gui_text_width("PAUSE", fs * 1.4), ly, fs * 1.4);
        }
        /* the scale: a length of 1, 2 or 5 times a power of ten */
        double len = clair_nbody_nice(W / 5 / v.scale / clair_nb.lscale);
        double pix = len * clair_nb.lscale * v.scale;
        double sy0 = bar_y - 10 * ui;
        clair_gui_set_color(200, 205, 220, 255);
        clair_gui_rect(lx, sy0, pix, 2 * ui, true);
        clair_nbody_num(a, sizeof(a), len);
        snprintf(line, sizeof(line), "%s%s%s", a, clair_nb.lunit[0] ? " " : "", clair_nb.lunit);
        clair_gui_text(line, lx, sy0 - 1.4 * fs, fs);
        /* the buttons (what they do counts from the next frame) */
        const char *names[8] = { paused ? "Lecture" : "Pause", "-", "+", "<<", ">>", "Centrer", trails ? "Traces" : "Sans trace", "Quitter" };
        double bw = (W - (per_row + 1) * 4 * ui) / per_row;
        for (int k = 0; k < 8; k++) {
            double bx = 4 * ui + (k % per_row) * (bw + 4 * ui), by = bar_y + (k / per_row) * (bar_h + 4 * ui);
            if (!clair_gui_button(names[k], bx, by, bw, bar_h)) continue;
            switch (k) {
                case 0: paused = !paused; break;
                case 1: v.scale /= 1.5; break;
                case 2: v.scale *= 1.5; break;
                case 3: speed /= 2; break;
                case 4: speed *= 2; break;
                case 5: fit = 1; break;
                case 6: trails = !trails && T > 0; break;
                default: quit = 1; break;
            }
        }
        clair_gui_present();
    }
    free(tx); free(ty); free(head); free(filled); free(px); free(py); free(labels); free(order);
    if (opened_here) clair_gui_close();
    clair_ias.out[0] = h;                                   /* V0.68: the step IAS15 got to */
    return t;
}
