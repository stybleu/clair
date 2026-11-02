/*
 * V0.66: the module nbody (import nbody): N bodies pulled by Newton's gravity.
 * V0.68: IAS15 (s.integrator = "ias15"), the orbital elements, the solar system.
 * V0.69: WHFast (s.integrator = "whfast"), s.save() and nbody.load().
 * V0.70: the tree of Barnes and Hut (s.gravity = "tree") on every core, Plummer spheres.
 * V0.71: MERCURIUS (s.integrator = "mercurius"), the hybrid of WHFast and IAS15.
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
    ClairList crit;                                  /* V0.71: s.crit, the critical distances of MERCURIUS */
    double *ax, *ay, *az;
    size_t cap;
    double dt, speed, trail, tscale, lscale, G, soft, t, epsilon, theta_saved, hill;
    double c;                                        /* V0.71.1: the speed of light (0: Newton only) */
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

/* V0.71: s.crit (MERCURIUS keeps there the critical distance of each body, then the dt, G and s.hill they
   were computed with) */
static int64_t clair_nbody_crit(ClairList c) {
    clair_nb.crit = c;
    return 0;
}

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

/* ---- V0.70: the tree of Barnes and Hut (s.gravity = "tree"), on every core ----
 *
 * After Barnes & Hut (1986), "A hierarchical O(N log N) force-calculation algorithm", Nature 324, 446.
 * The bodies go into an octree (a cube cut into 8, again and again, until one body per cube).  Each
 * cube keeps its mass, its center of mass and its quadrupole.  For the force on a body, a cube seen
 * under a small angle (side / distance < theta) counts as one mass at its center of mass, corrected by
 * its quadrupole; otherwise its 8 smaller cubes are looked at (the distance also counts how far its
 * center of mass is from its middle: Barnes 1994, fewer large errors).  N log N instead of N^2: the forces of
 * 100 000 bodies in a fraction of a second.  The error of a force is about theta^4 with the quadrupole
 * (theta = 0.5: about 1e-4 to 1e-3 of the force).  Each body is computed alone, in a fixed order: the
 * result does not depend on the number of cores.
 */

#include <pthread.h>

typedef struct {
    double c[3], half;                               /* the cube: its center, half its side */
    double m, com[3], q[6];                          /* mass, center of mass, quadrupole xx xy xz yy yz zz */
    int32_t child[8];                                /* -1: none */
    int32_t first, count, leaf;                      /* a leaf: its bodies (first, then next[]) */
} ClairBhNode;

static struct {
    ClairBhNode *node;
    size_t nnode, capnode, capnext;
    int32_t *next, root;
    double theta;                                    /* 0: no tree, the direct sums */
    int threads;                                     /* 0: all the cores */
} clair_bh = { .root = -1 };

static int32_t clair_bh_node(const double c[3], double half) {
    if (clair_bh.nnode == clair_bh.capnode) {
        size_t cap = clair_bh.capnode ? clair_bh.capnode * 2 : 1024;
        ClairBhNode *n = realloc(clair_bh.node, cap * sizeof(ClairBhNode));
        if (!n) clair_nbody_die("out of memory");
        clair_bh.node = n;
        clair_bh.capnode = cap;
    }
    ClairBhNode *n = &clair_bh.node[clair_bh.nnode];
    memset(n, 0, sizeof(*n));
    for (int k = 0; k < 3; k++) n->c[k] = c[k];
    n->half = half;
    for (int k = 0; k < 8; k++) n->child[k] = -1;
    n->first = -1;
    n->leaf = 1;
    return (int32_t)clair_bh.nnode++;
}

static int clair_bh_octant(const ClairBhNode *n, const double p[3]) {
    return (p[0] > n->c[0]) | ((p[1] > n->c[1]) << 1) | ((p[2] > n->c[2]) << 2);
}

static void clair_bh_place(int32_t k, size_t b, const double *x, const double *y, const double *z) {
    double p[3] = { x[b], y[b], z[b] };
    for (int depth = 0;; depth++) {
        ClairBhNode *n = &clair_bh.node[k];
        if (!n->leaf) {
            int o = clair_bh_octant(n, p);
            if (n->child[o] < 0) {
                double c[3], h = n->half / 2;
                for (int d = 0; d < 3; d++) c[d] = n->c[d] + ((o >> d) & 1 ? h : -h);
                int32_t nk = clair_bh_node(c, h);
                n = &clair_bh.node[k];
                n->child[o] = nk;
            }
            k = n->child[o];
            continue;
        }
        if (n->count == 0 || depth >= 48) {           /* empty, or bodies on top of one another: they share it */
            clair_bh.next[b] = n->first;
            n->first = (int32_t)b;
            n->count++;
            return;
        }
        int32_t list = n->first;                      /* one body already: the cube is cut in 8 */
        n->first = -1;
        n->count = 0;
        n->leaf = 0;
        while (list >= 0) {
            int32_t nx = clair_bh.next[list];
            clair_bh_place(k, (size_t)list, x, y, z);
            list = nx;
        }
    }
}

/* the tree of the bodies still there (massive = 1: only those with a mass; 2: only those with a size) */
static void clair_bh_build(int which) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    if (n > clair_bh.capnext) {
        int32_t *nx = realloc(clair_bh.next, n * sizeof(int32_t));
        if (!nx) clair_nbody_die("out of memory");
        clair_bh.next = nx;
        clair_bh.capnext = n;
    }
    clair_bh.nnode = 0;
    clair_bh.root = -1;
    double lo[3] = { INFINITY, INFINITY, INFINITY }, hi[3] = { -INFINITY, -INFINITY, -INFINITY };
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0 || (which == 1 && !(m[i] > 0)) || (which == 2 && !(r[i] > 0))) continue;
        double p[3] = { x[i], y[i], z[i] };
        for (int d = 0; d < 3; d++) { if (p[d] < lo[d]) lo[d] = p[d]; if (p[d] > hi[d]) hi[d] = p[d]; }
        used++;
    }
    if (!used) return;
    double c[3], half = 0;
    for (int d = 0; d < 3; d++) {
        c[d] = (lo[d] + hi[d]) / 2;
        if ((hi[d] - lo[d]) / 2 > half) half = (hi[d] - lo[d]) / 2;
    }
    if (!isfinite(half)) clair_nbody_die("a body is at an infinite place (or not a number)");
    half = half * 1.0001 + 1e-300;
    clair_bh.root = clair_bh_node(c, half);
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0 || (which == 1 && !(m[i] > 0)) || (which == 2 && !(r[i] > 0))) continue;
        clair_bh_place(clair_bh.root, i, x, y, z);
    }
}

/* mass, center of mass and quadrupole of each cube, from the small ones up */
static void clair_bh_moments(int32_t k) {
    ClairBhNode *n = &clair_bh.node[k];
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m);
    double M = 0, s[3] = { 0, 0, 0 };
    if (n->leaf) {
        for (int32_t b = n->first; b >= 0; b = clair_bh.next[b]) { M += m[b]; s[0] += m[b] * x[b]; s[1] += m[b] * y[b]; s[2] += m[b] * z[b]; }
    } else {
        for (int o = 0; o < 8; o++) {
            int32_t ck = n->child[o];
            if (ck < 0) continue;
            clair_bh_moments(ck);
            n = &clair_bh.node[k];
            const ClairBhNode *c = &clair_bh.node[ck];
            M += c->m;
            for (int d = 0; d < 3; d++) s[d] += c->m * c->com[d];
        }
    }
    n->m = M;
    for (int d = 0; d < 3; d++) n->com[d] = M > 0 ? s[d] / M : n->c[d];
    double q[6] = { 0, 0, 0, 0, 0, 0 };
    if (n->leaf) {
        for (int32_t b = n->first; b >= 0; b = clair_bh.next[b]) {
            double d[3] = { x[b] - n->com[0], y[b] - n->com[1], z[b] - n->com[2] }, d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            q[0] += m[b] * (3 * d[0] * d[0] - d2); q[1] += m[b] * 3 * d[0] * d[1]; q[2] += m[b] * 3 * d[0] * d[2];
            q[3] += m[b] * (3 * d[1] * d[1] - d2); q[4] += m[b] * 3 * d[1] * d[2]; q[5] += m[b] * (3 * d[2] * d[2] - d2);
        }
    } else {
        for (int o = 0; o < 8; o++) {
            int32_t ck = n->child[o];
            if (ck < 0) continue;
            const ClairBhNode *c = &clair_bh.node[ck];
            double d[3] = { c->com[0] - n->com[0], c->com[1] - n->com[1], c->com[2] - n->com[2] }, d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            q[0] += c->q[0] + c->m * (3 * d[0] * d[0] - d2); q[1] += c->q[1] + c->m * 3 * d[0] * d[1]; q[2] += c->q[2] + c->m * 3 * d[0] * d[2];
            q[3] += c->q[3] + c->m * (3 * d[1] * d[1] - d2); q[4] += c->q[4] + c->m * 3 * d[1] * d[2]; q[5] += c->q[5] + c->m * (3 * d[2] * d[2] - d2);
        }
    }
    for (int k6 = 0; k6 < 6; k6++) n->q[k6] = q[k6];
}

/* the tree laid flat in the order of the walk (depth first): after a cube come its small cubes, and
   skip[k] is the first cube after all of them; the bodies of the leaves follow one another in memory */
static struct {
    size_t n, cap, nb, capb;
    double *m, *com, *q, *w2, *box;                 /* com: 3, q: 6, w2: the distance to open it, squared; box: center 3 + half */
    int32_t *start, *count, *skip;                  /* start: first body of a leaf (-1: not a leaf) */
    size_t *bidx;                                   /* the bodies in the order of the leaves */
    double *bp;                                     /* x y z m of those bodies */
} clair_bf;

static void clair_bf_grow(void) {
    size_t cap = clair_bf.cap ? clair_bf.cap * 2 : 1024;
    double *m = realloc(clair_bf.m, cap * sizeof(double));
    if (m) clair_bf.m = m;
    double *com = realloc(clair_bf.com, 3 * cap * sizeof(double));
    if (com) clair_bf.com = com;
    double *q = realloc(clair_bf.q, 6 * cap * sizeof(double));
    if (q) clair_bf.q = q;
    double *w2 = realloc(clair_bf.w2, cap * sizeof(double));
    if (w2) clair_bf.w2 = w2;
    double *box = realloc(clair_bf.box, 4 * cap * sizeof(double));
    if (box) clair_bf.box = box;
    int32_t *st = realloc(clair_bf.start, cap * sizeof(int32_t));
    if (st) clair_bf.start = st;
    int32_t *ct = realloc(clair_bf.count, cap * sizeof(int32_t));
    if (ct) clair_bf.count = ct;
    int32_t *sk = realloc(clair_bf.skip, cap * sizeof(int32_t));
    if (sk) clair_bf.skip = sk;
    if (!m || !com || !q || !w2 || !box || !st || !ct || !sk) clair_nbody_die("out of memory");
    clair_bf.cap = cap;
}

