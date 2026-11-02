/*
 * V0.61: the module plot (import plot): curves, points, bars, histograms, shapes,
 * the map of a field on a grid, its contour lines, wind arrows and animations,
 * written as SVG (a picture made of lines and text: sharp at any zoom, read by
 * every browser, no library needed).
 *
 * Copied into the C of the programs that import plot (tools/embed_file.cmake),
 * after the runtime of lists.  Its #include lines are written by the compiler.
 *
 * The figure being drawn lives here (Clair has no global variables).  plot.show()
 * writes it: in Clair Studio (CLAIR_PLOT_DIR is set) into that folder, with a line
 * "ESC[clair:plot NAME]" on the standard output that the Studio replaces by the
 * picture; elsewhere into plot_1.svg, plot_2.svg... next to the program.
 */

enum { CLAIR_PLOT_LINE, CLAIR_PLOT_POINTS, CLAIR_PLOT_POLYGON, CLAIR_PLOT_BARS, CLAIR_PLOT_HIST, CLAIR_PLOT_SEGMENT,
       CLAIR_PLOT_ARROW, CLAIR_PLOT_CIRCLE, CLAIR_PLOT_TEXT, CLAIR_PLOT_MAP, CLAIR_PLOT_CONTOUR, CLAIR_PLOT_ARROWS };

typedef struct {
    int kind;
    double *x, *y;               /* the points; a map or contour: the values (n = nx * ny); arrows: u and v */
    size_t n, nx, ny;
    int levels;
    double a, b, c, d;           /* segment, arrow: from (a, b) to (c, d); circle: center (a, b), radius c */
    double x0, x1, y0, y1;       /* where a field lies */
    char *label, *text, **names; /* its name in the legend, a text, the names of bars */
    char color[32], fill[32];
    double width;
    int colormap;
} ClairPlotItem;

static struct {
    int w, h;
    char *title, *xlabel, *ylabel;
    int fixed_x, fixed_y, equal, hide_axes, colormap, has_extent;
    double xmin, xmax, ymin, ymax, ex0, ex1, ey0, ey1;
    ClairPlotItem *items;
    size_t count, cap;
    char **frames;                /* an animation: each frame drawn */
    size_t frame_count, frame_cap;
    int frame_w, frame_h;
    int shown, animations, palette;
} clair_plot = { .w = 640, .h = 400 };

/* the ticks from lo to hi: multiples of step, at most 200 */
#define CLAIR_PLOT_TICKS(var, lo, hi, step) \
    for (double var##_k = ceil((lo) / (step)), var##_end = var##_k + 200, var = var##_k * (step); \
         var##_k < var##_end && var <= (hi) + (step) * 1e-9; var##_k += 1, var = var##_k * (step))

_Noreturn static void clair_plot_fail(const char *what) {
    fflush(stdout);
    fprintf(stderr, "Clair runtime error: %s\n", what);
    exit(EXIT_FAILURE);
}

static void *clair_plot_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) clair_plot_fail("plot: out of memory");
    return p;
}

static char *clair_plot_dup(const char *s) {
    char *d = clair_plot_alloc(strlen(s) + 1);
    strcpy(d, s);
    return d;
}

/* ---- a growing text ---- */

typedef struct { char *p; size_t len, cap; } ClairPlotBuf;

static void clair_plot_put(ClairPlotBuf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (b->len + n + 1 > cap) cap *= 2;
        char *p = realloc(b->p, cap);
        if (!p) clair_plot_fail("plot: out of memory");
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void clair_plot_str(ClairPlotBuf *b, const char *s) { clair_plot_put(b, s, strlen(s)); }

static void clair_plot_printf(ClairPlotBuf *b, const char *fmt, ...) {
    char small[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(small, sizeof(small), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof(small)) { clair_plot_put(b, small, (size_t)n); return; }
    char *big = clair_plot_alloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    clair_plot_put(b, big, (size_t)n);
    free(big);
}

/* a text inside the SVG: & < > " written so they stay text */
static void clair_plot_xml(ClairPlotBuf *b, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&': clair_plot_str(b, "&amp;"); break;
            case '<': clair_plot_str(b, "&lt;"); break;
            case '>': clair_plot_str(b, "&gt;"); break;
            case '"': clair_plot_str(b, "&quot;"); break;
            default:
                if ((unsigned char)*s >= 0x20 || *s == '\t') clair_plot_put(b, s, 1);
        }
    }
}

/* ---- colors ---- */

static const char *const clair_plot_palette[] = {
    "#2563eb", "#dc2626", "#16a34a", "#ea580c", "#7c3aed", "#0891b2", "#db2777", "#65a30d", "#475569", "#b45309"
};

static void clair_plot_color_of(const char *name, char out[32], const char *who) {
    static const char *const names[][2] = {
        { "rouge", "#dc2626" }, { "red", "#dc2626" }, { "bleu", "#2563eb" }, { "blue", "#2563eb" },
        { "vert", "#16a34a" }, { "green", "#16a34a" }, { "orange", "#ea580c" }, { "violet", "#7c3aed" },
        { "purple", "#7c3aed" }, { "jaune", "#eab308" }, { "yellow", "#eab308" }, { "noir", "#111827" },
        { "black", "#111827" }, { "blanc", "#ffffff" }, { "white", "#ffffff" }, { "gris", "#6b7280" },
        { "gray", "#6b7280" }, { "grey", "#6b7280" }, { "rose", "#db2777" }, { "pink", "#db2777" },
        { "cyan", "#0891b2" }, { "marron", "#92400e" }, { "brown", "#92400e" }, { "aucune", "none" }, { "none", "none" },
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (strcmp(name, names[i][0]) == 0) { strcpy(out, names[i][1]); return; }
    }
    size_t n = strlen(name);
    int ok = n > 0 && n < 32;
    for (size_t i = 0; ok && i < n; i++) ok = isalnum((unsigned char)name[i]) || (i == 0 && name[i] == '#');
    if (!ok) {
        char msg[200];
        snprintf(msg, sizeof(msg), "%s: unknown color \"%.40s\" (rouge, bleu, vert, orange, violet, jaune, noir, gris... or \"#1d4ed8\")", who, name);
        clair_plot_fail(msg);
    }
    strcpy(out, name);
}

/* the colormaps of a field: from the lowest value to the highest */
static const char *const clair_plot_maps[][11] = {
    { "#440154", "#482475", "#414487", "#355f8d", "#2a788e", "#21918c", "#22a884", "#44bf70", "#7ad151", "#bddf26", "#fde725" },
    { "#000004", "#160b39", "#420a68", "#6a176e", "#932667", "#bc3754", "#dd513a", "#f37819", "#fca50a", "#f6d746", "#fcffa4" },
    { "#053061", "#2166ac", "#4393c3", "#92c5de", "#d1e5f0", "#f7f7f7", "#fddbc7", "#f4a582", "#d6604d", "#b2182b", "#67001f" },
};

static void clair_plot_rgb(int map, double t, unsigned char rgb[3]) {
    if (!(t >= 0)) t = 0;              /* nan too */
    if (t > 1) t = 1;
    double pos = t * 10;
    int k = (int)pos;
    if (k > 9) k = 9;
    double f = pos - k;
    unsigned int c0 = (unsigned int)strtoul(clair_plot_maps[map][k] + 1, NULL, 16);
    unsigned int c1 = (unsigned int)strtoul(clair_plot_maps[map][k + 1] + 1, NULL, 16);
    for (int i = 0; i < 3; i++) {
        double a = (c0 >> (16 - 8 * i)) & 255, b = (c1 >> (16 - 8 * i)) & 255;
        rgb[i] = (unsigned char)(a + (b - a) * f + 0.5);
    }
}

