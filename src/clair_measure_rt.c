/*
 * V0.53: measures, 9.81 ± 0.02 (see clair_measure.h).
 *
 * Each operation gives the new value and moves every source by the partial
 * derivative of the operation: a * b gives b.v * (sources of a) + a.v * (sources
 * of b), sqrt(a) gives (sources of a) / (2 sqrt(a.v)).  A source in both a and b
 * is added once, which keeps the correlations: x - x = 0 ± 0, x / x = 1 ± 0.
 */

/* sources written with ± (each time the ± is computed); 1 to 6: those behind the constants of phys */
static uint64_t clair_m_next = (uint64_t)1 << 20;

/*
 * The constants of phys that are not exact (CODATA 2022) come from six independent
 * sources, with their relative standard uncertainty:
 *   1 the fine-structure constant alpha, 2 the Rydberg constant, 3 4 5 the relative
 *   atomic masses of the electron, the proton and the neutron, 6 the constant G.
 * mu0 ~ alpha, eps0 ~ 1 / alpha, a0 ~ alpha / R_inf, m_e ~ R_inf / alpha^2, m_u = m_e / A_r(e),
 * m_p = A_r(p) m_u, m_n = A_r(n) m_u, l_P ~ G^1/2, m_P ~ G^-1/2 ...: these exact relations
 * keep eps0 * mu0 * c^2 = 1 exactly, and each constant gets back its published
 * uncertainty (2 digits) with these values.
 */
static const double clair_m_base[7] = {
    0.0, 1.555e-10, 1.0935204606991638e-12, 1.7682018316897974e-11,
    8.24004161259719e-12, 4.0102515073096727e-10, 2.2249e-05
};

static ClairMeasure clair_m_exact(double v) {
    ClairMeasure m = { .v = v };
    return m;
}

static ClairMeasure clair_m_new(double v, double u, size_t line) {
    if (!(u >= 0.0) || u == INFINITY) {
        fprintf(stderr, "Clair runtime error: the uncertainty after ± must be a number, 0 or more, got %g (line %zu)\n",
                u, line);
        exit(EXIT_FAILURE);
    }
    ClairMeasure m = { .v = v };
    if (u > 0.0) {
        m.n = 1;
        m.id[0] = clair_m_next++;
        m.c[0] = u;
    }
    return m;
}

/* a constant of phys: value ~ source^exponent for each of its sources (in increasing order) */
static ClairMeasure clair_m_phys(double v, int count, const int *source, const double *exponent) {
    ClairMeasure m = { .v = v };
    for (int i = 0; i < count && i < CLAIR_MEASURE_K; i++) {
        m.id[m.n] = (uint64_t)source[i];
        m.c[m.n++] = v * exponent[i] * clair_m_base[source[i]];
    }
    return m;
}

/* keeps the sources that move the value; more than K: the two smallest become one new source */
static void clair_m_pack(ClairMeasure *r, uint64_t *id, double *c, int n) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (c[i] == 0.0) continue;
        id[k] = id[i];
        c[k++] = c[i];
    }
    n = k;
    while (n > CLAIR_MEASURE_K) {
        int a = 0, b = 1;
        if (fabs(c[b]) < fabs(c[a])) { a = 1; b = 0; }
        for (int i = 2; i < n; i++) {
            if (fabs(c[i]) < fabs(c[a])) { b = a; a = i; }
            else if (fabs(c[i]) < fabs(c[b])) b = i;
        }
        double merged = hypot(c[a], c[b]);
        int lo = a < b ? a : b, hi = a < b ? b : a;
        for (int i = hi; i + 1 < n; i++) { id[i] = id[i + 1]; c[i] = c[i + 1]; }
        n--;
        for (int i = lo; i + 1 < n; i++) { id[i] = id[i + 1]; c[i] = c[i + 1]; }
        n--;
        id[n] = clair_m_next++;                 /* the newest source: the order stays increasing */
        c[n++] = merged;
    }
    r->n = n;
    for (int i = 0; i < n; i++) { r->id[i] = id[i]; r->c[i] = c[i]; }
}

/* the result v moved by da * (sources of a) + db * (sources of b) */
static ClairMeasure clair_m_mix(double v, ClairMeasure a, double da, ClairMeasure b, double db) {
    uint64_t id[2 * CLAIR_MEASURE_K];
    double c[2 * CLAIR_MEASURE_K];
    int n = 0, i = 0, j = 0;
    while (i < a.n || j < b.n) {
        if (j >= b.n || (i < a.n && a.id[i] < b.id[j])) { id[n] = a.id[i]; c[n++] = da * a.c[i++]; }
        else if (i >= a.n || b.id[j] < a.id[i]) { id[n] = b.id[j]; c[n++] = db * b.c[j++]; }
        else { id[n] = a.id[i]; c[n++] = da * a.c[i++] + db * b.c[j++]; }
    }
    ClairMeasure r = { .v = v };
    clair_m_pack(&r, id, c, n);
    return r;
}