static void clair_bf_lay(int32_t k) {
    if (clair_bf.n == clair_bf.cap) clair_bf_grow();
    size_t f = clair_bf.n++;
    const ClairBhNode *n = &clair_bh.node[k];
    clair_bf.m[f] = n->m;
    for (int d = 0; d < 3; d++) { clair_bf.com[3 * f + d] = n->com[d]; clair_bf.box[4 * f + d] = n->c[d]; }
    for (int d = 0; d < 6; d++) clair_bf.q[6 * f + d] = n->q[d];
    clair_bf.box[4 * f + 3] = n->half;
    {   /* opened when nearer than side / theta + the distance from its center to its center of mass (Barnes 1994) */
        double dc = sqrt((n->com[0] - n->c[0]) * (n->com[0] - n->c[0]) + (n->com[1] - n->c[1]) * (n->com[1] - n->c[1]) + (n->com[2] - n->c[2]) * (n->com[2] - n->c[2]));
        double lim = 2 * n->half / clair_bh.theta + dc;
        clair_bf.w2[f] = lim * lim;
    }
    clair_bf.start[f] = -1;
    clair_bf.count[f] = 0;
    if (n->leaf) {
        const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m);
        clair_bf.start[f] = (int32_t)clair_bf.nb;
        for (int32_t b = n->first; b >= 0; b = clair_bh.next[b]) {
            size_t q = clair_bf.nb++;
            clair_bf.bidx[q] = (size_t)b;
            clair_bf.bp[4 * q] = x[b]; clair_bf.bp[4 * q + 1] = y[b]; clair_bf.bp[4 * q + 2] = z[b]; clair_bf.bp[4 * q + 3] = m[b];
            clair_bf.count[f]++;
        }
    } else {
        for (int o = 0; o < 8; o++) if (clair_bh.node[k].child[o] >= 0) clair_bf_lay(clair_bh.node[k].child[o]);
    }
    clair_bf.skip[f] = (int32_t)clair_bf.n;
}

static void clair_bf_build(void) {
    size_t n = clair_nbody_n();
    if (n > clair_bf.capb) {
        size_t *bi = realloc(clair_bf.bidx, n * sizeof(size_t));
        if (bi) clair_bf.bidx = bi;
        double *bp = realloc(clair_bf.bp, 4 * n * sizeof(double));
        if (bp) clair_bf.bp = bp;
        if (!bi || !bp) clair_nbody_die("out of memory");
        clair_bf.capb = n;
    }
    clair_bf.n = clair_bf.nb = 0;
    if (clair_bh.root >= 0) clair_bf_lay(clair_bh.root);
}

/* the pull of the tree on the body i at p (and its potential, per unit of mass) */
static void clair_bh_walk(size_t i, const double p[3], double G, double eps2, double a[3], double *phi) {
    size_t k = 0, nn = clair_bf.n;
    double ax = 0, ay = 0, az = 0, ph = 0;
    while (k < nn) {
        if (!(clair_bf.m[k] > 0)) { k = (size_t)clair_bf.skip[k]; continue; }
        int32_t st = clair_bf.start[k];
        if (st >= 0) {                                /* a leaf: its bodies one by one */
            for (int32_t q = st; q < st + clair_bf.count[k]; q++) {
                if (clair_bf.bidx[q] == i) continue;
                const double *b = clair_bf.bp + 4 * q;
                double dx = b[0] - p[0], dy = b[1] - p[1], dz = b[2] - p[2], d2 = dx * dx + dy * dy + dz * dz + eps2;
                if (d2 == 0) continue;
                double rinv = 1 / sqrt(d2), gm = G * b[3] * rinv, f = gm * rinv * rinv;
                ax += f * dx; ay += f * dy; az += f * dz;
                ph -= gm;
            }
            k = (size_t)clair_bf.skip[k];
            continue;
        }
        const double *c = clair_bf.com + 3 * k, *bx = clair_bf.box + 4 * k;
        double d0 = p[0] - c[0], d1 = p[1] - c[1], d2 = p[2] - c[2];
        double r2 = d0 * d0 + d1 * d1 + d2 * d2;
        if (r2 > clair_bf.w2[k] && !(fabs(p[0] - bx[0]) <= bx[3] && fabs(p[1] - bx[1]) <= bx[3] && fabs(p[2] - bx[2]) <= bx[3])) {
            const double *Q = clair_bf.q + 6 * k;     /* far enough: one mass and its quadrupole */
            double s2 = r2 + eps2, rinv = 1 / sqrt(s2), ri2 = rinv * rinv, r3 = rinv * ri2, r5 = r3 * ri2, r7 = r5 * ri2;
            double q0 = Q[0] * d0 + Q[1] * d1 + Q[2] * d2, q1 = Q[1] * d0 + Q[3] * d1 + Q[4] * d2, q2 = Q[2] * d0 + Q[4] * d1 + Q[5] * d2;
            double dQd = d0 * q0 + d1 * q1 + d2 * q2, m = clair_bf.m[k];
            double radial = -m * r3 - 2.5 * dQd * r7;
            ax += G * (radial * d0 + q0 * r5); ay += G * (radial * d1 + q1 * r5); az += G * (radial * d2 + q2 * r5);
            ph -= G * (m * rinv + 0.5 * dQd * r5);
            k = (size_t)clair_bf.skip[k];
        } else {
            k++;                                      /* its small cubes come next */
        }
    }
    a[0] += ax; a[1] += ay; a[2] += az;
    *phi += ph;
}

/* V0.70.1: the work is cut in pieces of 64 bodies; each core takes the next piece as soon as it is done
   with one (the cores of a phone are not all as fast: equal shares would wait for the slowest) */
#define CLAIR_BH_PIECE 64

typedef struct { size_t n; double G, eps2; int want_phi; size_t next; double *phi; } ClairBhWork;

/* the bodies in the order of the leaves (neighbours together: the same cubes stay in the cache) */
static void *clair_bh_worker(void *arg) {
    ClairBhWork *w = arg;
    const double *m = CLAIR_NB(m);
    for (;;) {
        size_t lo = __atomic_fetch_add(&w->next, CLAIR_BH_PIECE, __ATOMIC_RELAXED);
        if (lo >= w->n) break;
        size_t hi = lo + CLAIR_BH_PIECE < w->n ? lo + CLAIR_BH_PIECE : w->n;
        for (size_t q = lo; q < hi; q++) {
            size_t i = clair_bf.bidx[q];
            const double *p = clair_bf.bp + 4 * q;
            double a[3] = { 0, 0, 0 }, phi = 0;
            clair_bh_walk(i, p, w->G, w->eps2, a, &phi);
            if (w->want_phi) w->phi[q] = 0.5 * m[i] * phi;
            else { clair_nb.ax[i] = a[0]; clair_nb.ay[i] = a[1]; clair_nb.az[i] = a[2]; }
        }
    }
    return NULL;
}

static int clair_bh_cores(void) {
    int t = clair_bh.threads;
    if (t <= 0) {
        long c = sysconf(_SC_NPROCESSORS_ONLN);
        t = c > 0 ? (int)c : 1;
    }
    return t < 1 ? 1 : t > 64 ? 64 : t;
}

/* the forces with the tree (want_phi: the potential energy instead), on every core */
static double clair_bh_run(double G, double eps2, int want_phi) {
    size_t n = clair_nbody_n();
    if (!want_phi) for (size_t i = 0; i < n; i++) clair_nb.ax[i] = clair_nb.ay[i] = clair_nb.az[i] = 0;
    clair_bh_build(0);                                /* every body still there: those without mass are pulled too */
    if (clair_bh.root >= 0) clair_bh_moments(clair_bh.root);
    clair_bf_build();
    n = clair_bf.nb;
    static double *phis;
    static size_t capphi;
    if (want_phi && n > capphi) {
        double *p = realloc(phis, n * sizeof(double));
        if (!p) clair_nbody_die("out of memory");
        phis = p;
        capphi = n;
    }
    int t = n >= 256 ? clair_bh_cores() : 1;
    if ((size_t)t > (n + CLAIR_BH_PIECE - 1) / CLAIR_BH_PIECE) t = (int)((n + CLAIR_BH_PIECE - 1) / CLAIR_BH_PIECE);
    if (t < 1) t = 1;
    ClairBhWork w = { n, G, eps2, want_phi, 0, phis };
    pthread_t th[64];
    int started[64] = { 0 };
    for (int k = 1; k < t; k++) started[k] = pthread_create(&th[k], NULL, clair_bh_worker, &w) == 0;
    clair_bh_worker(&w);
    for (int k = 1; k < t; k++) if (started[k]) pthread_join(th[k], NULL);
    double phi = 0;
    if (want_phi) for (size_t q = 0; q < n; q++) phi += phis[q];   /* in a fixed order: the same whatever the cores */
    return phi;
}

static void clair_bh_search(int32_t k, size_t i, const double p[3], double reach, size_t *found, size_t *nfound, size_t cap) {
    const ClairBhNode *n = &clair_bh.node[k];
    for (int d = 0; d < 3; d++) if (fabs(p[d] - n->c[d]) > n->half + reach) return;
    if (n->leaf) {
        for (int32_t b = n->first; b >= 0; b = clair_bh.next[b]) if ((size_t)b > i && *nfound < cap) found[(*nfound)++] = (size_t)b;
        return;
    }
    for (int o = 0; o < 8; o++) if (n->child[o] >= 0) clair_bh_search(n->child[o], i, p, reach, found, nfound, cap);
}

static int clair_bh_less(const void *a, const void *b) {
    size_t x = *(const size_t *)a, y = *(const size_t *)b;
    return (x > y) - (x < y);
}