/* ---- a PNG in the SVG (the map of a field): stored, not compressed: always right, and simple ---- */

static uint32_t clair_plot_crc_table[256];

static uint32_t clair_plot_crc(uint32_t c, const unsigned char *p, size_t n) {
    if (!clair_plot_crc_table[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            clair_plot_crc_table[i] = v;
        }
    }
    c ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = clair_plot_crc_table[(c ^ p[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void clair_plot_be32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
}

static void clair_plot_chunk(ClairPlotBuf *png, const char *type, const unsigned char *data, size_t n) {
    unsigned char head[8];
    clair_plot_be32(head, (uint32_t)n);
    memcpy(head + 4, type, 4);
    clair_plot_put(png, (const char *)head, 8);
    if (n) clair_plot_put(png, (const char *)data, n);
    uint32_t c = clair_plot_crc(0, (const unsigned char *)type, 4);
    c = clair_plot_crc(c, data, n);
    unsigned char tail[4];
    clair_plot_be32(tail, c);
    clair_plot_put(png, (const char *)tail, 4);
}

/* rgb: w * h pixels, the top row first; appends the PNG in base64 */
static void clair_plot_png_base64(ClairPlotBuf *out, const unsigned char *rgb, size_t w, size_t h) {
    size_t row = w * 3 + 1, raw_len = row * h;
    unsigned char *raw = clair_plot_alloc(raw_len);
    for (size_t y = 0; y < h; y++) memcpy(raw + y * row + 1, rgb + y * w * 3, w * 3);
    ClairPlotBuf z = { 0 }, png = { 0 };
    clair_plot_put(&z, "\x78\x01", 2);
    uint32_t s1 = 1, s2 = 0;
    for (size_t i = 0; i < raw_len; i++) { s1 = (s1 + raw[i]) % 65521; s2 = (s2 + s1) % 65521; }
    for (size_t at = 0; at < raw_len || raw_len == 0; ) {
        size_t n = raw_len - at > 65535 ? 65535 : raw_len - at;
        unsigned char head[5] = { (unsigned char)(at + n == raw_len), (unsigned char)n, (unsigned char)(n >> 8),
                                  (unsigned char)~n, (unsigned char)(~n >> 8) };
        clair_plot_put(&z, (const char *)head, 5);
        if (n) clair_plot_put(&z, (const char *)raw + at, n);
        at += n;
        if (raw_len == 0) break;
    }
    unsigned char adler[4];
    clair_plot_be32(adler, (s2 << 16) | s1);
    clair_plot_put(&z, (const char *)adler, 4);
    clair_plot_put(&png, "\x89PNG\r\n\x1a\n", 8);
    unsigned char ihdr[13] = { 0 };
    clair_plot_be32(ihdr, (uint32_t)w);
    clair_plot_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 2;                       /* 8 bits, red green blue */
    clair_plot_chunk(&png, "IHDR", ihdr, 13);
    clair_plot_chunk(&png, "IDAT", (const unsigned char *)z.p, z.len);
    clair_plot_chunk(&png, "IEND", NULL, 0);
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *p = (const unsigned char *)png.p;
    for (size_t i = 0; i < png.len; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < png.len ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < png.len ? p[i + 2] : 0);
        char q[4] = { b64[v >> 18], b64[(v >> 12) & 63], i + 1 < png.len ? b64[(v >> 6) & 63] : '=', i + 2 < png.len ? b64[v & 63] : '=' };
        clair_plot_put(out, q, 4);
    }
    free(raw);
    free(z.p);
    free(png.p);
}

/* ---- what the module asks ---- */

static ClairPlotItem *clair_plot_add(int kind) {
    if (clair_plot.count == clair_plot.cap) {
        size_t cap = clair_plot.cap ? clair_plot.cap * 2 : 16;
        ClairPlotItem *p = realloc(clair_plot.items, cap * sizeof(ClairPlotItem));
        if (!p) clair_plot_fail("plot: out of memory");
        clair_plot.items = p;
        clair_plot.cap = cap;
    }
    ClairPlotItem *it = &clair_plot.items[clair_plot.count++];
    memset(it, 0, sizeof(*it));
    it->kind = kind;
    it->width = 2;
    it->colormap = clair_plot.colormap;
    strcpy(it->fill, "none");
    if (kind <= CLAIR_PLOT_HIST) {     /* the data take the colors one after the other */
        strcpy(it->color, clair_plot_palette[clair_plot.palette++ % 10]);
        if (kind == CLAIR_PLOT_BARS || kind == CLAIR_PLOT_HIST) strcpy(it->fill, it->color);
    }
    if (clair_plot.has_extent) {
        it->x0 = clair_plot.ex0; it->x1 = clair_plot.ex1; it->y0 = clair_plot.ey0; it->y1 = clair_plot.ey1;
    }
    return it;
}

static double *clair_plot_copy(ClairList l) {
    double *d = clair_plot_alloc((l ? l->len : 0) * sizeof(double));
    if (l && l->len) memcpy(d, l->data, l->len * sizeof(double));
    return d;
}

static void clair_plot_free_items(void) {
    for (size_t i = 0; i < clair_plot.count; i++) {
        ClairPlotItem *it = &clair_plot.items[i];
        free(it->x); free(it->y); free(it->label); free(it->text);
        if (it->names) { for (size_t k = 0; k < it->n; k++) free(it->names[k]); free(it->names); }
    }
    clair_plot.count = 0;
    clair_plot.palette = 0;
}

/* after show(): a new figure, as at the start */
static void clair_plot_reset(void) {
    clair_plot_free_items();
    free(clair_plot.title); free(clair_plot.xlabel); free(clair_plot.ylabel);
    clair_plot.title = clair_plot.xlabel = clair_plot.ylabel = NULL;
    clair_plot.w = 640; clair_plot.h = 400;
    clair_plot.fixed_x = clair_plot.fixed_y = clair_plot.equal = clair_plot.hide_axes = 0;
    clair_plot.colormap = clair_plot.has_extent = 0;
}

static int64_t clair_plot_size(int64_t w, int64_t h) {
    if (w < 100 || h < 80 || w > 4000 || h > 4000) clair_plot_fail("plot.size(): from 100 x 80 to 4000 x 4000 pixels");
    clair_plot.w = (int)w;
    clair_plot.h = (int)h;
    return 0;
}

static ClairPlotItem *clair_plot_last(const char *who) {
    if (!clair_plot.count) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s: there is nothing to change yet: write it after plot.line(...), plot.points(...)...", who);
        clair_plot_fail(msg);
    }
    return &clair_plot.items[clair_plot.count - 1];
}

/* 0 title, 1 xlabel, 2 ylabel, 3 name, 4 color, 5 fill (of the last thing), 6 colormap */
static int64_t clair_plot_set(int64_t what, const char *text) {
    switch (what) {
        case 0: free(clair_plot.title); clair_plot.title = clair_plot_dup(text); break;
        case 1: free(clair_plot.xlabel); clair_plot.xlabel = clair_plot_dup(text); break;
        case 2: free(clair_plot.ylabel); clair_plot.ylabel = clair_plot_dup(text); break;
        case 3: { ClairPlotItem *it = clair_plot_last("plot.name()"); free(it->label); it->label = clair_plot_dup(text); break; }
        case 4: {
            ClairPlotItem *it = clair_plot_last("plot.color()");
            clair_plot_color_of(text, it->color, "plot.color()");
            if (it->kind == CLAIR_PLOT_BARS || it->kind == CLAIR_PLOT_HIST) strcpy(it->fill, it->color);
            break;
        }
        case 5: clair_plot_color_of(text, clair_plot_last("plot.fill()")->fill, "plot.fill()"); break;
        default: {
            static const char *const names[][2] = { { "viridis", "0" }, { "chaleur", "1" }, { "inferno", "1" },
                                                    { "bleu-rouge", "2" }, { "coolwarm", "2" } };
            int found = -1;
            for (size_t i = 0; i < 5; i++) if (strcmp(text, names[i][0]) == 0) found = names[i][1][0] - '0';
            if (found < 0) clair_plot_fail("plot.colormap(): \"viridis\", \"chaleur\" or \"bleu-rouge\"");
            clair_plot.colormap = found;
        }
    }
    return 0;
}

static int64_t clair_plot_range(int64_t axis, double low, double high) {
    if (!(low < high) || !isfinite(low) || !isfinite(high)) clair_plot_fail("plot.xrange() and plot.yrange() need low < high");
    if (axis == 0) { clair_plot.fixed_x = 1; clair_plot.xmin = low; clair_plot.xmax = high; }
    else { clair_plot.fixed_y = 1; clair_plot.ymin = low; clair_plot.ymax = high; }
    return 0;
}

/* 0 equal (the same scale on both axes), 1 hide the axes */
static int64_t clair_plot_flag(int64_t what, int64_t value) {
    if (what == 0) clair_plot.equal = value != 0;
    else clair_plot.hide_axes = value != 0;
    return 0;
}

static int64_t clair_plot_width(double w) {
    if (!(w > 0 && w <= 50)) clair_plot_fail("plot.width(): a width from 0 to 50 pixels");
    clair_plot_last("plot.width()")->width = w;
    return 0;
}

static void clair_plot_same_length(ClairList xs, ClairList ys, const char *who) {
    size_t a = xs ? xs->len : 0, b = ys ? ys->len : 0;
    if (a != b) {
        char msg[200];
        snprintf(msg, sizeof(msg), "%s: the two lists have %zu and %zu values, they need as many", who, a, b);
        clair_plot_fail(msg);
    }
}

/* 0 line, 1 points, 2 polygon */
static int64_t clair_plot_series(int64_t kind, ClairList xs, ClairList ys) {
    static const char *const who[] = { "plot.line()", "plot.points()", "plot.polygon()" };
    clair_plot_same_length(xs, ys, who[kind]);
    ClairPlotItem *it = clair_plot_add(kind == 0 ? CLAIR_PLOT_LINE : kind == 1 ? CLAIR_PLOT_POINTS : CLAIR_PLOT_POLYGON);
    it->n = xs ? xs->len : 0;
    it->x = clair_plot_copy(xs);
    it->y = clair_plot_copy(ys);
    return (int64_t)clair_plot.count;
}

static int64_t clair_plot_bars(ClairList names, ClairList values) {
    clair_plot_same_length(names, values, "plot.bars()");
    ClairPlotItem *it = clair_plot_add(CLAIR_PLOT_BARS);
    it->n = values ? values->len : 0;
    it->y = clair_plot_copy(values);
    it->names = clair_plot_alloc((it->n ? it->n : 1) * sizeof(char *));
    for (size_t i = 0; i < it->n; i++) it->names[i] = clair_plot_dup(((const char **)names->data)[i]);
    return (int64_t)clair_plot.count;
}

static int64_t clair_plot_histogram(ClairList values, int64_t bins) {
    if (bins < 1 || bins > 10000) clair_plot_fail("plot.histogram(): from 1 to 10000 bins");
    double lo = INFINITY, hi = -INFINITY;
    size_t n = values ? values->len : 0;
    const double *v = n ? (const double *)values->data : NULL;
    for (size_t i = 0; i < n; i++) if (isfinite(v[i])) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
    if (!(lo <= hi)) { lo = 0; hi = 1; }
    if (lo == hi) { lo -= 0.5; hi += 0.5; }
    ClairPlotItem *it = clair_plot_add(CLAIR_PLOT_HIST);
    it->n = (size_t)bins;
    it->x = clair_plot_alloc((size_t)bins * sizeof(double));
    it->y = clair_plot_alloc((size_t)bins * sizeof(double));
    it->c = (hi - lo) / (double)bins;
    for (int64_t k = 0; k < bins; k++) it->x[k] = lo + (double)k * it->c;
    for (size_t i = 0; i < n; i++) {
        if (!isfinite(v[i])) continue;
        int64_t k = (int64_t)((v[i] - lo) / it->c);
        if (k >= bins) k = bins - 1;
        if (k < 0) k = 0;
        it->y[k] += 1;
    }
    return (int64_t)clair_plot.count;
}

/* 0 segment (x1, y1) -> (x2, y2), 1 arrow (the same with a head), 2 circle (x, y, radius) */
static int64_t clair_plot_shape(int64_t kind, double a, double b, double c, double d) {
    if (kind == 2 && !(c > 0)) clair_plot_fail("plot.circle(): the radius must be > 0");
    ClairPlotItem *it = clair_plot_add(kind == 0 ? CLAIR_PLOT_SEGMENT : kind == 1 ? CLAIR_PLOT_ARROW : CLAIR_PLOT_CIRCLE);
    strcpy(it->color, "currentColor");
    it->a = a; it->b = b; it->c = c; it->d = d;
    it->width = 1.5;
    return (int64_t)clair_plot.count;
}

static int64_t clair_plot_text(double x, double y, const char *t) {
    ClairPlotItem *it = clair_plot_add(CLAIR_PLOT_TEXT);
    strcpy(it->color, "currentColor");
    it->a = x; it->b = y;
    it->text = clair_plot_dup(t);
    return (int64_t)clair_plot.count;
}

static int64_t clair_plot_extent(double x0, double x1, double y0, double y1) {
    if (!(x0 < x1) || !(y0 < y1)) clair_plot_fail("plot.extent(): x0 < x1 and y0 < y1");
    clair_plot.has_extent = 1;
    clair_plot.ex0 = x0; clair_plot.ex1 = x1; clair_plot.ey0 = y0; clair_plot.ey1 = y1;
    return 0;
}

static void clair_plot_grid_size(ClairList values, int64_t nx, int64_t ny, const char *who) {
    char msg[200];
    if (nx < 2 || ny < 2 || nx > 4096 || ny > 4096) {
        snprintf(msg, sizeof(msg), "%s: a grid of 2 to 4096 columns and rows, got %" PRId64 " x %" PRId64, who, nx, ny);
        clair_plot_fail(msg);
    }
    size_t n = values ? values->len : 0;
    if (n != (size_t)nx * (size_t)ny) {
        snprintf(msg, sizeof(msg), "%s: %" PRId64 " x %" PRId64 " = %" PRId64 " values expected (values[j * nx + i]), got %zu",
                 who, nx, ny, nx * ny, n);
        clair_plot_fail(msg);
    }
}

/* 0 a map in colors, 1 contour lines */
static int64_t clair_plot_field(int64_t kind, ClairList values, int64_t nx, int64_t ny, int64_t levels) {
    const char *who = kind == 0 ? "plot.heatmap()" : "plot.contour()";
    clair_plot_grid_size(values, nx, ny, who);
    if (kind == 1 && (levels < 1 || levels > 100)) clair_plot_fail("plot.contour(): from 1 to 100 levels");
    ClairPlotItem *it = clair_plot_add(kind == 0 ? CLAIR_PLOT_MAP : CLAIR_PLOT_CONTOUR);
    if (kind == 1) strcpy(it->color, "currentColor");
    it->nx = (size_t)nx; it->ny = (size_t)ny; it->n = it->nx * it->ny;
    it->x = clair_plot_copy(values);
    it->levels = (int)levels;
    it->width = 1.2;
    if (!clair_plot.has_extent) { it->x0 = 0; it->x1 = (double)nx; it->y0 = 0; it->y1 = (double)ny; }
    return (int64_t)clair_plot.count;
}

static int64_t clair_plot_arrows(ClairList us, ClairList vs, int64_t nx, int64_t ny) {
    clair_plot_grid_size(us, nx, ny, "plot.arrows()");
    clair_plot_grid_size(vs, nx, ny, "plot.arrows()");
    ClairPlotItem *it = clair_plot_add(CLAIR_PLOT_ARROWS);
    strcpy(it->color, "currentColor");
    it->nx = (size_t)nx; it->ny = (size_t)ny; it->n = it->nx * it->ny;
    it->x = clair_plot_copy(us);
    it->y = clair_plot_copy(vs);
    it->width = 1.2;
    if (!clair_plot.has_extent) { it->x0 = 0; it->x1 = (double)nx; it->y0 = 0; it->y1 = (double)ny; }
    return (int64_t)clair_plot.count;
}

/* ---- drawing ---- */

typedef struct {
    double xmin, xmax, ymin, ymax;       /* what the plot area shows */
    double left, top, pw, ph;            /* the plot area in pixels */
    int id;                              /* for the ids of the clip and of the gradients */
} ClairPlotView;

static double clair_plot_px(const ClairPlotView *v, double x) { return v->left + (x - v->xmin) / (v->xmax - v->xmin) * v->pw; }
static double clair_plot_py(const ClairPlotView *v, double y) { return v->top + v->ph - (y - v->ymin) / (v->ymax - v->ymin) * v->ph; }

/* 1, 2, 5, 10... the step between two ticks */
static double clair_plot_step(double range, int target) {
    double raw = range / (target > 0 ? target : 1), p = pow(10, floor(log10(raw))), m = raw / p;
    return (m < 1.5 ? 1 : m < 3 ? 2 : m < 7 ? 5 : 10) * p;
}

static void clair_plot_number(char *out, size_t size, double v, double step) {
    if (fabs(v) < step * 1e-9) v = 0;
    double big = fabs(v);
    if ((big >= 1e6 || step < 1e-4) && v != 0) { snprintf(out, size, "%g", v); return; }
    int decimals = step >= 1 ? 0 : (int)ceil(-log10(step) - 1e-9);
    if (decimals > 8) decimals = 8;
    snprintf(out, size, "%.*f", decimals, v);
    if (strcmp(out, "-0") == 0) strcpy(out, "0");
}

static void clair_plot_extend(double *lo, double *hi, double v) {
    if (!isfinite(v)) return;
    if (v < *lo) *lo = v;
    if (v > *hi) *hi = v;
}

/* the smallest and largest values of a field (its finite values) */
static void clair_plot_field_range(const ClairPlotItem *it, double *lo, double *hi) {
    *lo = INFINITY; *hi = -INFINITY;
    for (size_t i = 0; i < it->n; i++) clair_plot_extend(lo, hi, it->x[i]);
    if (!(*lo <= *hi)) { *lo = 0; *hi = 1; }
    if (*lo == *hi) { *lo -= 0.5; *hi += 0.5; }
}

static void clair_plot_view(ClairPlotView *v, int w, int h) {
    double xmin = INFINITY, xmax = -INFINITY, ymin = INFINITY, ymax = -INFINITY;
    int only_fields = clair_plot.count > 0, zero_y = 0, has_map = 0;
    for (size_t i = 0; i < clair_plot.count; i++) {
        const ClairPlotItem *it = &clair_plot.items[i];
        switch (it->kind) {
            case CLAIR_PLOT_LINE: case CLAIR_PLOT_POINTS: case CLAIR_PLOT_POLYGON:
                for (size_t k = 0; k < it->n; k++) {
                    if (!isfinite(it->x[k]) || !isfinite(it->y[k])) continue;
                    clair_plot_extend(&xmin, &xmax, it->x[k]);
                    clair_plot_extend(&ymin, &ymax, it->y[k]);
                }
                only_fields = 0;
                break;
            case CLAIR_PLOT_BARS:
                clair_plot_extend(&xmin, &xmax, -0.6);
                clair_plot_extend(&xmin, &xmax, (double)it->n - 0.4);
                clair_plot_extend(&ymin, &ymax, 0);
                for (size_t k = 0; k < it->n; k++) clair_plot_extend(&ymin, &ymax, it->y[k]);
                only_fields = 0; zero_y = 1;
                break;
            case CLAIR_PLOT_HIST:
                clair_plot_extend(&xmin, &xmax, it->x[0]);
                clair_plot_extend(&xmin, &xmax, it->x[it->n - 1] + it->c);
                clair_plot_extend(&ymin, &ymax, 0);
                for (size_t k = 0; k < it->n; k++) clair_plot_extend(&ymin, &ymax, it->y[k]);
                only_fields = 0; zero_y = 1;
                break;
            case CLAIR_PLOT_SEGMENT: case CLAIR_PLOT_ARROW:
                clair_plot_extend(&xmin, &xmax, it->a); clair_plot_extend(&xmin, &xmax, it->c);
                clair_plot_extend(&ymin, &ymax, it->b); clair_plot_extend(&ymin, &ymax, it->d);
                only_fields = 0;
                break;
            case CLAIR_PLOT_CIRCLE:
                clair_plot_extend(&xmin, &xmax, it->a - it->c); clair_plot_extend(&xmin, &xmax, it->a + it->c);
                clair_plot_extend(&ymin, &ymax, it->b - it->c); clair_plot_extend(&ymin, &ymax, it->b + it->c);
                only_fields = 0;
                break;
            case CLAIR_PLOT_TEXT:
                clair_plot_extend(&xmin, &xmax, it->a);
                clair_plot_extend(&ymin, &ymax, it->b);
                break;
            default:          /* a field lies on its extent */
                clair_plot_extend(&xmin, &xmax, it->x0); clair_plot_extend(&xmin, &xmax, it->x1);
                clair_plot_extend(&ymin, &ymax, it->y0); clair_plot_extend(&ymin, &ymax, it->y1);
                has_map |= it->kind == CLAIR_PLOT_MAP;
        }
    }
    if (!(xmin <= xmax)) { xmin = 0; xmax = 1; }
    if (!(ymin <= ymax)) { ymin = 0; ymax = 1; }
    if (xmin == xmax) { double d = fabs(xmin) > 0 ? fabs(xmin) * 0.1 : 1; xmin -= d; xmax += d; }
    if (ymin == ymax) { double d = fabs(ymin) > 0 ? fabs(ymin) * 0.1 : 1; ymin -= d; ymax += d; }
    if (!only_fields) {                 /* a little room around the data, as matplotlib */
        double dx = (xmax - xmin) * 0.05, dy = (ymax - ymin) * 0.05;
        xmin -= dx; xmax += dx;
        if (!(zero_y && ymin == 0)) ymin -= dy;
        if (!(zero_y && ymax == 0)) ymax += dy;
    }
    if (clair_plot.fixed_x) { xmin = clair_plot.xmin; xmax = clair_plot.xmax; }
    if (clair_plot.fixed_y) { ymin = clair_plot.ymin; ymax = clair_plot.ymax; }
    /* the room of the texts around the plot area */
    double left = 12, right = 14, top = clair_plot.title ? 36 : 14, bottom = 12;
    if (!clair_plot.hide_axes) {
        char t[64];
        double step = clair_plot_step(ymax - ymin, 5);
        size_t widest = 1;
        CLAIR_PLOT_TICKS(y, ymin, ymax, step) {
            clair_plot_number(t, sizeof(t), y, step);
            if (strlen(t) > widest) widest = strlen(t);
        }
        left = 14 + 7.0 * (double)widest + (clair_plot.ylabel ? 20 : 0);
        bottom = 30 + (clair_plot.xlabel ? 20 : 0);
    }
    if (has_map) right += 70;
    v->left = left; v->top = top;
    v->pw = w - left - right; v->ph = h - top - bottom;
    if (v->pw < 20) v->pw = 20;
    if (v->ph < 20) v->ph = 20;
    if (clair_plot.equal) {             /* one unit is as long on both axes */
        double ux = (xmax - xmin) / v->pw, uy = (ymax - ymin) / v->ph;
        if (ux > uy) { double c = (ymin + ymax) / 2, half = ux * v->ph / 2; ymin = c - half; ymax = c + half; }
        else { double c = (xmin + xmax) / 2, half = uy * v->pw / 2; xmin = c - half; xmax = c + half; }
    }
    v->xmin = xmin; v->xmax = xmax; v->ymin = ymin; v->ymax = ymax;
}

static void clair_plot_arrow_head(ClairPlotBuf *b, double x1, double y1, double x2, double y2, double size) {
    double dx = x2 - x1, dy = y2 - y1, len = sqrt(dx * dx + dy * dy);
    if (len < 1e-9) return;
    double ux = dx / len, uy = dy / len, s = size < len * 0.6 ? size : len * 0.6;
    clair_plot_printf(b, "M%.1f %.1fL%.1f %.1fL%.1f %.1f", x2 - s * ux + s * 0.5 * uy, y2 - s * uy - s * 0.5 * ux, x2, y2,
                      x2 - s * ux - s * 0.5 * uy, y2 - s * uy + s * 0.5 * ux);
}

/* marching squares: the segments where the field crosses level, on the centers of the cells */
static void clair_plot_contour_level(ClairPlotBuf *b, const ClairPlotView *v, const ClairPlotItem *it, double level) {
    double cw = (it->x1 - it->x0) / (double)it->nx, ch = (it->y1 - it->y0) / (double)it->ny;
    for (size_t j = 0; j + 1 < it->ny; j++) {
        for (size_t i = 0; i + 1 < it->nx; i++) {
            double c[4] = { it->x[j * it->nx + i], it->x[j * it->nx + i + 1], it->x[(j + 1) * it->nx + i + 1], it->x[(j + 1) * it->nx + i] };
            if (!isfinite(c[0]) || !isfinite(c[1]) || !isfinite(c[2]) || !isfinite(c[3])) continue;
            /* corners: 0 (i, j), 1 (i+1, j), 2 (i+1, j+1), 3 (i, j+1); edges: 0 bottom, 1 right, 2 top, 3 left */
            int mask = (c[0] > level) | (c[1] > level) << 1 | (c[2] > level) << 2 | (c[3] > level) << 3;
            if (mask == 0 || mask == 15) continue;
            double ex[4], ey[4];
            double xi = it->x0 + ((double)i + 0.5) * cw, yj = it->y0 + ((double)j + 0.5) * ch;
            double t;
            t = (level - c[0]) / (c[1] - c[0]); ex[0] = xi + t * cw; ey[0] = yj;
            t = (level - c[1]) / (c[2] - c[1]); ex[1] = xi + cw; ey[1] = yj + t * ch;
            t = (level - c[3]) / (c[2] - c[3]); ex[2] = xi + t * cw; ey[2] = yj + ch;
            t = (level - c[0]) / (c[3] - c[0]); ex[3] = xi; ey[3] = yj + t * ch;
            static const signed char pairs[16][4] = {
                { -1, -1, -1, -1 }, { 3, 0, -1, -1 }, { 0, 1, -1, -1 }, { 3, 1, -1, -1 },
                { 1, 2, -1, -1 }, { 3, 0, 1, 2 }, { 0, 2, -1, -1 }, { 3, 2, -1, -1 },
                { 2, 3, -1, -1 }, { 0, 2, -1, -1 }, { 0, 1, 2, 3 }, { 1, 2, -1, -1 },
                { 1, 3, -1, -1 }, { 0, 1, -1, -1 }, { 3, 0, -1, -1 }, { -1, -1, -1, -1 }
            };
            int m = mask;
            if (mask == 5 || mask == 10) {          /* a saddle: the center decides */
                double center = (c[0] + c[1] + c[2] + c[3]) / 4;
                if ((center > level) != (mask == 5)) m = mask == 5 ? 10 : 5;
            }
            for (int k = 0; k < 4 && pairs[m][k] >= 0; k += 2) {
                int e1 = pairs[m][k], e2 = pairs[m][k + 1];
                clair_plot_printf(b, "M%.1f %.1fL%.1f %.1f", clair_plot_px(v, ex[e1]), clair_plot_py(v, ey[e1]),
                                  clair_plot_px(v, ex[e2]), clair_plot_py(v, ey[e2]));
            }
        }
    }
}

static int clair_plot_ids = 0;

/* the inside of an <svg> of w x h: background, grid, axes, data, legend */
static void clair_plot_draw(ClairPlotBuf *b, int w, int h) {
    ClairPlotView v;
    clair_plot_view(&v, w, h);
    v.id = (int)(((unsigned)time(NULL) * 2654435761u + (unsigned)getpid() * 40503u) % 100000u) * 100 + (++clair_plot_ids % 100);
    double right = v.left + v.pw, bottom = v.top + v.ph;
    clair_plot_printf(b, "<rect class=\"cp-fond\" x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"/>\n", w, h);
    clair_plot_printf(b, "<defs><clipPath id=\"cp%d\"><rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\"/></clipPath></defs>\n",
                      v.id, v.left, v.top, v.pw, v.ph);
    int bars = 0;
    for (size_t i = 0; i < clair_plot.count; i++) if (clair_plot.items[i].kind == CLAIR_PLOT_BARS) bars = 1;
    char t[64];
    double sx = clair_plot_step(v.xmax - v.xmin, (int)(v.pw / 80) < 2 ? 2 : (int)(v.pw / 80));
    double sy = clair_plot_step(v.ymax - v.ymin, (int)(v.ph / 50) < 2 ? 2 : (int)(v.ph / 50));
    if (!clair_plot.hide_axes) {         /* the grid */
        clair_plot_str(b, "<path class=\"cp-grille\" d=\"");
        if (!bars)
            CLAIR_PLOT_TICKS(x, v.xmin, v.xmax, sx)
                clair_plot_printf(b, "M%.1f %.1fV%.1f", clair_plot_px(&v, x), v.top, bottom);
        CLAIR_PLOT_TICKS(y, v.ymin, v.ymax, sy)
            clair_plot_printf(b, "M%.1f %.1fH%.1f", v.left, clair_plot_py(&v, y), right);
        clair_plot_str(b, "\"/>\n");
    }
    /* the data, cut at the edges of the plot area */
    clair_plot_printf(b, "<g clip-path=\"url(#cp%d)\">\n", v.id);
    for (size_t i = 0; i < clair_plot.count; i++) {
        const ClairPlotItem *it = &clair_plot.items[i];
        switch (it->kind) {
            case CLAIR_PLOT_MAP: {
                double lo, hi;
                clair_plot_field_range(it, &lo, &hi);
                unsigned char *rgb = clair_plot_alloc(it->n * 3);
                for (size_t j = 0; j < it->ny; j++)
                    for (size_t k = 0; k < it->nx; k++) {
                        double val = it->x[j * it->nx + k];
                        unsigned char *p = rgb + ((it->ny - 1 - j) * it->nx + k) * 3;     /* row 0 at the bottom */
                        if (isfinite(val)) clair_plot_rgb(it->colormap, (val - lo) / (hi - lo), p);
                        else p[0] = p[1] = p[2] = 128;
                    }
                double x0 = clair_plot_px(&v, it->x0), x1 = clair_plot_px(&v, it->x1);
                double y0 = clair_plot_py(&v, it->y1), y1 = clair_plot_py(&v, it->y0);
                clair_plot_printf(b, "<image x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" preserveAspectRatio=\"none\" "
                                     "style=\"image-rendering:pixelated\" href=\"data:image/png;base64,", x0, y0, x1 - x0, y1 - y0);
                clair_plot_png_base64(b, rgb, it->nx, it->ny);
                clair_plot_str(b, "\"/>\n");
                free(rgb);
                break;
            }
            case CLAIR_PLOT_CONTOUR: {
                double lo, hi;
                clair_plot_field_range(it, &lo, &hi);
                double step = clair_plot_step(hi - lo, it->levels);
                clair_plot_printf(b, "<path fill=\"none\" stroke=\"%s\" stroke-width=\"%.2g\" stroke-linecap=\"round\" d=\"", it->color, it->width);
                CLAIR_PLOT_TICKS(level, lo, hi, step)
                    if (level > lo && level < hi) clair_plot_contour_level(b, &v, it, level);
                clair_plot_str(b, "\"/>\n");
                break;
            }
            case CLAIR_PLOT_ARROWS: {
                size_t every = (it->nx > it->ny ? it->nx : it->ny) / 24 + 1;
                double big = 0;
                for (size_t k = 0; k < it->n; k++) {
                    double m = sqrt(it->x[k] * it->x[k] + it->y[k] * it->y[k]);
                    if (isfinite(m) && m > big) big = m;
                }
                double cell_x = v.pw * (it->x1 - it->x0) / (v.xmax - v.xmin) / (double)it->nx * (double)every;
                double cell_y = v.ph * (it->y1 - it->y0) / (v.ymax - v.ymin) / (double)it->ny * (double)every;
                double cell = cell_x < cell_y ? cell_x : cell_y, scale = big > 0 ? 0.9 * cell / big : 0;
                double cw = (it->x1 - it->x0) / (double)it->nx, ch = (it->y1 - it->y0) / (double)it->ny;
                clair_plot_printf(b, "<path fill=\"none\" stroke=\"%s\" stroke-width=\"%.2g\" stroke-linecap=\"round\" d=\"", it->color, it->width);
                for (size_t j = every / 2; j < it->ny; j += every)
                    for (size_t k = every / 2; k < it->nx; k += every) {
                        double u = it->x[j * it->nx + k], w2 = it->y[j * it->nx + k];
                        if (!isfinite(u) || !isfinite(w2)) continue;
                        double cx = clair_plot_px(&v, it->x0 + ((double)k + 0.5) * cw), cy = clair_plot_py(&v, it->y0 + ((double)j + 0.5) * ch);
                        double dx = u * scale, dy = -w2 * scale;
                        if (fabs(dx) + fabs(dy) < 3) continue;          /* too short to be seen as an arrow */
                        clair_plot_printf(b, "M%.1f %.1fL%.1f %.1f", cx - dx / 2, cy - dy / 2, cx + dx / 2, cy + dy / 2);
                        clair_plot_arrow_head(b, cx - dx / 2, cy - dy / 2, cx + dx / 2, cy + dy / 2, 4 + it->width);
                    }
                clair_plot_str(b, "\"/>\n");
                break;
            }
            case CLAIR_PLOT_LINE: case CLAIR_PLOT_POLYGON: {
                clair_plot_printf(b, "<path fill=\"%s\"%s stroke=\"%s\" stroke-width=\"%.2g\" stroke-linejoin=\"round\" stroke-linecap=\"round\" d=\"",
                                  it->kind == CLAIR_PLOT_POLYGON ? it->fill : "none",
                                  it->kind == CLAIR_PLOT_POLYGON && strcmp(it->fill, "none") != 0 ? " fill-opacity=\"0.35\"" : "",
                                  it->color, it->width);
                int pen = 0;
                for (size_t k = 0; k < it->n; k++) {
                    if (!isfinite(it->x[k]) || !isfinite(it->y[k])) { pen = 0; continue; }
                    clair_plot_printf(b, "%c%.1f %.1f", pen ? 'L' : 'M', clair_plot_px(&v, it->x[k]), clair_plot_py(&v, it->y[k]));
                    pen = 1;
                }
                if (it->kind == CLAIR_PLOT_POLYGON && it->n) clair_plot_str(b, "Z");
                clair_plot_str(b, "\"/>\n");
                break;
            }
            case CLAIR_PLOT_POINTS:
                clair_plot_printf(b, "<path fill=\"none\" stroke=\"%s\" stroke-width=\"%.2g\" stroke-linecap=\"round\" d=\"",
                                  it->color, it->width * 3);
                for (size_t k = 0; k < it->n; k++)
                    if (isfinite(it->x[k]) && isfinite(it->y[k]))
                        clair_plot_printf(b, "M%.1f %.1fh0", clair_plot_px(&v, it->x[k]), clair_plot_py(&v, it->y[k]));
                clair_plot_str(b, "\"/>\n");
                break;
            case CLAIR_PLOT_BARS: case CLAIR_PLOT_HIST: {
                clair_plot_printf(b, "<g fill=\"%s\" class=\"cp-barre\">", it->fill);
                for (size_t k = 0; k < it->n; k++) {
                    double a = it->kind == CLAIR_PLOT_BARS ? (double)k - 0.4 : it->x[k];
                    double c = it->kind == CLAIR_PLOT_BARS ? (double)k + 0.4 : it->x[k] + it->c;
                    double y0 = clair_plot_py(&v, 0), y1 = clair_plot_py(&v, it->y[k]);
                    double x0 = clair_plot_px(&v, a), x1 = clair_plot_px(&v, c);
                    clair_plot_printf(b, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\"/>", x0,
                                      y0 < y1 ? y0 : y1, x1 - x0, fabs(y1 - y0));
                }
                clair_plot_str(b, "</g>\n");
                break;
            }
            case CLAIR_PLOT_SEGMENT: case CLAIR_PLOT_ARROW: {
                double x1 = clair_plot_px(&v, it->a), y1 = clair_plot_py(&v, it->b), x2 = clair_plot_px(&v, it->c), y2 = clair_plot_py(&v, it->d);
                clair_plot_printf(b, "<path fill=\"none\" stroke=\"%s\" stroke-width=\"%.2g\" stroke-linecap=\"round\" stroke-linejoin=\"round\" d=\"M%.1f %.1fL%.1f %.1f",
                                  it->color, it->width, x1, y1, x2, y2);
                if (it->kind == CLAIR_PLOT_ARROW) clair_plot_arrow_head(b, x1, y1, x2, y2, 6 + 2 * it->width);
                clair_plot_str(b, "\"/>\n");
                break;
            }
            case CLAIR_PLOT_CIRCLE: {
                double rx = it->c / (v.xmax - v.xmin) * v.pw, ry = it->c / (v.ymax - v.ymin) * v.ph;
                clair_plot_printf(b, "<ellipse cx=\"%.1f\" cy=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\" fill=\"%s\"%s stroke=\"%s\" stroke-width=\"%.2g\"/>\n",
                                  clair_plot_px(&v, it->a), clair_plot_py(&v, it->b), rx, ry, it->fill,
                                  strcmp(it->fill, "none") != 0 ? " fill-opacity=\"0.35\"" : "", it->color, it->width);
                break;
            }
            case CLAIR_PLOT_TEXT:
                clair_plot_printf(b, "<text class=\"cp-texte\" x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" dominant-baseline=\"middle\"%s%s%s>",
                                  clair_plot_px(&v, it->a), clair_plot_py(&v, it->b),
                                  strcmp(it->color, "currentColor") != 0 ? " fill=\"" : "",
                                  strcmp(it->color, "currentColor") != 0 ? it->color : "",
                                  strcmp(it->color, "currentColor") != 0 ? "\"" : "");
                clair_plot_xml(b, it->text);
                clair_plot_str(b, "</text>\n");
                break;
        }
    }
    clair_plot_str(b, "</g>\n");
    /* the axes, their ticks and their names */
    if (!clair_plot.hide_axes) {
        clair_plot_printf(b, "<path class=\"cp-axe\" fill=\"none\" d=\"M%.1f %.1fV%.1fH%.1f\"/>\n", v.left, v.top, bottom, right);
        clair_plot_str(b, "<g class=\"cp-texte cp-graduation\">");
        const ClairPlotItem *named = NULL;
        for (size_t i = 0; i < clair_plot.count; i++) if (clair_plot.items[i].kind == CLAIR_PLOT_BARS) named = &clair_plot.items[i];
        if (named) {
            for (size_t k = 0; k < named->n; k++) {
                double x = clair_plot_px(&v, (double)k);
                if (x < v.left - 1 || x > right + 1) continue;
                clair_plot_printf(b, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\">", x, bottom + 18);
                clair_plot_xml(b, named->names[k]);
                clair_plot_str(b, "</text>");
            }
        } else {
            CLAIR_PLOT_TICKS(x, v.xmin, v.xmax, sx) {
                clair_plot_number(t, sizeof(t), x, sx);
                clair_plot_printf(b, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\">%s</text>", clair_plot_px(&v, x), bottom + 18, t);
            }
        }
        CLAIR_PLOT_TICKS(y, v.ymin, v.ymax, sy) {
            clair_plot_number(t, sizeof(t), y, sy);
            clair_plot_printf(b, "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"end\" dominant-baseline=\"middle\">%s</text>",
                              v.left - 6, clair_plot_py(&v, y), t);
        }
        clair_plot_str(b, "</g>\n");
        if (clair_plot.xlabel) {
            clair_plot_printf(b, "<text class=\"cp-texte\" x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\">", v.left + v.pw / 2, (double)h - 8);
            clair_plot_xml(b, clair_plot.xlabel);
            clair_plot_str(b, "</text>\n");
        }
        if (clair_plot.ylabel) {
            clair_plot_printf(b, "<text class=\"cp-texte\" transform=\"translate(14 %.1f) rotate(-90)\" text-anchor=\"middle\">", v.top + v.ph / 2);
            clair_plot_xml(b, clair_plot.ylabel);
            clair_plot_str(b, "</text>\n");
        }
    }
    if (clair_plot.title) {
        clair_plot_printf(b, "<text class=\"cp-texte cp-titre\" x=\"%.1f\" y=\"22\" text-anchor=\"middle\">", (double)w / 2);
        clair_plot_xml(b, clair_plot.title);
        clair_plot_str(b, "</text>\n");
    }
    /* the scale of the last map in colors */
    const ClairPlotItem *map = NULL;
    for (size_t i = 0; i < clair_plot.count; i++) if (clair_plot.items[i].kind == CLAIR_PLOT_MAP) map = &clair_plot.items[i];
    if (map) {
        double lo, hi, x = right + 16;
        clair_plot_field_range(map, &lo, &hi);
        clair_plot_printf(b, "<defs><linearGradient id=\"cg%d\" x1=\"0\" y1=\"1\" x2=\"0\" y2=\"0\">", v.id);
        for (int k = 0; k <= 10; k++)
            clair_plot_printf(b, "<stop offset=\"%.1f\" stop-color=\"%s\"/>", k / 10.0, clair_plot_maps[map->colormap][k]);
        clair_plot_str(b, "</linearGradient></defs>\n");
        clair_plot_printf(b, "<rect x=\"%.1f\" y=\"%.1f\" width=\"14\" height=\"%.1f\" fill=\"url(#cg%d)\"/>\n", x, v.top, v.ph, v.id);
        double step = clair_plot_step(hi - lo, 5);
        clair_plot_str(b, "<g class=\"cp-texte cp-graduation\">");
        CLAIR_PLOT_TICKS(val, lo, hi, step) {
            clair_plot_number(t, sizeof(t), val, step);
            clair_plot_printf(b, "<text x=\"%.1f\" y=\"%.1f\" dominant-baseline=\"middle\">%s</text>", x + 20,
                              v.top + v.ph - (val - lo) / (hi - lo) * v.ph, t);
        }
        clair_plot_str(b, "</g>\n");
    }
    /* the legend: the data that have a name */
    size_t named = 0, longest = 0;
    for (size_t i = 0; i < clair_plot.count; i++)
        if (clair_plot.items[i].label) { named++; if (strlen(clair_plot.items[i].label) > longest) longest = strlen(clair_plot.items[i].label); }
    if (named) {
        double lw = 40 + 7.2 * (double)longest, lh = 8 + 18.0 * (double)named, lx = right - lw - 8, ly = v.top + 8;
        /* the corner that hides the fewest points: top right, top left, bottom right, bottom left */
        size_t best = (size_t)-1;
        for (int corner = 0; corner < 4; corner++) {
            double cx = corner % 2 ? v.left + 8 : right - lw - 8, cy = corner < 2 ? v.top + 8 : bottom - lh - 8;
            size_t hidden = 0;
            for (size_t i = 0; i < clair_plot.count; i++) {
                const ClairPlotItem *it = &clair_plot.items[i];
                if (it->kind != CLAIR_PLOT_LINE && it->kind != CLAIR_PLOT_POINTS && it->kind != CLAIR_PLOT_POLYGON) continue;
                for (size_t k = 0; k < it->n; k++) {
                    double px = clair_plot_px(&v, it->x[k]), py = clair_plot_py(&v, it->y[k]);
                    hidden += px >= cx - 4 && px <= cx + lw + 4 && py >= cy - 4 && py <= cy + lh + 4;
                }
            }
            if (hidden < best) { best = hidden; lx = cx; ly = cy; }
        }
        clair_plot_printf(b, "<rect class=\"cp-legende\" x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"4\"/>\n", lx, ly, lw, lh);
        double y = ly + 13;
        for (size_t i = 0; i < clair_plot.count; i++) {
            const ClairPlotItem *it = &clair_plot.items[i];
            if (!it->label) continue;
            int box = it->kind == CLAIR_PLOT_BARS || it->kind == CLAIR_PLOT_HIST;
            if (box) clair_plot_printf(b, "<rect x=\"%.1f\" y=\"%.1f\" width=\"18\" height=\"10\" fill=\"%s\"/>", lx + 8, y - 5, it->fill);
            else if (it->kind == CLAIR_PLOT_POINTS)
                clair_plot_printf(b, "<path stroke=\"%s\" stroke-width=\"6\" stroke-linecap=\"round\" d=\"M%.1f %.1fh0\"/>", it->color, lx + 17, y);
            else clair_plot_printf(b, "<path stroke=\"%s\" stroke-width=\"%.2g\" d=\"M%.1f %.1fh18\"/>", it->color, it->width, lx + 8, y);
            clair_plot_printf(b, "<text class=\"cp-texte\" x=\"%.1f\" y=\"%.1f\" dominant-baseline=\"middle\">", lx + 32, y);
            clair_plot_xml(b, it->label);
            clair_plot_str(b, "</text>\n");
            y += 18;
        }
    }
}

/* an SVG file: its colors follow the light or dark theme of the phone */
static void clair_plot_head(ClairPlotBuf *b, int w, int h, size_t frames, double fps) {
    clair_plot_printf(b, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" width=\"%d\" height=\"%d\" class=\"clair-plot\"", w, h, w, h);
    if (frames) clair_plot_printf(b, " data-frames=\"%zu\" data-fps=\"%g\"", frames, fps);
    clair_plot_str(b, ">\n<style>"
        ".clair-plot{color:#1f2937;font:12px system-ui,-apple-system,Roboto,sans-serif}"
        ".cp-fond{fill:#fff}.cp-grille{stroke:#e5e7eb;stroke-width:1;fill:none}.cp-axe{stroke:#6b7280;stroke-width:1}"
        ".cp-texte{fill:#1f2937}.cp-graduation{fill:#4b5563}.cp-titre{font-size:15px;font-weight:600}"
        ".cp-legende{fill:#fff;fill-opacity:.88;stroke:#d1d5db}.cp-barre{stroke:#fff;stroke-width:.5}"
        "@media (prefers-color-scheme:dark){.clair-plot{color:#e5e7eb}.cp-fond{fill:#16181d}.cp-grille{stroke:#2b2f37}"
        ".cp-axe{stroke:#8b919c}.cp-texte{fill:#e5e7eb}.cp-graduation{fill:#a3a9b3}.cp-legende{fill:#1f2229;stroke:#3a3f48}"
        ".cp-barre{stroke:#16181d}}"
        "</style>\n");
}

static void clair_plot_write(const char *path, const ClairPlotBuf *b) {
    FILE *f = fopen(path, "w");
    if (!f || fwrite(b->p, 1, b->len, f) != b->len || fclose(f) != 0) {
        char msg[600];
        snprintf(msg, sizeof(msg), "plot: cannot write %.500s: %s", path, strerror(errno));
        clair_plot_fail(msg);
    }
}

/* in Clair Studio: the file in CLAIR_PLOT_DIR and a line the Studio replaces by the picture */
static void clair_plot_emit(const ClairPlotBuf *b, const char *kind) {
    const char *dir = getenv("CLAIR_PLOT_DIR");
    char path[1200], name[64];
    int anim = strcmp(kind, "plot") != 0;
    int number = anim ? ++clair_plot.animations : ++clair_plot.shown;
    if (dir && *dir) {
        snprintf(name, sizeof(name), "%ld_%s%d_%d.svg", (long)getpid(), anim ? "a" : "", number, clair_plot_ids % 1000);
        snprintf(path, sizeof(path), "%.1000s/%s", dir, name);
        clair_plot_write(path, b);
        fflush(stdout);
        printf("\x1b[clair:%s %s]\n", kind, name);
        fflush(stdout);
    } else {
        snprintf(path, sizeof(path), "%s_%d.svg", anim ? "animation" : "plot", number);
        clair_plot_write(path, b);
        fflush(stdout);
        fprintf(stderr, "plot: %s (open it in a browser)\n", path);
    }
}

/* 0 show, 1 save into path, 2 keep as a frame of an animation, 3 show the animation */
static int64_t clair_plot_out(int64_t mode, const char *path, double fps) {
    ClairPlotBuf b = { 0 };
    if (mode == 2) {
        if (!clair_plot.frame_count) { clair_plot.frame_w = clair_plot.w; clair_plot.frame_h = clair_plot.h; }
        if (clair_plot.frame_count == clair_plot.frame_cap) {
            size_t cap = clair_plot.frame_cap ? clair_plot.frame_cap * 2 : 32;
            char **p = realloc(clair_plot.frames, cap * sizeof(char *));
            if (!p) clair_plot_fail("plot: out of memory");
            clair_plot.frames = p;
            clair_plot.frame_cap = cap;
        }
        if (clair_plot.frame_count >= 2000) clair_plot_fail("plot.frame(): at most 2000 frames in an animation");
        clair_plot_draw(&b, clair_plot.frame_w, clair_plot.frame_h);
        clair_plot.frames[clair_plot.frame_count++] = b.p ? b.p : clair_plot_dup("");
        clair_plot_free_items();             /* the next frame keeps the title, the axes, the size */
        return (int64_t)clair_plot.frame_count;
    }
    if (mode == 3) {
        if (!(fps > 0 && fps <= 120)) clair_plot_fail("plot.animate(): from 0.1 to 120 frames per second");
        if (!clair_plot.frame_count) clair_plot_fail("plot.animate(): no frame yet: draw, then plot.frame(), for each frame");
        size_t n = clair_plot.frame_count;
        double dur = (double)n / fps;
        clair_plot_head(&b, clair_plot.frame_w, clair_plot.frame_h, n, fps);
        for (size_t k = 0; k < n; k++) {
            /* SMIL: frame k is shown from k/n to (k+1)/n of the loop; without SMIL, the first frame */
            clair_plot_printf(&b, "<g display=\"%s\">", k == 0 ? "inline" : "none");
            if (n > 1) {
                if (k == 0) clair_plot_printf(&b, "<animate attributeName=\"display\" values=\"inline;none\" keyTimes=\"0;%.6f\"", 1.0 / (double)n);
                else if (k == n - 1) clair_plot_printf(&b, "<animate attributeName=\"display\" values=\"none;inline\" keyTimes=\"0;%.6f\"", (double)k / (double)n);
                else clair_plot_printf(&b, "<animate attributeName=\"display\" values=\"none;inline;none\" keyTimes=\"0;%.6f;%.6f\"",
                                       (double)k / (double)n, (double)(k + 1) / (double)n);
                clair_plot_printf(&b, " dur=\"%.4fs\" calcMode=\"discrete\" repeatCount=\"indefinite\"/>", dur);
            }
            clair_plot_str(&b, clair_plot.frames[k]);
            clair_plot_str(&b, "</g>\n");
            free(clair_plot.frames[k]);
        }
        clair_plot.frame_count = 0;
        clair_plot_str(&b, "</svg>\n");
        clair_plot_emit(&b, "anim");
        free(b.p);
        clair_plot_reset();
        return 0;
    }
    if (!clair_plot.count) {
        if (mode == 0) return 0;               /* nothing to show */
    }
    clair_plot_head(&b, clair_plot.w, clair_plot.h, 0, 0);
    clair_plot_draw(&b, clair_plot.w, clair_plot.h);
    clair_plot_str(&b, "</svg>\n");
    if (mode == 1) clair_plot_write(path, &b);
    else {
        clair_plot_emit(&b, "plot");
        clair_plot_reset();
    }
    free(b.p);
    return 0;
}

/* an image of the folder, shown in the console of Clair Studio */
static int64_t clair_plot_image(const char *path) {
    const char *dir = getenv("CLAIR_PLOT_DIR");
    if (!dir || !*dir) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) {
        char msg[600];
        snprintf(msg, sizeof(msg), "plot.image(): cannot read %.500s", path);
        clair_plot_fail(msg);
    }
    fclose(f);
    fflush(stdout);
    printf("\x1b[clair:image %s]\n", path);
    fflush(stdout);
    return 0;
}
