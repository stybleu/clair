/*
 * V0.66: the module nbody (import nbody): N bodies pulled by Newton's gravity.
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
    double dt, speed, trail, tscale, lscale, G, soft, t;
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

/* 0 dt, 1 speed, 2 trail, 3 time scale, 4 length scale, 5 G, 6 softening, 7 flags, 8 t */
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
        if (!paused) {
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
    return t;
}