/* the accelerations of every body (the absorbed ones: none) */
static void clair_nbody_forces(double G, double eps2) {
    size_t n = clair_nbody_n();
    if (clair_bh.theta > 0) { clair_bh_run(G, eps2, 0); return; }     /* V0.70: with the tree */
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

/* the bodies i and j touch? then they become one (the lighter one is absorbed): gives 1 */
static int clair_nbody_merge(size_t i, size_t j, int *i_gone) {
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    if (r[j] <= 0) return 0;
    double dx = x[j] - x[i], dy = y[j] - y[i], dz = z[j] - z[i], s = r[i] + r[j];
    if (dx * dx + dy * dy + dz * dz >= s * s) return 0;
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
    *i_gone = gone == i;
    return 1;
}

/* the bodies that touch become one; gives how many collisions */
static int64_t clair_nbody_collide(void) {
    size_t n = clair_nbody_n();
    double *r = CLAIR_NB(r);
    int64_t hits = 0;
    if (clair_bh.theta > 0 && n > 64) {               /* V0.70: the tree finds the neighbours */
        clair_bh_build(2);
        if (clair_bh.root < 0) return 0;
        double rmax = 0;
        for (size_t i = 0; i < n; i++) if (r[i] > rmax) rmax = r[i];
        size_t cap = 256, *found = malloc(cap * sizeof(size_t));
        if (!found) clair_nbody_die("out of memory");
        for (size_t i = 0; i < n; i++) {
            if (r[i] <= 0) continue;
            double p[3] = { CLAIR_NB(x)[i], CLAIR_NB(y)[i], CLAIR_NB(z)[i] };
            size_t nf = 0;
            clair_bh_search(clair_bh.root, i, p, r[i] + rmax, found, &nf, cap);
            qsort(found, nf, sizeof(size_t), clair_bh_less);
            for (size_t q = 0; q < nf; q++) {
                int i_gone = 0;
                if (r[i] <= 0) break;
                if (clair_nbody_merge(i, found[q], &i_gone)) { hits++; if (i_gone) break; }
            }
        }
        free(found);
        return hits;
    }
    for (size_t i = 0; i < n; i++) {
        if (r[i] <= 0) continue;                           /* a body without a size never touches */
        for (size_t j = i + 1; j < n; j++) {
            int i_gone = 0;
            if (clair_nbody_merge(i, j, &i_gone)) { hits++; if (i_gone) break; }
        }
    }
    return hits;
}

static int64_t clair_nbody_whfast(double h, int64_t steps, double G, double soft);
static int64_t clair_nbody_mercurius(double h, int64_t steps, double G, double soft, int64_t flags);

/* V0.70: the forces at the end of a leapfrog are those its next call starts with, if the program has
   moved nothing in between (checked to the last bit: the result is the same as computing them again) */
static struct { size_t n, cap; double *keep; double G, eps2, theta; int valid; } clair_fk;

static void clair_nbody_forces_keep(double G, double eps2) {
    size_t n = clair_nbody_n();
    if (8 * n > clair_fk.cap) {
        double *k = realloc(clair_fk.keep, 8 * n * sizeof(double));
        if (!k) { clair_fk.valid = 0; return; }
        clair_fk.keep = k;
        clair_fk.cap = 8 * n;
    }
    double *k = clair_fk.keep;
    memcpy(k, CLAIR_NB(x), n * sizeof(double)); memcpy(k + n, CLAIR_NB(y), n * sizeof(double));
    memcpy(k + 2 * n, CLAIR_NB(z), n * sizeof(double)); memcpy(k + 3 * n, CLAIR_NB(m), n * sizeof(double));
    memcpy(k + 4 * n, CLAIR_NB(r), n * sizeof(double));
    memcpy(k + 5 * n, clair_nb.ax, n * sizeof(double)); memcpy(k + 6 * n, clair_nb.ay, n * sizeof(double));
    memcpy(k + 7 * n, clair_nb.az, n * sizeof(double));
    clair_fk.n = n; clair_fk.G = G; clair_fk.eps2 = eps2; clair_fk.theta = clair_bh.theta; clair_fk.valid = 1;
}

static int clair_nbody_forces_kept(double G, double eps2) {
    size_t n = clair_nbody_n();
    if (!clair_fk.valid || clair_fk.n != n || clair_fk.G != G || clair_fk.eps2 != eps2 || clair_fk.theta != clair_bh.theta) return 0;
    const double *k = clair_fk.keep;
    if (memcmp(k, CLAIR_NB(x), n * sizeof(double)) || memcmp(k + n, CLAIR_NB(y), n * sizeof(double)) ||
        memcmp(k + 2 * n, CLAIR_NB(z), n * sizeof(double)) || memcmp(k + 3 * n, CLAIR_NB(m), n * sizeof(double)) ||
        memcmp(k + 4 * n, CLAIR_NB(r), n * sizeof(double))) return 0;
    memcpy(clair_nb.ax, k + 5 * n, n * sizeof(double)); memcpy(clair_nb.ay, k + 6 * n, n * sizeof(double));
    memcpy(clair_nb.az, k + 7 * n, n * sizeof(double));
    return 1;
}

/* n steps of h; flags: 1 collisions, 2 Yoshida's fourth order, 8 WHFast (V0.69). Gives the number of collisions. */
static int64_t clair_nbody_step(double h, int64_t steps, double G, double soft, int64_t flags) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    if (flags & 8) return clair_nbody_whfast(h, steps, G, soft);
    if (flags & 16) return clair_nbody_mercurius(h, steps, G, soft, flags);
    double eps2 = soft * soft;
    int64_t hits = 0;
    static const double w1 = 1.3512071919596578, w0 = -1.7024143839193153;     /* 1/(2-2^(1/3)), -2^(1/3)/(2-2^(1/3)) */
    if (!(flags & 2)) {
        if (!clair_nbody_forces_kept(G, eps2)) clair_nbody_forces(G, eps2);   /* V0.70: the forces of the last step, when nothing moved */
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
        clair_nbody_forces_keep(G, eps2);
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

/* 0 the kinetic energy, 1 the potential energy (2: the potential energy with the tree, V0.70) */
static double clair_nbody_energy(double G, double soft, int64_t what) {
    if (what == 2) return clair_bh.theta > 0 ? clair_bh_run(G, soft * soft, 1) : clair_nbody_energy(G, soft, 1);
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
        case 10: clair_bh.theta = v > 0 ? v : 0; break;   /* V0.70: the tree (0: direct sums) */
        case 12: clair_nb.theta_saved = v; break;     /* V0.70: s.theta, kept even without the tree */
        case 13: clair_nb.hill = v > 0 ? v : 3; break;    /* V0.71: MERCURIUS, the critical distance in Hill radii */
        case 14: clair_nb.c = v > 0 ? v : 0; break;       /* V0.71.1: general relativity (IAS15) */
        case 11: clair_bh.threads = (int)v; break;    /* V0.70: the cores (0: all) */
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
    double *x0, *v0, *a0, *at, *csx, *csv, *xs, *vs;
    double *g[7], *b[7], *e[7], *br[7], *er[7], *cb[7];   /* cb: what rounding took from the b */
    double dt_last;                                  /* the last step done (0: none yet) */
    const void *owner;                               /* the simulation the predictions belong to */
    int64_t steps, rejected, unconverged;
    double out[12];                                  /* what __nbody_out(i) gives */
} clair_ias;

static double clair_nbody_out(int64_t i) { return i >= 0 && i < 12 ? clair_ias.out[i] : 0; }

static int64_t clair_mc_enc_steps, clair_mc_ias_steps;   /* V0.71, MERCURIUS: the steps with an encounter, their IAS15 steps */

/* 0 the IAS15 steps done, 1 the steps done again, 2 the predictor-corrector that did not settle;
   MERCURIUS: 3 the steps with an encounter, 4 the IAS15 steps they took */
static double clair_nbody_stat(int64_t what) {
    if (what == 3 || what == 4) return (double)(what == 3 ? clair_mc_enc_steps : clair_mc_ias_steps);
    return what == 0 ? (double)clair_ias.steps : what == 1 ? (double)clair_ias.rejected : (double)clair_ias.unconverged;
}

/* the predictions start again from nothing (a collision, other bodies) */
static void clair_ias_forget(void) {
    if (clair_ias.mem) memset(clair_ias.mem, 0, clair_ias.cap * 50 * sizeof(double));
    clair_ias.dt_last = 0;
}

static void clair_ias_ready(void) {
    size_t n3 = 3 * clair_nbody_n();
    if (n3 > clair_ias.cap) {
        double *mem = calloc(n3 * 50, sizeof(double));      /* 8 vectors + 6 polynomials of 7 */
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
    clair_ias.vs = p + 49 * c;
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

static void clair_ias_put_v(const double *vel) {
    size_t n = clair_nbody_n();
    double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    for (size_t i = 0; i < n; i++) { vx[i] = vel[3 * i]; vy[i] = vel[3 * i + 1]; vz[i] = vel[3 * i + 2]; }
}

/* V0.71.1: general relativity, the first post-Newtonian term of the first body (the star) on each of the
   others: a = G M / (c^2 r^3) ((4 G M / r - v^2) r + 4 (r.v) v), r and v from the star (Schwarzschild, harmonic
   coordinates; the precession of Mercury: 43" a century).  The star takes the opposite push, weighted by the
   masses: the momentum is kept.  It needs the speeds: IAS15 predicts them inside its step. */
static void clair_nbody_gr(double G) {
    size_t n = clair_nbody_n();
    if (n < 2 || !(clair_nb.c > 0)) return;
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    if (!(m[0] > 0) || r[0] < 0) return;
    double GM = G * m[0], c2 = clair_nb.c * clair_nb.c, back[3] = { 0, 0, 0 };
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        double dx = x[i] - x[0], dy = y[i] - y[0], dz = z[i] - z[0], ux = vx[i] - vx[0], uy = vy[i] - vy[0], uz = vz[i] - vz[0];
        double d2 = dx * dx + dy * dy + dz * dz;
        if (!(d2 > 0)) continue;
        double d = sqrt(d2), f = GM / (c2 * d2 * d);
        double A = 4 * GM / d - (ux * ux + uy * uy + uz * uz), B = 4 * (dx * ux + dy * uy + dz * uz);
        double ax = f * (A * dx + B * ux), ay = f * (A * dy + B * uy), az = f * (A * dz + B * uz);
        clair_nb.ax[i] += ax; clair_nb.ay[i] += ay; clair_nb.az[i] += az;
        back[0] += m[i] * ax; back[1] += m[i] * ay; back[2] += m[i] * az;
    }
    clair_nb.ax[0] -= back[0] / m[0]; clair_nb.ay[0] -= back[1] / m[0]; clair_nb.az[0] -= back[2] / m[0];
}

static void (*clair_ias_hook)(double G, double eps2);   /* V0.71: other forces (MERCURIUS: the bodies of an encounter) */
static int64_t (*clair_ias_collide_hook)(void);         /* V0.71: more collisions (MERCURIUS: with the star) */

static void clair_ias_accel(double G, double eps2, double *out) {
    size_t n = clair_nbody_n();
    if (clair_ias_hook) clair_ias_hook(G, eps2);
    else { clair_nbody_forces(G, eps2); clair_nbody_gr(G); }
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
            if (clair_nb.c > 0 && !clair_ias_hook) {    /* V0.71.1: relativity needs the speeds there too */
                double *vs = clair_ias.vs;
                for (size_t k = 0; k < n3; k++) {
                    double d = b[6][k] / 8;
                    d = d * hs + b[5][k] / 7;
                    d = d * hs + b[4][k] / 6;
                    d = d * hs + b[3][k] / 5;
                    d = d * hs + b[2][k] / 4;
                    d = d * hs + b[1][k] / 3;
                    d = d * hs + b[0][k] / 2;
                    d = d * hs + a0[k];
                    vs[k] = (d * ht - csv[k]) + v0[k];
                }
                clair_ias_put_v(vs);
            }
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
            clair_ias_put_v(v0);
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
        if (!last || want >= dt || trying < want) dt = trying;   /* a last step cut short does not shrink the next ones */
        if (flags & 1) {
            int64_t k = clair_nbody_collide();
            if (clair_ias_collide_hook) k += clair_ias_collide_hook();
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

/* ---- V0.69: WHFast, the symplectic integrator of Wisdom and Holman (s.integrator = "whfast") ----
 *
 * After Wisdom & Holman (1991), "Symplectic maps for the N-body problem", AJ 102, 1528, and Rein &
 * Tamayo (2015), "WHFast: a fast and unbiased implementation of a symplectic Wisdom-Holman integrator
 * for long-term gravitational simulations", MNRAS 452, 376.  Written here from the articles and from
 * Danby, "Fundamentals of Celestial Mechanics" (universal variables); REBOUND (GPL) was only used to
 * compare the results.
 *
 * For a dominant body (the first one) and bodies that turn around it: in Jacobi coordinates (each body
 * from the center of mass of those before it), each body follows its own Kepler orbit exactly (the
 * drift), and the pulls of the bodies on one another are added as kicks between the drifts:
 * half a drift, a kick, a drift, a kick, ... half a drift.  The energy does not drift away, even over
 * millions of orbits; the error is about (mass of the planets / mass of the star) x (dt / period)^2.
 * The step is fixed (s.dt): a twentieth of the shortest orbit or less.
 */

static struct {
    size_t cap, n;                                   /* the bodies still there, in order */
    size_t *idx;
    double *jx, *jv, *ja, *eta;                      /* Jacobi positions, velocities, accelerations; masses up to i */
} clair_wh;

/* c0 .. c3 of Stumpff for z: c0 = cos(sqrt z), c1 = sin(sqrt z) / sqrt z, ... (any sign of z):
   z is divided by 4 until small, the series is summed, then doubled back (Danby) */
static void clair_wh_stumpff(double z, double c[4]) {
    /* (-1)^j / (2j+2)! and (-1)^j / (2j+3)!: for |z| <= 0.1 the next terms are below 1e-20 */
    static const double s2[8] = { 1.0 / 2, -1.0 / 24, 1.0 / 720, -1.0 / 40320, 1.0 / 3628800, -1.0 / 479001600,
                                  1.0 / 87178291200.0, -1.0 / 20922789888000.0 };
    static const double s3[8] = { 1.0 / 6, -1.0 / 120, 1.0 / 5040, -1.0 / 362880, 1.0 / 39916800,
                                  -1.0 / 6227020800.0, 1.0 / 1307674368000.0, -1.0 / 355687428096000.0 };
    int k = 0;
    while (fabs(z) > 0.1) { z /= 4; k++; }
    double c2 = s2[7], c3 = s3[7];                   /* the series in z, smallest terms first (Horner) */
    for (int j = 6; j >= 0; j--) { c2 = c2 * z + s2[j]; c3 = c3 * z + s3[j]; }
    double c1 = 1 - z * c3, c0 = 1 - z * c2;
    for (; k > 0; k--) {                             /* c(4z) from c(z) */
        c3 = (c2 + c0 * c3) / 4;
        c2 = c1 * c1 / 2;
        c1 = c0 * c1;
        c0 = 2 * c0 * c0 - 1;
    }
    c[0] = c0; c[1] = c1; c[2] = c2; c[3] = c3;
}

/* the Kepler orbit for mu = G M: the place x and the speed v after the time dt (in place) */
static void clair_wh_kepler(double *x, double *v, double mu, double dt) {
    double r0 = sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (!(r0 > 0) || mu == 0) {                      /* nothing pulls it: a straight line */
        for (int k = 0; k < 3; k++) x[k] += dt * v[k];
        return;
    }
    double v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    double beta = 2 * mu / r0 - v2;                  /* mu / a: more than 0 for an ellipse */
    double eta0 = x[0] * v[0] + x[1] * v[1] + x[2] * v[2];
    double zeta0 = mu - beta * r0;
    double c[4], G[4], X;
    /* solve r0 X + eta0 G2 + zeta0 G3 = dt for the universal variable X (G_k = X^k c_k(beta X^2)) */
    double t = dt;
    if (beta > 0) {                                  /* a whole number of orbits does not change anything */
        double period = 2 * 3.141592653589793238 * mu / (beta * sqrt(beta));
        if (fabs(t) > period) t = fmod(t, period);
        X = t / r0 * (1 - t * eta0 / (2 * r0 * r0));
    } else {
        X = t / r0;
    }
    int ok = 0;
    double last = NAN, before = NAN;
    for (int it = 0; it < 50; it++) {                /* Newton, then Laguerre-Conway if it wanders */
        clair_wh_stumpff(beta * X * X, c);
        G[0] = c[0]; G[1] = X * c[1]; G[2] = X * X * c[2]; G[3] = X * X * X * c[3];
        double F = r0 * X + eta0 * G[2] + zeta0 * G[3] - t;
        double F1 = r0 + eta0 * G[1] + zeta0 * G[2];
        double F2 = eta0 * G[0] + zeta0 * G[1];
        double Xn;
        if (it < 8) Xn = X - F / F1;
        else {
            double d = sqrt(fabs(16 * F1 * F1 - 20 * F * F2));
            Xn = X - 5 * F / (F1 + (F1 >= 0 ? d : -d));
        }
        if (!isfinite(Xn)) break;
        before = last;
        last = X;
        X = Xn;
        if (X == last || X == before) { ok = X == last ? 2 : 1; break; }
    }
    if (!ok) {                                       /* bisection: slow but sure (F grows with X) */
        double lo = 0, hi = 0, step = t / r0;
        double sg = t >= 0 ? 1 : -1;
        if (step == 0) return;
        hi = step;
        for (int it = 0; it < 200; it++) {
            clair_wh_stumpff(beta * hi * hi, c);
            double F = r0 * hi + eta0 * hi * hi * c[2] + zeta0 * hi * hi * hi * c[3] - t;
            if (sg * F >= 0) break;
            lo = hi;
            hi *= 2;
        }
        for (int it = 0; it < 200; it++) {
            double mid = (lo + hi) / 2;
            if (mid == lo || mid == hi) break;
            clair_wh_stumpff(beta * mid * mid, c);
            double F = r0 * mid + eta0 * mid * mid * c[2] + zeta0 * mid * mid * mid * c[3] - t;
            if (sg * F >= 0) hi = mid; else lo = mid;
        }
        X = (lo + hi) / 2;
    }
    if (ok != 2) clair_wh_stumpff(beta * X * X, c); /* (2: c is already the one of X) */
    G[0] = c[0]; G[1] = X * c[1]; G[2] = X * X * c[2]; G[3] = X * X * X * c[3];
    double r = r0 + eta0 * G[1] + zeta0 * G[2];
    double fm1 = -mu * G[2] / r0;                   /* f - 1 */
    double g = r0 * G[1] + eta0 * G[2];
    double fd = -mu * G[1] / (r0 * r);
    double gdm1 = -mu * G[2] / r;                   /* g' - 1 */
    double nx[3], nv[3];
    for (int k = 0; k < 3; k++) {
        nx[k] = x[k] + (fm1 * x[k] + g * v[k]);
        nv[k] = v[k] + (fd * x[k] + gdm1 * v[k]);
    }
    for (int k = 0; k < 3; k++) { x[k] = nx[k]; v[k] = nv[k]; }
}

/* the bodies still there, and their Jacobi coordinates */
static void clair_wh_to_jacobi(void) {
    size_t n = clair_nbody_n();
    if (n > clair_wh.cap) {
        size_t cap = n < 16 ? 16 : n;
        size_t *idx = realloc(clair_wh.idx, cap * sizeof(size_t));
        if (idx) clair_wh.idx = idx;
        double *a = realloc(clair_wh.jx, 3 * cap * sizeof(double));
        if (a) clair_wh.jx = a;
        double *b = realloc(clair_wh.jv, 3 * cap * sizeof(double));
        if (b) clair_wh.jv = b;
        double *c = realloc(clair_wh.ja, 3 * cap * sizeof(double));
        if (c) clair_wh.ja = c;
        double *e = realloc(clair_wh.eta, cap * sizeof(double));
        if (e) clair_wh.eta = e;
        if (!idx || !a || !b || !c || !e) clair_nbody_die("out of memory");
        clair_wh.cap = cap;
    }
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    size_t q = 0;
    for (size_t i = 0; i < n; i++) if (r[i] >= 0) clair_wh.idx[q++] = i;
    clair_wh.n = q;
    if (q == 0) return;
    if (!(m[clair_wh.idx[0]] > 0)) clair_nbody_die("WHFast: the first body is the one the others turn around: it needs a mass");
    double eta = 0, s[6] = { 0 };
    for (size_t k = 0; k < q; k++) {
        size_t i = clair_wh.idx[k];
        double p[6] = { x[i], y[i], z[i], vx[i], vy[i], vz[i] };
        if (k > 0)
            for (int d = 0; d < 3; d++) { clair_wh.jx[3 * k + d] = p[d] - s[d] / eta; clair_wh.jv[3 * k + d] = p[3 + d] - s[3 + d] / eta; }
        for (int d = 0; d < 6; d++) s[d] += m[i] * p[d];
        eta += m[i];
        clair_wh.eta[k] = eta;
    }
    for (int d = 0; d < 3; d++) { clair_wh.jx[d] = s[d] / eta; clair_wh.jv[d] = s[3 + d] / eta; }
}

/* back to the places (what = 0) or the speeds (what = 1) of the bodies */
static void clair_wh_from_jacobi(int what) {
    size_t q = clair_wh.n;
    if (q == 0) return;
    const double *m = CLAIR_NB(m);
    double *out[3] = { what ? CLAIR_NB(vx) : CLAIR_NB(x), what ? CLAIR_NB(vy) : CLAIR_NB(y), what ? CLAIR_NB(vz) : CLAIR_NB(z) };
    const double *J = what ? clair_wh.jv : clair_wh.jx;
    double s[3];
    for (int d = 0; d < 3; d++) s[d] = J[d] * clair_wh.eta[q - 1];        /* the sum of m x of all the bodies */
    for (size_t k = q - 1; k > 0; k--) {
        size_t i = clair_wh.idx[k];
        for (int d = 0; d < 3; d++) {
            double R = (s[d] - m[i] * J[3 * k + d]) / clair_wh.eta[k];    /* the center of mass of the bodies before */
            double xi = J[3 * k + d] + R;
            out[d][i] = xi;
            s[d] -= m[i] * xi;
        }
    }
    size_t i0 = clair_wh.idx[0];
    for (int d = 0; d < 3; d++) out[d][i0] = s[d] / m[i0];
}

static void clair_wh_drift(double G, double h) {
    for (size_t k = 1; k < clair_wh.n; k++) clair_wh_kepler(clair_wh.jx + 3 * k, clair_wh.jv + 3 * k, G * clair_wh.eta[k], h);
    for (int d = 0; d < 3; d++) clair_wh.jx[d] += h * clair_wh.jv[d];    /* the center of mass goes straight */
}

/* the kick: the pulls of the bodies on one another, but not what the Kepler orbits already do */
static void clair_wh_kick(double G, double eps2, double h) {
    size_t q = clair_wh.n;
    if (q < 2) return;
    clair_wh_from_jacobi(0);
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m);
    double *a = clair_wh.ja;
    for (size_t k = 0; k < 3 * q; k++) a[k] = 0;
    for (size_t k = 0; k < q; k++) {                 /* every pair but the first one (its Kepler orbit) */
        size_t i = clair_wh.idx[k];
        for (size_t l = k + 1; l < q; l++) {
            if (k == 0 && l == 1) continue;
            size_t j = clair_wh.idx[l];
            double dx = x[j] - x[i], dy = y[j] - y[i], dz = z[j] - z[i];
            double d2 = dx * dx + dy * dy + dz * dz + eps2;
            if (d2 == 0) continue;
            double inv = 1.0 / (d2 * sqrt(d2));
            a[3 * k] += G * m[j] * inv * dx; a[3 * k + 1] += G * m[j] * inv * dy; a[3 * k + 2] += G * m[j] * inv * dz;
            a[3 * l] -= G * m[i] * inv * dx; a[3 * l + 1] -= G * m[i] * inv * dy; a[3 * l + 2] -= G * m[i] * inv * dz;
        }
    }
    double s[3] = { 0, 0, 0 };                       /* Jacobi accelerations: minus that of the center of mass before */
    for (size_t k = 0; k < q; k++) {
        size_t i = clair_wh.idx[k];
        double aj[3];
        for (int d = 0; d < 3; d++) aj[d] = k > 0 ? a[3 * k + d] - s[d] / clair_wh.eta[k - 1] : 0;
        for (int d = 0; d < 3; d++) s[d] += m[i] * a[3 * k + d];
        if (k == 0) continue;
        double add = 0;
        if (k > 1) {                                 /* what the Kepler orbit of this body counts, given back */
            const double *r = clair_wh.jx + 3 * k;
            double r2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
            add = G * clair_wh.eta[k] / (r2 * sqrt(r2));
        }
        for (int d = 0; d < 3; d++) clair_wh.jv[3 * k + d] += h * (aj[d] + add * clair_wh.jx[3 * k + d]);
    }
}

/* steps of h with WHFast: half a drift, then kick and drift, ..., the last drift by half */
static int64_t clair_nbody_whfast(double h, int64_t steps, double G, double soft) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    if (steps <= 0) return 0;
    double eps2 = soft * soft;
    clair_wh_to_jacobi();
    if (clair_wh.n == 0) return 0;
    clair_wh_drift(G, h / 2);
    for (int64_t st = 0; st < steps; st++) {
        clair_wh_kick(G, eps2, h);
        clair_wh_drift(G, st == steps - 1 ? h / 2 : h);
    }
    clair_wh_from_jacobi(0);
    clair_wh_from_jacobi(1);
    return 0;
}

/* ---- V0.71: MERCURIUS, the hybrid integrator (s.integrator = "mercurius") ----
 *
 * After Chambers (1999), "A hybrid symplectic integrator that permits close encounters between massive
 * bodies", MNRAS 304, 793, and Rein, Hernandez, Tamayo et al. (2019), "Hybrid symplectic integrators for
 * planetary dynamics", MNRAS 485, 5490.  Written here from the articles; REBOUND (GPL) is the reference
 * the results are checked against (same start, same steps: the same numbers), no code is copied from it.
 *
 * Democratic heliocentric coordinates: places from the star, speeds from the center of mass.  Each
 * step: half a kick (the planets pull one another), half a "jump" (the star moves under the total
 * momentum), each planet on its Kepler orbit around the star, half a jump, half a kick.  The pull of
 * two planets is cut in two by a smooth function L of their distance (the one of Mercury, Chambers
 * 1999): far away (L = 1) it is all in the kicks, as in WHFast; within a critical distance (s.hill Hill
 * radii, or how far they go in 0.4 dt) part of it goes with the Kepler orbits.  The planets that come
 * that close during a step (found from their places and speeds at both ends) do that step again with
 * IAS15, with the star and the rest of their pull: precise even when two planets pass a few planet
 * radii apart, at the speed of WHFast otherwise.  Collisions (s.merge) are looked for after each IAS15
 * step of the encounters, with the star as well.  The critical distances are computed at the first step
 * and kept in s.crit (each simulation its own): again only when bodies are added or s.dt, G or s.hill
 * change, and s.save() keeps them, so that a simulation read again goes on exactly the same.
 */

static struct {
    size_t cap;
    double *dcrit, *bk;                              /* the critical distances (in s.crit); the bodies before Kepler */
    size_t n;                                        /* the bodies (bk holds 6 lists of n) */
    int *enc;                                        /* in an encounter at this step */
    double m0, r0, G;                                /* the star (its mass and size change when it takes a body) */
    size_t *map;                                     /* encounter: the body in the lists of each one integrated */
    size_t nmap;
    double *buf;                                     /* encounter: the bodies integrated (8 lists) */
    const void *owner;                               /* the simulation the counts belong to */
} clair_mc;

/* the changeover function of Mercury (Chambers 1999): 0 near, 1 far, smooth (C2) in between */
static double clair_mc_L(double d, double dcrit) {
    double y = (d - 0.1 * dcrit) / (0.9 * dcrit);
    if (y < 0) return 0;
    if (y > 1) return 1;
    return 10 * (y * y * y) - 15 * (y * y * y * y) + 6 * (y * y * y * y * y);   /* 10 y^3 - 15 y^4 + 6 y^5 */
}

static void clair_mc_to_dh(double cp[3], double cv[3]) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double M = 0;
    for (int d = 0; d < 3; d++) cp[d] = cv[d] = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        cp[0] += m[i] * x[i]; cp[1] += m[i] * y[i]; cp[2] += m[i] * z[i];
        cv[0] += m[i] * vx[i]; cv[1] += m[i] * vy[i]; cv[2] += m[i] * vz[i];
        M += m[i];
    }
    for (int d = 0; d < 3; d++) { cp[d] /= M; cv[d] /= M; }
    double s0[3] = { x[0], y[0], z[0] };
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0 && i > 0) continue;
        x[i] -= s0[0]; y[i] -= s0[1]; z[i] -= s0[2];
        vx[i] -= cv[0]; vy[i] -= cv[1]; vz[i] -= cv[2];
    }
}