/* f(a) with f'(a.v) = d */
static ClairMeasure clair_m_scale(double v, ClairMeasure a, double d) {
    ClairMeasure r = { .v = v };
    uint64_t id[CLAIR_MEASURE_K];
    double c[CLAIR_MEASURE_K];
    for (int i = 0; i < a.n; i++) { id[i] = a.id[i]; c[i] = d * a.c[i]; }
    clair_m_pack(&r, id, c, a.n);
    return r;
}

static double clair_m_value(ClairMeasure a) { return a.v; }

/* sqrt of the sum of the squares, without overflow or underflow */
static double clair_m_uncertainty(ClairMeasure a) {
    double big = 0.0;
    for (int i = 0; i < a.n; i++) {
        double x = fabs(a.c[i]);
        if (x != x) return x;
        if (x > big) big = x;
    }
    if (big == 0.0 || big == INFINITY) return big;
    double s = 0.0;
    for (int i = 0; i < a.n; i++) { double r = a.c[i] / big; s += r * r; }
    return big * sqrt(s);
}

static ClairMeasure clair_m_add(ClairMeasure a, ClairMeasure b) { return clair_m_mix(a.v + b.v, a, 1.0, b, 1.0); }
static ClairMeasure clair_m_sub(ClairMeasure a, ClairMeasure b) { return clair_m_mix(a.v - b.v, a, 1.0, b, -1.0); }
static ClairMeasure clair_m_mul(ClairMeasure a, ClairMeasure b) { return clair_m_mix(a.v * b.v, a, b.v, b, a.v); }
static ClairMeasure clair_m_div(ClairMeasure a, ClairMeasure b) {
    return clair_m_mix(a.v / b.v, a, 1.0 / b.v, b, -a.v / (b.v * b.v));
}
static ClairMeasure clair_m_neg(ClairMeasure a) { return clair_m_scale(-a.v, a, -1.0); }

static ClairMeasure clair_m_pow(ClairMeasure a, ClairMeasure b) {
    double v = pow(a.v, b.v);
    double da = a.n ? b.v * pow(a.v, b.v - 1.0) : 0.0;
    double db = b.n ? v * log(a.v) : 0.0;
    return clair_m_mix(v, a, da, b, db);
}
static ClairMeasure clair_m_atan2(ClairMeasure y, ClairMeasure x) {
    double d = x.v * x.v + y.v * y.v;
    return clair_m_mix(atan2(y.v, x.v), y, x.v / d, x, -y.v / d);
}
static ClairMeasure clair_m_hypot(ClairMeasure a, ClairMeasure b) {
    double h = hypot(a.v, b.v);
    return clair_m_mix(h, a, a.v / h, b, b.v / h);
}