static void clair_mc_from_dh(const double cp[3], const double cv[3]) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double s[3] = { 0, 0, 0 }, p[3] = { 0, 0, 0 }, M = 0;
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        s[0] += m[i] * x[i]; s[1] += m[i] * y[i]; s[2] += m[i] * z[i];
        p[0] += m[i] * vx[i]; p[1] += m[i] * vy[i]; p[2] += m[i] * vz[i];
        M += m[i];
    }
    M += m[0];
    double s0[3] = { cp[0] - s[0] / M, cp[1] - s[1] / M, cp[2] - s[2] / M };   /* the star, from the center of mass */
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        x[i] += s0[0]; y[i] += s0[1]; z[i] += s0[2];
        vx[i] += cv[0]; vy[i] += cv[1]; vz[i] += cv[2];
    }
    x[0] = s0[0]; y[0] = s0[1]; z[0] = s0[2];
    vx[0] = cv[0] - p[0] / m[0]; vy[0] = cv[1] - p[1] / m[0]; vz[0] = cv[2] - p[2] / m[0];
}

/* the critical distance of each planet: s.hill Hill radii, or how far it goes in 0.4 dt, or twice its size */
static void clair_mc_dcrit(double G, double h) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    clair_mc.dcrit[0] = 2 * (r[0] > 0 ? r[0] : 0);
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) { clair_mc.dcrit[i] = 0; continue; }
        double dv[3] = { vx[i] - vx[0], vy[i] - vy[0], vz[i] - vz[0] };
        double d = sqrt(x[i] * x[i] + y[i] * y[i] + z[i] * z[i]), v2 = dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2];
        double GM = G * (m[0] + m[i]), a = GM * d / (2 * GM - d * v2), vc = sqrt(GM / fabs(a));
        double dc = 0;
        if (vc * 0.4 * h > dc) dc = vc * 0.4 * h;
        if (sqrt(v2) * 0.4 * h > dc) dc = sqrt(v2) * 0.4 * h;
        double hr = clair_nb.hill * a * cbrt(m[i] / (3 * m[0]));
        if (hr > dc) dc = hr;
        if (2 * r[i] > dc) dc = 2 * r[i];
        clair_mc.dcrit[i] = dc;
    }
}

/* the kick: the planets on one another, the far part (L) */
static void clair_mc_kick(double G, double eps2, double h) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz), *ax = clair_nb.ax, *ay = clair_nb.ay, *az = clair_nb.az;
    for (size_t i = 0; i < n; i++) ax[i] = ay[i] = az[i] = 0;
    for (size_t i = 2; i < n; i++) {
        if (r[i] < 0) continue;
        for (size_t j = 1; j < i; j++) {
            if (r[j] < 0) continue;
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
            double d = sqrt(dx * dx + dy * dy + dz * dz + eps2);
            double dc = clair_mc.dcrit[i] > clair_mc.dcrit[j] ? clair_mc.dcrit[i] : clair_mc.dcrit[j];
            double f = G * clair_mc_L(d, dc) / (d * d * d);
            ax[i] -= f * m[j] * dx; ay[i] -= f * m[j] * dy; az[i] -= f * m[j] * dz;
            ax[j] += f * m[i] * dx; ay[j] += f * m[i] * dy; az[j] += f * m[i] * dz;
        }
    }
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        vx[i] += h * ax[i]; vy[i] += h * ay[i]; vz[i] += h * az[i];
    }
}

/* the jump: the star moves under the momentum of the planets */
static void clair_mc_jump(double h) {
    size_t n = clair_nbody_n();
    double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z);
    const double *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double p[3] = { 0, 0, 0 };
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        p[0] += m[i] * vx[i]; p[1] += m[i] * vy[i]; p[2] += m[i] * vz[i];
    }
    for (int d = 0; d < 3; d++) p[d] /= m[0];
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        x[i] += h * p[0]; y[i] += h * p[1]; z[i] += h * p[2];
    }
}

/* the forces of an encounter (heliocentric): the star, and the near part (1 - L) of the pulls between them */
static void clair_mc_encounter_forces(double G, double eps2) {
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    double *ax = clair_nb.ax, *ay = clair_nb.ay, *az = clair_nb.az;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) { ax[i] = ay[i] = az[i] = 0; continue; }
        double d = sqrt(x[i] * x[i] + y[i] * y[i] + z[i] * z[i] + eps2), f = -G / (d * d * d) * clair_mc.m0;
        ax[i] = f * x[i]; ay[i] = f * y[i]; az[i] = f * z[i];
    }
    for (size_t i = 1; i < n; i++) {
        if (r[i] < 0) continue;
        for (size_t j = 0; j < i; j++) {
            if (r[j] < 0) continue;
            double dx = x[i] - x[j], dy = y[i] - y[j], dz = z[i] - z[j];
            double d = sqrt(dx * dx + dy * dy + dz * dz + eps2);
            double di = clair_mc.dcrit[clair_mc.map[i]], dj = clair_mc.dcrit[clair_mc.map[j]];
            double f = G * (1 - clair_mc_L(d, di > dj ? di : dj)) / (d * d * d);
            ax[i] -= f * m[j] * dx; ay[i] -= f * m[j] * dy; az[i] -= f * m[j] * dz;
            ax[j] += f * m[i] * dx; ay[j] += f * m[i] * dy; az[j] += f * m[i] * dz;
        }
    }
}

/* will i and j come within 1.1 critical distances during the step?  Their distance squared is a cubic in
   time, from its values and slopes at both ends (Hermite); where its slope is 0 inside the step gives the
   closest point.  p: the places and speeds after the step, b: before (6 lists of n) */
static int clair_mc_close(const double *const p[6], const double *b, size_t n, size_t i, size_t j, double h) {
    double dn[3], vn[3], dO[3], vO[3];
    for (int d = 0; d < 3; d++) {
        dn[d] = p[d][i] - p[d][j]; vn[d] = p[3 + d][i] - p[3 + d][j];
        dO[d] = b[d * n + i] - b[d * n + j]; vO[d] = b[(3 + d) * n + i] - b[(3 + d) * n + j];
    }
    double rn = dn[0] * dn[0] + dn[1] * dn[1] + dn[2] * dn[2], ro = dO[0] * dO[0] + dO[1] * dO[1] + dO[2] * dO[2];
    double drn = 2 * (dn[0] * vn[0] + dn[1] * vn[1] + dn[2] * vn[2]), dro = 2 * (dO[0] * vO[0] + dO[1] * vO[1] + dO[2] * vO[2]);
    double dc = clair_mc.dcrit[i] > clair_mc.dcrit[j] ? clair_mc.dcrit[i] : clair_mc.dcrit[j], lim = 1.21 * dc * dc;
    double rmin = rn < ro ? rn : ro;
    /* far apart: the two slope terms of the cubic are each at most 4/27 of h |slope|, so it stays above
       rmin - 4/27 h (|dro| + |drn|) (with a margin for the rounding: same answer as the full test) */
    double sl = h * (fabs(dro) + fabs(drn));
    if (rmin - 4.0 / 27 * sl - 1e-12 * (rn + ro + sl) > lim) return 0;
    double A = 6 * (ro - rn) + 3 * h * (dro + drn), B = 6 * (rn - ro) - 2 * h * (2 * dro + drn), C = h * dro;
    double disc = B * B - 4 * A * C, sq = sqrt(disc > 0 ? disc : 0);
    for (int k = 0; k < 2; k++) {                    /* where its slope is 0, inside the step */
        double tm = (-B + (k ? -sq : sq)) / (2 * A);
        if (tm > 0 && tm < 1) {
            double rm = (1 - tm) * (1 - tm) * (1 + 2 * tm) * ro + tm * tm * (3 - 2 * tm) * rn
                      + tm * (1 - tm) * (1 - tm) * h * dro - tm * tm * (1 - tm) * h * drn;
            if (rm < 0) rm = 0;
            if (rm < rmin) rmin = rm;
        }
    }
    return rmin < lim;
}