static ClairMeasure clair_m_sqrt(ClairMeasure a) { double r = sqrt(a.v); return clair_m_scale(r, a, 0.5 / r); }
static ClairMeasure clair_m_cbrt(ClairMeasure a) { double r = cbrt(a.v); return clair_m_scale(r, a, 1.0 / (3.0 * r * r)); }
static ClairMeasure clair_m_exp(ClairMeasure a) { double r = exp(a.v); return clair_m_scale(r, a, r); }
static ClairMeasure clair_m_exp2(ClairMeasure a) { double r = exp2(a.v); return clair_m_scale(r, a, r * 0.6931471805599453); }
static ClairMeasure clair_m_expm1(ClairMeasure a) { return clair_m_scale(expm1(a.v), a, exp(a.v)); }
static ClairMeasure clair_m_log(ClairMeasure a) { return clair_m_scale(log(a.v), a, 1.0 / a.v); }
static ClairMeasure clair_m_log10(ClairMeasure a) { return clair_m_scale(log10(a.v), a, 1.0 / (a.v * 2.302585092994046)); }
static ClairMeasure clair_m_log2(ClairMeasure a) { return clair_m_scale(log2(a.v), a, 1.0 / (a.v * 0.6931471805599453)); }
static ClairMeasure clair_m_log1p(ClairMeasure a) { return clair_m_scale(log1p(a.v), a, 1.0 / (1.0 + a.v)); }
static ClairMeasure clair_m_sin(ClairMeasure a) { return clair_m_scale(sin(a.v), a, cos(a.v)); }
static ClairMeasure clair_m_cos(ClairMeasure a) { return clair_m_scale(cos(a.v), a, -sin(a.v)); }
static ClairMeasure clair_m_tan(ClairMeasure a) { double t = tan(a.v); return clair_m_scale(t, a, 1.0 + t * t); }
static ClairMeasure clair_m_asin(ClairMeasure a) { return clair_m_scale(asin(a.v), a, 1.0 / sqrt(1.0 - a.v * a.v)); }
static ClairMeasure clair_m_acos(ClairMeasure a) { return clair_m_scale(acos(a.v), a, -1.0 / sqrt(1.0 - a.v * a.v)); }
static ClairMeasure clair_m_atan(ClairMeasure a) { return clair_m_scale(atan(a.v), a, 1.0 / (1.0 + a.v * a.v)); }
static ClairMeasure clair_m_sinh(ClairMeasure a) { return clair_m_scale(sinh(a.v), a, cosh(a.v)); }
static ClairMeasure clair_m_cosh(ClairMeasure a) { return clair_m_scale(cosh(a.v), a, sinh(a.v)); }
static ClairMeasure clair_m_tanh(ClairMeasure a) { double t = tanh(a.v); return clair_m_scale(t, a, 1.0 - t * t); }
static ClairMeasure clair_m_asinh(ClairMeasure a) { return clair_m_scale(asinh(a.v), a, 1.0 / sqrt(a.v * a.v + 1.0)); }
static ClairMeasure clair_m_acosh(ClairMeasure a) { return clair_m_scale(acosh(a.v), a, 1.0 / sqrt(a.v * a.v - 1.0)); }
static ClairMeasure clair_m_atanh(ClairMeasure a) { return clair_m_scale(atanh(a.v), a, 1.0 / (1.0 - a.v * a.v)); }
static ClairMeasure clair_m_abs(ClairMeasure a) { return clair_m_scale(fabs(a.v), a, a.v < 0.0 ? -1.0 : 1.0); }
static ClairMeasure clair_m_degrees(ClairMeasure a) {
    return clair_m_scale(a.v * 57.29577951308232, a, 57.29577951308232);
}
static ClairMeasure clair_m_radians(ClairMeasure a) {
    return clair_m_scale(a.v * 0.017453292519943295, a, 0.017453292519943295);
}

/*
 * The text of a measure.  spec (from format(m, ".3f")) writes both numbers with it;
 * otherwise the uncertainty gets 2 significant digits and the value stops at the
 * same place: 686.980 ± 0.031, 12350 ± 680, (6.67430 ± 0.00015)e-11.
 */
static void clair_m_text(ClairMeasure m, const char *spec, char *out, size_t size) {
    double v = m.v, u = clair_m_uncertainty(m);
    if (spec) {
        char a[96], b[96];
        snprintf(a, sizeof(a), spec, v);
        snprintf(b, sizeof(b), spec, u);
        snprintf(out, size, "%s ± %s", a, b);
        return;
    }
    if (!isfinite(v) || !isfinite(u)) { snprintf(out, size, "%.15g ± %.15g", v, u); return; }
    if (u == 0.0) { snprintf(out, size, "%.15g ± 0", v); return; }
    if (u < fabs(v) * 1e-15) { snprintf(out, size, "%.15g ± %.2g", v, u); return; }
    int eu = (int)floor(log10(u));
    if (round(u / pow(10.0, eu - 1)) >= 100.0) eu++;       /* 0.0996 is 0.10 */
    int place = eu - 1;                                     /* the last digit kept is 10^place */
    if (fabs(v) < 0.5 * pow(10.0, place)) v = 0.0;          /* rounded to 0: 0, not -0 */
    int ev = v != 0.0 ? (int)floor(log10(fabs(v))) : eu;
    if (ev >= 6 || ev < -3) {
        int e = ev > eu ? ev : eu;
        double k = pow(10.0, e);
        int d = e - place > 0 ? e - place : 0;
        snprintf(out, size, "(%.*f ± %.*f)e%+03d", d, v / k, d, u / k, e);
        return;
    }
    if (place > 0) {
        double k = pow(10.0, place);
        snprintf(out, size, "%.0f ± %.0f", round(v / k) * k, round(u / k) * k);
        return;
    }
    snprintf(out, size, "%.*f ± %.*f", -place, v, -place, u);
}

static void clair_m_print(ClairMeasure m) {
    char text[200];
    clair_m_text(m, NULL, text, sizeof(text));
    puts(text);
}