/* in an encounter, after each IAS15 step: the bodies that touch the star (at 0) fall into it */
static int64_t clair_mc_star_hits(void) {
    if (!(clair_mc.r0 > 0)) return 0;
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z);
    double *m = CLAIR_NB(m), *r = CLAIR_NB(r), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    int64_t hits = 0;
    for (size_t i = 0; i < n; i++) {
        if (r[i] < 0) continue;
        double d = sqrt(x[i] * x[i] + y[i] * y[i] + z[i] * z[i]);
        if (d >= clair_mc.r0 + r[i]) continue;
        clair_mc.m0 += m[i];                         /* the star takes its mass (its speed comes from the center of mass) */
        clair_mc.r0 = cbrt(clair_mc.r0 * clair_mc.r0 * clair_mc.r0 + r[i] * r[i] * r[i]);
        m[i] = 0; r[i] = -1;
        vx[i] = vy[i] = vz[i] = 0;
        hits++;
    }
    return hits;
}

/* the planets in an encounter: this step again, with IAS15, from where they were before it */
static int64_t clair_mc_encounter(double G, double soft, int64_t flags, double h) {
    size_t n = clair_nbody_n(), q = 0;
    for (size_t i = 1; i < n; i++) if (clair_mc.enc[i]) clair_mc.map[q++] = i;
    if (q == 0) return 0;
    clair_mc.nmap = q;
    double *buf = clair_mc.buf;
    const double *b = clair_mc.bk;
    for (size_t k = 0; k < q; k++) {
        size_t i = clair_mc.map[k];
        for (int d = 0; d < 6; d++) buf[d * q + k] = b[d * n + i];
        buf[6 * q + k] = CLAIR_NB(m)[i];
        buf[7 * q + k] = CLAIR_NB(r)[i];
    }
    ClairListObj L[8];
    for (int d = 0; d < 8; d++) { L[d].data = buf + d * q; L[d].len = q; L[d].capacity = q; L[d].elem_size = sizeof(double); }
    ClairList keep[8] = { clair_nb.x, clair_nb.y, clair_nb.z, clair_nb.vx, clair_nb.vy, clair_nb.vz, clair_nb.m, clair_nb.r };
    clair_nb.x = &L[0]; clair_nb.y = &L[1]; clair_nb.z = &L[2]; clair_nb.vx = &L[3]; clair_nb.vy = &L[4]; clair_nb.vz = &L[5];
    clair_nb.m = &L[6]; clair_nb.r = &L[7];
    clair_ias_hook = clair_mc_encounter_forces;
    clair_ias_collide_hook = clair_mc_star_hits;
    clair_ias.owner = NULL;                          /* a new IAS15 for each encounter */
    int64_t hits = clair_ias_run(h, G, soft, flags & 1, 1e-9, h * 1e-4, 0);
    clair_mc_ias_steps += clair_ias.steps;
    clair_mc_enc_steps++;
    clair_ias_hook = NULL;
    clair_ias_collide_hook = NULL;
    clair_ias.owner = NULL;
    clair_nb.x = keep[0]; clair_nb.y = keep[1]; clair_nb.z = keep[2]; clair_nb.vx = keep[3]; clair_nb.vy = keep[4]; clair_nb.vz = keep[5];
    clair_nb.m = keep[6]; clair_nb.r = keep[7];
    double *lists[8] = { CLAIR_NB(x), CLAIR_NB(y), CLAIR_NB(z), CLAIR_NB(vx), CLAIR_NB(vy), CLAIR_NB(vz), CLAIR_NB(m), CLAIR_NB(r) };
    for (size_t k = 0; k < q; k++) for (int d = 0; d < 8; d++) lists[d][clair_mc.map[k]] = buf[d * q + k];
    CLAIR_NB(m)[0] = clair_mc.m0;
    CLAIR_NB(r)[0] = clair_mc.r0;
    return hits;
}

static void clair_mc_reserve(size_t n) {
    if (n > clair_mc.cap) {
        size_t cap = n < 16 ? 16 : n;
        double *bk = realloc(clair_mc.bk, 6 * cap * sizeof(double));
        if (bk) clair_mc.bk = bk;
        int *en = realloc(clair_mc.enc, cap * sizeof(int));
        if (en) clair_mc.enc = en;
        size_t *mp = realloc(clair_mc.map, cap * sizeof(size_t));
        if (mp) clair_mc.map = mp;
        double *bf = realloc(clair_mc.buf, 8 * cap * sizeof(double));
        if (bf) clair_mc.buf = bf;
        if (!bk || !en || !mp || !bf) clair_nbody_die("out of memory");
        clair_mc.cap = cap;
    }
}

/* s.crit is up to date: computed for these bodies, with this dt, G and s.hill (V0.71) */
static int clair_mc_crit_ok(size_t n, double h, double G) {
    ClairList c = clair_nb.crit;
    if (!c || c->len != n + 3) return 0;
    const double *v = (const double *)c->data;
    return v[n] == h && v[n + 1] == G && v[n + 2] == clair_nb.hill;
}

static int64_t clair_nbody_mercurius(double h, int64_t steps, double G, double soft, int64_t flags) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    size_t n = clair_nbody_n();
    if (steps <= 0 || n < 2) return 0;
    double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    if (!(m[0] > 0) || r[0] < 0) clair_nbody_die("MERCURIUS: the first body is the star the others turn around: it needs a mass");
    clair_mc_reserve(n);
    /* the critical distances: computed at the first step, then kept in s.crit (as REBOUND does), again
       when bodies were added or s.dt, G or s.hill changed */
    if (!clair_nb.crit || clair_nb.crit->len != n + 3) clair_nbody_die("MERCURIUS: s.crit is kept by the module (one value per body, then 3)");
    int same = clair_mc_crit_ok(n, h, G);
    double *crit = (double *)clair_nb.crit->data;
    clair_mc.dcrit = crit;
    double cp[3], cv[3], eps2 = soft * soft;
    clair_mc_to_dh(cp, cv);
    if (!same) {
        clair_mc_dcrit(G, h);
        crit[n] = h; crit[n + 1] = G; crit[n + 2] = clair_nb.hill;
    }
    clair_mc.n = n;
    clair_mc.m0 = m[0];
    clair_mc.r0 = r[0];
    clair_mc.G = G;
    if (clair_mc.owner != (const void *)clair_nb.x) {   /* another simulation: the counts start again */
        clair_mc.owner = clair_nb.x;
        clair_mc_enc_steps = clair_mc_ias_steps = 0;
    }
    int64_t hits = 0;
    clair_mc_kick(G, eps2, h / 2);
    for (int64_t st = 0; st < steps; st++) {
        clair_mc_jump(h / 2);
        for (int d = 0; d < 3; d++) cp[d] += h * cv[d];
        double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
        double *cur[6] = { x, y, z, vx, vy, vz };
        for (int d = 0; d < 6; d++) memcpy(clair_mc.bk + d * n, cur[d], n * sizeof(double));
        for (size_t i = 1; i < n; i++) {             /* each planet on its Kepler orbit around the star */
            if (r[i] < 0) continue;
            double p[3] = { x[i], y[i], z[i] }, v[3] = { vx[i], vy[i], vz[i] };
            clair_wh_kepler(p, v, G * m[0], h);
            x[i] = p[0]; y[i] = p[1]; z[i] = p[2]; vx[i] = v[0]; vy[i] = v[1]; vz[i] = v[2];
        }
        int any = 0;
        const double *const now[6] = { x, y, z, vx, vy, vz };
        for (size_t i = 0; i < n; i++) clair_mc.enc[i] = 0;
        for (size_t i = 0; i < n; i++) {             /* the encounters of this step (the star counts, for its size) */
            if (r[i] < 0) continue;
            for (size_t j = i + 1; j < n; j++) {
                if (r[j] < 0) continue;
                if (clair_mc_close(now, clair_mc.bk, n, i, j, h)) { clair_mc.enc[j] = 1; if (i > 0) clair_mc.enc[i] = 1; any = 1; }
            }
        }
        if (any) hits += clair_mc_encounter(G, soft, flags, h);
        clair_mc_jump(h / 2);
        clair_mc_kick(G, eps2, st == steps - 1 ? h / 2 : h);
    }
    clair_mc_from_dh(cp, cv);
    return hits;
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

/* ---- V0.70: a Plummer sphere (a cluster of stars in equilibrium) ----
 * Aarseth, Henon & Wielen (1974), "A comparison of numerical methods for the study of star cluster
 * dynamics", A&A 37, 183: the radius from the mass inside it, the speed by von Neumann's rejection on
 * q^2 (1 - q^2)^(7/2), both in random directions.  The random numbers are SplitMix64 (Steele, Lea &
 * Flood 2014): the same seed gives the same cluster everywhere.
 */

static uint64_t clair_rng_state;

static uint64_t clair_rng_next(void) {
    uint64_t z = (clair_rng_state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static double clair_rng_uniform(void) { return (double)(clair_rng_next() >> 11) * (1.0 / 9007199254740992.0); }

static struct { size_t n, cap; double *v; } clair_gen;

static void clair_rng_direction(double len, double out[3]) {
    double z = 2 * clair_rng_uniform() - 1, ph = 2 * 3.141592653589793238 * clair_rng_uniform(), s = sqrt(1 - z * z);
    out[0] = len * s * cos(ph); out[1] = len * s * sin(ph); out[2] = len * z;
}

/* n stars of a cluster of mass M and Plummer radius a (G given), around 0, not moving as a whole */
static int64_t clair_nbody_plummer(int64_t n, double G, double M, double a, int64_t seed) {
    if (n <= 0) return 0;
    if (!(M > 0) || !(a > 0) || !(G > 0)) clair_nbody_die("add_plummer(): the mass, the radius and G must be more than 0");
    if ((size_t)n > clair_gen.cap) {
        double *v = realloc(clair_gen.v, (size_t)n * 6 * sizeof(double));
        if (!v) clair_nbody_die("out of memory");
        clair_gen.v = v;
        clair_gen.cap = (size_t)n;
    }
    clair_rng_state = (uint64_t)seed;
    double c[6] = { 0, 0, 0, 0, 0, 0 };
    for (int64_t i = 0; i < n; i++) {
        double X;
        do X = clair_rng_uniform(); while (X < 1e-10 || X > 0.999);       /* the mass inside: no star farther than 22 a */
        double r = a / sqrt(pow(X, -2.0 / 3) - 1);
        double q, g;
        do { q = clair_rng_uniform(); g = 0.1 * clair_rng_uniform(); } while (g > q * q * pow(1 - q * q, 3.5));
        double ve = sqrt(2 * G * M) * pow(r * r + a * a, -0.25);         /* the speed of escape there */
        double *v = clair_gen.v + 6 * i;
        clair_rng_direction(r, v);
        clair_rng_direction(q * ve, v + 3);
        for (int k = 0; k < 6; k++) c[k] += v[k];
    }
    for (int64_t i = 0; i < n; i++) for (int k = 0; k < 6; k++) clair_gen.v[6 * i + k] -= c[k] / (double)n;
    clair_gen.n = (size_t)n;
    return n;
}

static const double *clair_q_keys;

static int clair_q_less(const void *a, const void *b) {
    double x = clair_q_keys[*(const size_t *)a], y = clair_q_keys[*(const size_t *)b];
    return (x > y) - (x < y);
}

/* the smallest d such that the weights of the d below it reach want (the Lagrange radii) */
static double clair_nbody_quantile(ClairList d, ClairList w, double want) {
    size_t n = d ? d->len : 0;
    if (!n || !w || w->len != n) return 0;
    size_t *idx = malloc(n * sizeof(size_t));
    if (!idx) clair_nbody_die("out of memory");
    for (size_t i = 0; i < n; i++) idx[i] = i;
    clair_q_keys = (const double *)d->data;
    qsort(idx, n, sizeof(size_t), clair_q_less);
    double sum = 0, out = clair_q_keys[idx[n - 1]];
    const double *wv = (const double *)w->data;
    for (size_t i = 0; i < n; i++) {
        sum += wv[idx[i]];
        if (sum >= want) { out = clair_q_keys[idx[i]]; break; }
    }
    free(idx);
    return out;
}

/* star i, 0-2 its place, 3-5 its speed */
static double clair_nbody_gen(int64_t i, int64_t k) {
    if (i < 0 || (size_t)i >= clair_gen.n || k < 0 || k > 5) return 0;
    return clair_gen.v[6 * i + k];
}

/* ---- V0.69: s.save(path) and nbody.load(path): a simulation in a text file ----
 * One "key value" per line, then one line per body: name <tab> m x y z vx vy vz r color.  The numbers
 * are written with 17 digits: read back, they are the same doubles, to the last bit.  A run that goes on
 * from a file gives the same numbers as a run that did not stop (leapfrog, Yoshida, WHFast; IAS15 starts
 * its predictions again, which changes the last digits).
 */

static const char *clair_nbody_name(size_t i);

static int64_t clair_nbody_save(const char *path, const char *integrator, const char *tunit, const char *lunit, const char *gravity) {
    if (!clair_nb.m) clair_nbody_die("no simulation");
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Clair runtime error: nbody: s.save(): cannot write %s\n", path); exit(EXIT_FAILURE); }
    size_t n = clair_nbody_n();
    const double *x = CLAIR_NB(x), *y = CLAIR_NB(y), *z = CLAIR_NB(z), *vx = CLAIR_NB(vx), *vy = CLAIR_NB(vy), *vz = CLAIR_NB(vz);
    const double *m = CLAIR_NB(m), *r = CLAIR_NB(r);
    fprintf(f, "# Clair nbody 1\n");
    fprintf(f, "G %.17g\nt %.17g\ndt %.17g\nsoft %.17g\nepsilon %.17g\n", clair_nb.G, clair_nb.t, clair_nb.dt, clair_nb.soft, clair_nb.epsilon);
    fprintf(f, "integrator %s\nmerge %d\norder %d\n", integrator, clair_nb.flags & 1, (clair_nb.flags & 2) ? 4 : 2);
    fprintf(f, "speed %.17g\ntrail %.17g\n", clair_nb.speed, clair_nb.trail);
    fprintf(f, "time_unit %s\ntime_scale %.17g\nlength_unit %s\nlength_scale %.17g\n", tunit, clair_nb.tscale, lunit, clair_nb.lscale);
    fprintf(f, "gravity %s\ntheta %.17g\nthreads %d\n", gravity, clair_nb.theta_saved, clair_bh.threads);   /* V0.70 */
    fprintf(f, "hill %.17g\n", clair_nb.hill);       /* V0.71 */
    if (clair_nb.c > 0) fprintf(f, "c %.17g\n", clair_nb.c);   /* V0.71.1 */
    fprintf(f, "bodies %zu\n", n);
    int crit = strcmp(integrator, "mercurius") == 0 && clair_mc_crit_ok(n, clair_nb.dt, clair_nb.G);   /* V0.71 */
    for (size_t i = 0; i < n; i++) {
        for (const char *c = clair_nbody_name(i); *c; c++) fputc(*c == '\t' || *c == '\n' || *c == '\r' ? ' ' : *c, f);
        int64_t col = -1;
        if (clair_nb.colors && i < clair_nb.colors->len) col = ((int64_t *)clair_nb.colors->data)[i];
        fprintf(f, "\t%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %lld", m[i], x[i], y[i], z[i], vx[i], vy[i], vz[i], r[i], (long long)col);
        if (crit) fprintf(f, " %.17g", ((const double *)clair_nb.crit->data)[i]);   /* MERCURIUS: its critical distance */
        fprintf(f, "\n");
    }
    if (fclose(f) != 0) { fprintf(stderr, "Clair runtime error: nbody: s.save(): cannot write %s\n", path); exit(EXIT_FAILURE); }
    return (int64_t)n;
}

static struct {
    double p[15];                                    /* G t dt soft epsilon merge order speed trail time_scale length_scale theta threads hill c */
    char text[4][64];                                /* integrator, time unit, length unit, gravity */
    size_t n, cap;
    double *v;                                       /* 10 numbers per body (the 10th: MERCURIUS, or NAN) */
    char **names;
} clair_nl;

static _Noreturn void clair_nbody_load_fail(const char *path, long line, const char *what) {
    fprintf(stderr, "Clair runtime error: nbody: nbody.load(): %s, line %ld: %s\n", path, line, what);
    exit(EXIT_FAILURE);
}

/* reads the file; gives the number of bodies (then __nbody_loaded and __nbody_loaded_text) */
static int64_t clair_nbody_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Clair runtime error: nbody: nbody.load(): cannot read %s\n", path); exit(EXIT_FAILURE); }
    for (size_t i = 0; i < clair_nl.n; i++) free(clair_nl.names[i]);
    clair_nl.n = 0;
    static const char *keys[15] = { "G", "t", "dt", "soft", "epsilon", "merge", "order", "speed", "trail", "time_scale", "length_scale", "theta", "threads", "hill", "c" };
    static const double defaults[15] = { 1, 0, 0, 0, 1e-9, 0, 2, 0, 400, 1, 1, 0.5, 0, 3, 0 };
    memcpy(clair_nl.p, defaults, sizeof(defaults));
    snprintf(clair_nl.text[0], 64, "leapfrog");
    snprintf(clair_nl.text[3], 64, "direct");
    clair_nl.text[1][0] = clair_nl.text[2][0] = '\0';
    char line[4096];
    long ln = 0, want = -1;
    int header = 0;
    while (fgets(line, sizeof(line), f)) {
        ln++;
        size_t len = strlen(line);
        if (len && line[len - 1] != '\n' && !feof(f)) clair_nbody_load_fail(path, ln, "line too long");
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (ln == 1) {
            if (strncmp(line, "# Clair nbody", 13) != 0) clair_nbody_load_fail(path, ln, "not a file of s.save()");
            header = 1;
            continue;
        }
        if (len == 0) continue;
        if (want < 0) {                              /* "key value" */
            char *sp = strchr(line, ' ');
            const char *val = sp ? sp + 1 : "";
            if (sp) *sp = '\0';
            if (strcmp(line, "bodies") == 0) {
                char *end;
                long long k = strtoll(val, &end, 10);
                if (*end || k < 0) clair_nbody_load_fail(path, ln, "bodies: a number");
                want = k;
                if ((size_t)want > clair_nl.cap) {
                    size_t cap = (size_t)want;
                    double *v = realloc(clair_nl.v, cap * 10 * sizeof(double));
                    char **nm = v ? realloc(clair_nl.names, cap * sizeof(char *)) : NULL;
                    if (v) clair_nl.v = v;
                    if (!v || !nm) clair_nbody_die("out of memory");
                    clair_nl.names = nm;
                    clair_nl.cap = cap;
                }
                if (want == 0) break;
                continue;
            }
            int found = 0;
            for (int k = 0; k < 15; k++) {
                if (strcmp(line, keys[k]) != 0) continue;
                char *end;
                clair_nl.p[k] = strtod(val, &end);
                if (end == val || *end) clair_nbody_load_fail(path, ln, "a number is wanted");
                found = 1;
            }
            const char *tkeys[4] = { "integrator", "time_unit", "length_unit", "gravity" };
            for (int k = 0; k < 4; k++) {
                if (strcmp(line, tkeys[k]) != 0) continue;
                snprintf(clair_nl.text[k], 64, "%s", val);
                found = 1;
            }
            if (!found) clair_nbody_load_fail(path, ln, "unknown key");
            continue;
        }
        char *tab = strchr(line, '\t');               /* a body */
        if (!tab) clair_nbody_load_fail(path, ln, "a body: name <tab> m x y z vx vy vz r color");
        *tab = '\0';
        char *p = tab + 1, *end;
        double *v = clair_nl.v + 10 * clair_nl.n;
        for (int k = 0; k < 9; k++) {
            v[k] = strtod(p, &end);
            if (end == p) clair_nbody_load_fail(path, ln, "a body: name <tab> m x y z vx vy vz r color");
            p = end;
        }
        v[9] = strtod(p, &end);                      /* V0.71: the critical distance of MERCURIUS, if there */
        if (end == p) v[9] = NAN;
        p = end;
        while (*p == ' ') p++;
        if (*p) clair_nbody_load_fail(path, ln, "a body: too many numbers");
        char *name = malloc(strlen(line) + 1);
        if (!name) clair_nbody_die("out of memory");
        strcpy(name, line);
        clair_nl.names[clair_nl.n++] = name;
        if ((long)clair_nl.n == want) break;
    }
    fclose(f);
    if (!header) clair_nbody_load_fail(path, 1, "empty file");
    if (want < 0 || (long)clair_nl.n != want) clair_nbody_load_fail(path, ln, "fewer bodies than announced");
    return (int64_t)clair_nl.n;
}

/* body i, number k (0 m, 1-3 x y z, 4-6 vx vy vz, 7 r, 8 color, 9 the critical distance of MERCURIUS or
   NAN); i = -1: the settings */
static double clair_nbody_loaded(int64_t i, int64_t k) {
    if (i < 0) return k >= 0 && k < 15 ? clair_nl.p[k] : 0;
    if ((size_t)i >= clair_nl.n || k < 0 || k > 9) return 0;
    return clair_nl.v[10 * i + k];
}

/* the name of body i; -1 the integrator, -2 the time unit, -3 the length unit, -4 the gravity */
static const char *clair_nbody_loaded_text(int64_t i) {
    const char *s = i >= 0 ? ((size_t)i < clair_nl.n ? clair_nl.names[i] : "") : i >= -4 ? clair_nl.text[-i - 1] : "";
    char *d = clair_rt_arena_alloc(strlen(s) + 1);
    strcpy(d, s);
    return d;
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

static int clair_dbl_less(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static const double *clair_nbody_masses;           /* for qsort: the lightest first */

static int clair_nbody_lighter(const void *a, const void *b) {
    size_t i = *(const size_t *)a, j = *(const size_t *)b;
    double mi = clair_nbody_masses[i], mj = clair_nbody_masses[j];
    return mi < mj ? -1 : mi > mj ? 1 : (i > j) - (i < j);
}

/* V0.70.1: in the window, the physics goes on in a thread of its own, at its own pace; each frame only
   draws the last places it gave.  The window stays smooth (zoom, drag, buttons) even when a step of a
   big cluster takes a tenth of a second.  For the tests (CLAIR_GUI_FRAMES), everything stays in one
   thread, frame by frame, as before (the same numbers every time), unless CLAIR_NBODY_THREAD=1. */
typedef struct {
    pthread_mutex_t lock;
    int quit, paused, slow;
    double speed, h, G, soft, t, e0, e, cx, cy;
    size_t n;
    double *x, *y, *m, *r;                           /* the places given to the window */
} ClairNbPhys;

static double clair_nb_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void clair_nb_nap(long ms) {
    struct timespec ts = { 0, ms * 1000000L };
    nanosleep(&ts, NULL);
}

/* the places of now, for the window (the physics thread owns the lists) */
static void clair_nb_publish(ClairNbPhys *P, double t, double e, double e0, double h, int slow) {
    size_t n = P->n;
    double cx = clair_nbody_sum(6), cy = clair_nbody_sum(7);
    pthread_mutex_lock(&P->lock);
    memcpy(P->x, CLAIR_NB(x), n * sizeof(double));
    memcpy(P->y, CLAIR_NB(y), n * sizeof(double));
    memcpy(P->m, CLAIR_NB(m), n * sizeof(double));
    memcpy(P->r, CLAIR_NB(r), n * sizeof(double));
    P->cx = cx; P->cy = cy; P->t = t; P->e = e; P->e0 = e0; P->h = h; P->slow = slow;
    pthread_mutex_unlock(&P->lock);
}

static void *clair_nb_physics(void *arg) {
    ClairNbPhys *P = arg;
    pthread_mutex_lock(&P->lock);
    double t = P->t, h = P->h, e0 = P->e0, e = P->e, G = P->G, soft = P->soft;
    pthread_mutex_unlock(&P->lock);
    double last = clair_nb_now(), owed = 0, next_energy = last + 0.5;
    int64_t chunk = 1;
    int slow = 0;
    for (;;) {
        pthread_mutex_lock(&P->lock);
        int quit = P->quit, paused = P->paused;
        double speed = P->speed;
        pthread_mutex_unlock(&P->lock);
        if (quit) break;
        double now = clair_nb_now(), dt = now - last;
        last = now;
        if (dt > 0.1) dt = 0.1;
        if (paused) { owed = 0; slow = 0; clair_nb_nap(5); continue; }
        owed += speed * dt;
        double done = 0, start = now;
        int64_t hits = 0;
        if (clair_nb.flags & 4) {                    /* IAS15 chooses its steps: it is asked for a time */
            if (owed >= h) {
                hits = clair_ias_run(owed, G, soft, clair_nb.flags, clair_nb.epsilon, h, 20000000ull);
                h = clair_ias.out[0];
                done = clair_ias.out[1];
            }
        } else {
            int64_t k = (int64_t)(owed / h);
            if (k > chunk) k = chunk;
            if (k > 0) {
                hits = clair_nbody_step(h, k, G, soft, clair_nb.flags);
                done = (double)k * h;
                double spent = clair_nb_now() - start;   /* about 20 ms of work at a time */
                if (spent < 0.005 && chunk < (1 << 20)) chunk *= 2;
                else if (spent > 0.03 && chunk > 1) chunk /= 2;
            }
        }
        owed -= done;
        t += done;
        if (owed > speed * 0.25 + 2 * h) { owed = 0; slow = 1; }     /* too far behind: it cannot keep up */
        else if (done > 0 && owed < h) slow = 0;
        if (hits > 0) e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2);   /* a collision changes the energy */
        now = clair_nb_now();
        if (now >= next_energy) { e = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2); next_energy = now + 0.5; }
        if (done > 0) clair_nb_publish(P, t, e, e0, h, slow);
        else clair_nb_nap(2);
    }
    pthread_mutex_lock(&P->lock);
    P->t = t; P->h = h;
    pthread_mutex_unlock(&P->lock);
    return NULL;
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
    double tau = clair_bh.theta > 0 ? 0 : clair_nbody_tau(G, soft);          /* V0.70: with the tree, too many pairs */
    double speed = clair_nb.speed > 0 ? clair_nb.speed : clair_bh.theta > 0 ? 60 * h : (tau > 0 ? 2 * 3.141592653589793 * tau / 4 : 1);
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
    int paused = 0, trails = T > 0 && n <= 500, slow = 0, fit = 1, quit = 0;   /* V0.70.1: many bodies: no trails at first */
    double acc = 0, e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2), e_shown = e0;
    long frame_no = 0;
    const char *want_thread = getenv("CLAIR_NBODY_THREAD");
    int threaded = want_thread ? want_thread[0] == '1' : !clair_gui.stop_after;
    ClairNbPhys P;
    pthread_t phys_thread;
    double *ux = NULL, *uy = NULL, *um = NULL, *ur = NULL, cx = 0, cy = 0;
    ClairSdlFRect *dots = NULL;
    size_t ndots = 0;
    if (threaded) {
        memset(&P, 0, sizeof(P));
        P.n = n;
        P.x = malloc(n * sizeof(double)); P.y = malloc(n * sizeof(double)); P.m = malloc(n * sizeof(double)); P.r = malloc(n * sizeof(double));
        ux = malloc(n * sizeof(double)); uy = malloc(n * sizeof(double)); um = malloc(n * sizeof(double)); ur = malloc(n * sizeof(double));
        if (!P.x || !P.y || !P.m || !P.r || !ux || !uy || !um || !ur) clair_nbody_die("out of memory");
        pthread_mutex_init(&P.lock, NULL);
        P.speed = speed; P.h = h; P.G = G; P.soft = soft;
        clair_nb_publish(&P, t, e0, e0, h, 0);
        if (pthread_create(&phys_thread, NULL, clair_nb_physics, &P) != 0) threaded = 0;
    }
    dots = malloc((n + 1) * sizeof(ClairSdlFRect));
    if (!dots) clair_nbody_die("out of memory");
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
        n = threaded ? P.n : clair_nbody_n();              /* V0.71: the physics thread owns the lists (MERCURIUS swaps them) */
        if (threaded) {                                    /* the last places the physics gave */
            pthread_mutex_lock(&P.lock);
            memcpy(ux, P.x, n * sizeof(double)); memcpy(uy, P.y, n * sizeof(double));
            memcpy(um, P.m, n * sizeof(double)); memcpy(ur, P.r, n * sizeof(double));
            t = P.t; e_shown = P.e; e0 = P.e0; slow = P.slow; cx = P.cx; cy = P.cy; h = P.h;
            pthread_mutex_unlock(&P.lock);
            x = ux; y = uy; m = um; r = ur;
        } else {
            x = CLAIR_NB(x); y = CLAIR_NB(y); m = CLAIR_NB(m); r = CLAIR_NB(r);
            cx = clair_nbody_sum(6); cy = clair_nbody_sum(7);
        }
        if (fit) {                                         /* everything in the window, around the center of mass */
            double c0 = cx, c1 = cy, far = 0;
            double *ds = n > 300 ? malloc(n * sizeof(double)) : NULL;
            size_t nd = 0;
            for (size_t i = 0; i < n; i++) {
                if (r[i] < 0) continue;
                double d = hypot(x[i] - c0, y[i] - c1);
                if (d > far) far = d;
                if (ds) ds[nd++] = d;
            }
            if (ds && nd > 0) {                            /* V0.70: many bodies: 95 % of them in the window */
                qsort(ds, nd, sizeof(double), clair_dbl_less);
                far = ds[(size_t)(0.95 * (double)(nd - 1))];
            }
            free(ds);
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
        /* the physics: the time of the simulation for this frame, within 14 ms (V0.70.1: or its own thread) */
        if (threaded) {
            pthread_mutex_lock(&P.lock);
            P.paused = paused;
            P.speed = speed;
            pthread_mutex_unlock(&P.lock);
        } else if (!paused && (clair_nb.flags & 4)) {           /* V0.68: IAS15 chooses its steps; the frame asks for a time */
            double frame = clair_gui.dt > 0.1 ? 0.1 : clair_gui.dt;
            slow = 0;
            if (clair_ias_run(speed * frame, G, soft, clair_nb.flags, clair_nb.epsilon, h, 14000000ull) > 0)
                e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2);
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
                    e0 = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2);   /* a collision changes the energy */
                done += k;
                t += (double)k * h;
                uint64_t spent = clair_sdl.GetTicksNS() - start;
                if (spent > 14000000ull && done < want) { slow = 1; acc = 0; break; }
                if (spent < 2000000ull) chunk *= 2;
            }
        }
        if (follow >= 0 && ((size_t)follow >= n || r[follow] < 0)) follow = -1;
        if (follow >= 0) { v.cx = x[follow]; v.cy = y[follow]; }
        else if (follow == -2) { v.cx = cx; v.cy = cy; }
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
            double pr = n > 300 ? 1.2 * ui : mmax > 0 && m[i] > 0 ? ui * (2.5 + 4 * cbrt(m[i] / mmax)) : 2 * ui;
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
        int dot_color = -1;                                 /* V0.70.1: the small ones drawn together, color by color */
        ndots = 0;
        for (size_t q = 0; q < n; q++) {                    /* the bodies: the heaviest on top */
            size_t i = order[q];
            if (r[i] < 0) continue;
            double sx = clair_nbody_sx(&v, x[i]), sy = clair_nbody_sy(&v, y[i]);
            double pr = n > 300 ? 1.2 * ui : mmax > 0 && m[i] > 0 ? ui * (2.5 + 4 * cbrt(m[i] / mmax)) : 2 * ui;
            if (r[i] * v.scale > pr) pr = r[i] * v.scale;
            if (sx < -pr || sx > W + pr || sy < -pr || sy > bar_y) continue;     /* not over the buttons */
            uint8_t cr, cg, cb;
            clair_nbody_rgb(i, &cr, &cg, &cb);
            int col = (cr << 16) | (cg << 8) | cb;
            if (pr < 2.5 && follow != (int)i) {
                if (col != dot_color && ndots) {
                    clair_gui_set_color((dot_color >> 16) & 255, (dot_color >> 8) & 255, dot_color & 255, 255);
                    clair_sdl.RenderFillRects(clair_gui.renderer, dots, (int)ndots);
                    ndots = 0;
                }
                dot_color = col;
                float side = pr < 1.5 ? 2.0f : (float)(2 * pr);
                dots[ndots++] = (ClairSdlFRect){ (float)sx - side / 2, (float)sy - side / 2, side, side };
                continue;
            }
            if (follow == (int)i) {
                clair_gui_set_color(255, 255, 255, 200);
                clair_gui_circle(sx, sy, pr + 4 * ui, false);
            }
            clair_gui_set_color(cr, cg, cb, 255);
            if (pr < 2) clair_gui_rect(sx - 1, sy - 1, 2, 2, true);
            else clair_gui_circle(sx, sy, pr, true);
        }
        if (ndots) {
            clair_gui_set_color((dot_color >> 16) & 255, (dot_color >> 8) & 255, dot_color & 255, 255);
            clair_sdl.RenderFillRects(clair_gui.renderer, dots, (int)ndots);
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
        if (!threaded && (n <= 2000 || frame_no % 30 == 0)) e_shown = clair_nbody_energy(G, soft, 0) + clair_nbody_energy(G, soft, 2);
        frame_no++;
        double e = e_shown;
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
        if (threaded && clair_gui.stop_after) clair_nb_nap(16);      /* the tests: frames at the pace of a screen */
    }
    if (threaded) {                                         /* the physics stops, and gives its time and step */
        pthread_mutex_lock(&P.lock);
        P.quit = 1;
        pthread_mutex_unlock(&P.lock);
        pthread_join(phys_thread, NULL);
        t = P.t;
        h = P.h;
        pthread_mutex_destroy(&P.lock);
        free(P.x); free(P.y); free(P.m); free(P.r); free(ux); free(uy); free(um); free(ur);
    }
    free(dots);
    free(tx); free(ty); free(head); free(filled); free(px); free(py); free(labels); free(order);
    if (opened_here) clair_gui_close();
    clair_ias.out[0] = h;                                   /* V0.68: the step IAS15 got to */
    return t;
}
