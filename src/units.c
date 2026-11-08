#include "units.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Units as products of symbols with whole exponents                  */
/* ------------------------------------------------------------------ */

#define DIM_MAX 12
#define SYM_LEN 16

typedef struct {
    int n;
    char sym[DIM_MAX][SYM_LEN];
    int exp[DIM_MAX];
    int overflow;            /* too many symbols or exponents too large */
} Dim;

static void dim_add(Dim *d, const char *sym, int e) {
    if (e == 0) return;
    for (int i = 0; i < d->n; i++) {
        if (strcmp(d->sym[i], sym) == 0) {
            d->exp[i] += e;
            if (d->exp[i] > 1000 || d->exp[i] < -1000) d->overflow = 1;
            if (d->exp[i] == 0) {
                for (int j = i + 1; j < d->n; j++) {
                    strcpy(d->sym[j - 1], d->sym[j]);
                    d->exp[j - 1] = d->exp[j];
                }
                d->n--;
            }
            return;
        }
    }
    if (d->n >= DIM_MAX || strlen(sym) >= SYM_LEN) { d->overflow = 1; return; }
    int at = d->n;                                   /* kept sorted by symbol */
    while (at > 0 && strcmp(d->sym[at - 1], sym) > 0) {
        strcpy(d->sym[at], d->sym[at - 1]);
        d->exp[at] = d->exp[at - 1];
        at--;
    }
    strcpy(d->sym[at], sym);
    d->exp[at] = e;
    d->n++;
}

static void dim_mul(Dim *out, const Dim *a, const Dim *b, int sign) {
    Dim r = *a;
    for (int i = 0; i < b->n; i++) dim_add(&r, b->sym[i], sign * b->exp[i]);
    r.overflow |= b->overflow;
    *out = r;
}

static int dim_eq(const Dim *a, const Dim *b) {
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++) {
        if (strcmp(a->sym[i], b->sym[i]) != 0 || a->exp[i] != b->exp[i]) return 0;
    }
    return 1;
}

static void dim_scale(Dim *d, int k) {
    for (int i = 0; i < d->n; i++) {
        long long e = (long long)d->exp[i] * k;
        if (e > 1000 || e < -1000) d->overflow = 1;
        else d->exp[i] = (int)e;
    }
    if (k == 0) d->n = 0;
}

/* the k-th root: every exponent must be a multiple of k */
static int dim_root(Dim *d, int k) {
    for (int i = 0; i < d->n; i++) if (d->exp[i] % k != 0) return 0;
    for (int i = 0; i < d->n; i++) d->exp[i] /= k;
    return 1;
}

/* "kg^-1 m^3 s^-2", or "1" */
static void dim_canonical(const Dim *d, char *out, size_t size) {
    size_t n = 0;
    out[0] = '\0';
    if (d->n == 0) { snprintf(out, size, "1"); return; }
    for (int i = 0; i < d->n && n < size; i++) {
        n += (size_t)snprintf(out + n, size - n, "%s%s^%d", i ? " " : "", d->sym[i], d->exp[i]);
    }
}

/* "m^3/(kg*s^2)", "1/s", "1" */
static void dim_display(const Dim *d, char *out, size_t size) {
    char num[256] = "", den[256] = "";
    int nn = 0, nd = 0;
    for (int i = 0; i < d->n; i++) {
        char *buf = d->exp[i] > 0 ? num : den;
        int *count = d->exp[i] > 0 ? &nn : &nd;
        int e = d->exp[i] > 0 ? d->exp[i] : -d->exp[i];
        size_t len = strlen(buf);
        if (e == 1) snprintf(buf + len, 256 - len, "%s%s", *count ? "*" : "", d->sym[i]);
        else snprintf(buf + len, 256 - len, "%s%s^%d", *count ? "*" : "", d->sym[i], e);
        (*count)++;
    }
    if (!nn && !nd) snprintf(out, size, "1");
    else if (!nd) snprintf(out, size, "%s", num);
    else if (nd == 1) snprintf(out, size, "%s/%s", nn ? num : "1", den);
    else snprintf(out, size, "%s/(%s)", nn ? num : "1", den);
}

/* reads "kg^-1 m^3 s^-2" back */
static int dim_from_canonical(const char *text, Dim *d) {
    memset(d, 0, sizeof(*d));
    if (!text || strcmp(text, "1") == 0) return 1;
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char sym[SYM_LEN];
        size_t n = 0;
        while (*p && *p != '^' && *p != ' ' && n + 1 < SYM_LEN) sym[n++] = *p++;
        sym[n] = '\0';
        int e = 1;
        if (*p == '^') e = (int)strtol(p + 1, (char **)&p, 10);
        dim_add(d, sym, e);
    }
    return !d->overflow;
}

/* SI units written with the base units m, kg, s, A, K, mol, cd */
static const struct { const char *name; const char *base; } si_units[] = {
    { "N", "kg^1 m^1 s^-2" }, { "J", "kg^1 m^2 s^-2" }, { "W", "kg^1 m^2 s^-3" }, { "Pa", "kg^1 m^-1 s^-2" },
    { "Hz", "s^-1" }, { "C", "A^1 s^1" }, { "V", "A^-1 kg^1 m^2 s^-3" }, { "Ohm", "A^-2 kg^1 m^2 s^-3" },
    { "F", "A^2 kg^-1 m^-2 s^4" }, { "T", "A^-1 kg^1 s^-2" }, { "Wb", "A^-1 kg^1 m^2 s^-2" },
    { "H", "A^-2 kg^1 m^2 s^-2" }, { "S", "A^2 kg^-1 m^-2 s^3" }, { "lm", "cd^1" }, { "lx", "cd^1 m^-2" },
    { "Gy", "m^2 s^-2" }, { "Sv", "m^2 s^-2" }, { "Bq", "s^-1" }, { "kat", "mol^1 s^-1" },
    { "rad", "1" }, { "sr", "1" }, { NULL, NULL }
};

typedef struct { const char *s; size_t pos; char *err; size_t err_size; int bad; } UnitParser;

static void up_fail(UnitParser *u, const char *msg) {
    if (!u->bad) snprintf(u->err, u->err_size, "%s", msg);
    u->bad = 1;
}

static void up_skip(UnitParser *u) { while (u->s[u->pos] == ' ' || u->s[u->pos] == '\t') u->pos++; }

static void up_expr(UnitParser *u, Dim *out);

static void up_factor(UnitParser *u, Dim *out) {
    memset(out, 0, sizeof(*out));
    up_skip(u);
    char c = u->s[u->pos];
    if (c == '(') {
        u->pos++;
        up_expr(u, out);
        up_skip(u);
        if (u->s[u->pos] != ')') { up_fail(u, "missing ')' in the unit"); return; }
        u->pos++;
    } else if (c == '1') {
        u->pos++;                                       /* 1/s */
    } else if (isalpha((unsigned char)c) || c == '_') {
        char sym[SYM_LEN];
        size_t n = 0;
        while (isalnum((unsigned char)u->s[u->pos]) || u->s[u->pos] == '_') {
            if (n + 1 >= SYM_LEN) { up_fail(u, "unit name too long"); return; }
            sym[n++] = u->s[u->pos++];
        }
        sym[n] = '\0';
        int si = -1;
        for (int i = 0; si_units[i].name; i++) if (strcmp(si_units[i].name, sym) == 0) si = i;
        if (si >= 0) dim_from_canonical(si_units[si].base, out);
        else dim_add(out, sym, 1);
    } else {
        up_fail(u, c ? "a unit is made of names, *, /, ^ and parentheses, e.g. m/s^2" : "empty unit");
        return;
    }
    up_skip(u);
    if (u->s[u->pos] == '^') {
        u->pos++;
        up_skip(u);
        int sign = 1;
        if (u->s[u->pos] == '-') { sign = -1; u->pos++; up_skip(u); }
        if (!isdigit((unsigned char)u->s[u->pos])) { up_fail(u, "a whole number must follow ^ in a unit"); return; }
        long e = 0;
        while (isdigit((unsigned char)u->s[u->pos])) {
            e = e * 10 + (u->s[u->pos++] - '0');
            if (e > 1000) { up_fail(u, "exponent too large in a unit"); return; }
        }
        dim_scale(out, (int)(sign * e));
    }
}

static void up_expr(UnitParser *u, Dim *out) {
    up_factor(u, out);
    for (;;) {
        if (u->bad) return;
        up_skip(u);
        char c = u->s[u->pos];
        if (c != '*' && c != '/') return;
        u->pos++;
        Dim f;
        up_factor(u, &f);
        dim_mul(out, out, &f, c == '*' ? 1 : -1);
    }
}

char *units_normalize(const char *text, char *err, size_t err_size) {
    UnitParser u = { text, 0, err, err_size, 0 };
    Dim d;
    up_expr(&u, &d);
    up_skip(&u);
    if (!u.bad && u.s[u.pos]) up_fail(&u, "unexpected text in the unit");
    if (!u.bad && d.overflow) up_fail(&u, "unit too complex (at most 12 symbols)");
    if (u.bad) return NULL;
    char canon[512];
    dim_canonical(&d, canon, sizeof(canon));
    char *out = malloc(strlen(canon) + 1);
    if (out) strcpy(out, canon);
    return out;
}

void units_display(const char *canonical, char *out, size_t out_size) {
    Dim d;
    dim_from_canonical(canonical, &d);
    dim_display(&d, out, out_size);
}

/* ------------------------------------------------------------------ */
/* Checking a program                                                 */
/* ------------------------------------------------------------------ */

enum { U_NONE, U_KNOWN, U_BARE, U_ZERO };   /* not checked, a unit, a number written without unit (0) */

typedef struct { int kind; Dim d; const AstNode *node; } Unit;

typedef struct { const char *name; int has; Dim d; } Var;

static const AstNode *program_root = NULL;
static const AstNode *current_fn = NULL;
static Var *vars = NULL;
static size_t var_count = 0, var_capacity = 0;
static int failed = 0;

static size_t unit_line = 0;             /* V0.49: the line being checked */
static const char *unit_file = NULL;

static void unit_error(const char *fmt, ...) {
    if (failed) return;                       /* the first error is enough */
    va_list ap;
    va_start(ap, fmt);
    fputs("unit error: ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (current_fn && current_fn->text) {
        const char *name = current_fn->text, *owner = strstr(name, "__");
        if (owner) fprintf(stderr, " (in %.*s.%s", (int)(owner - name), name, owner + 2);
        else fprintf(stderr, " (in %s", name);
        if (unit_line && unit_file) fprintf(stderr, ", %s, line %zu", unit_file, unit_line);
        else if (unit_line) fprintf(stderr, ", line %zu", unit_line);
        fputc(')', stderr);
    }
    fputc('\n', stderr);
    failed = 1;
}

static const char *show(const Dim *d) {
    static char buf[4][256];
    static int k = 0;
    k = (k + 1) % 4;
    dim_display(d, buf[k], sizeof(buf[k]));
    return buf[k];
}

static int is_float_type(const char *t) {
    return t && (strcmp(t, "float") == 0 || strcmp(t, "float32") == 0 || strcmp(t, "measure") == 0 ||
                 strcmp(t, "list[float]") == 0 || strcmp(t, "list[float32]") == 0);
}


static Unit none(void) { Unit u; memset(&u, 0, sizeof(u)); u.kind = U_NONE; return u; }
static Unit known(const Dim *d) { Unit u; memset(&u, 0, sizeof(u)); u.kind = U_KNOWN; u.d = *d; return u; }
static Unit dimensionless(void) { Unit u; memset(&u, 0, sizeof(u)); u.kind = U_KNOWN; return u; }

/* V0.53: value ± uncertainty: the same unit; a number written without unit takes the other one */
static Unit plus_minus(Unit v, Unit u) {
    if (failed || v.kind == U_NONE || u.kind == U_NONE) return none();
    if (v.kind == U_KNOWN && u.kind == U_KNOWN) {
        if (!dim_eq(&v.d, &u.d)) { unit_error("the value is in %s, its uncertainty in %s", show(&v.d), show(&u.d)); return none(); }
        return v;
    }
    if (v.kind == U_KNOWN) return v;
    if (u.kind == U_KNOWN) return u;
    Unit b = none();
    b.kind = U_BARE;
    b.node = v.node;
    return b;
}

static int decl_unit(const AstNode *decl, Dim *d) {
    if (!decl || !decl->unit) return 0;
    return dim_from_canonical(decl->unit, d);
}

static void push_var(const char *name, const AstNode *decl) {
    if (!name) return;
    if (var_count == var_capacity) {
        size_t cap = var_capacity ? var_capacity * 2 : 64;
        Var *v = realloc(vars, cap * sizeof(Var));
        if (!v) { failed = 1; return; }
        vars = v;
        var_capacity = cap;
    }
    Var *v = &vars[var_count++];
    v->name = name;
    v->has = decl_unit(decl, &v->d);
}

static void push_var_unit(const char *name, const Unit *u) {
    push_var(name, NULL);
    if (var_count && u->kind == U_KNOWN) { vars[var_count - 1].has = 1; vars[var_count - 1].d = u->d; }
}

static const Var *find_var(const char *name) {
    for (size_t i = var_count; i > 0; i--) if (strcmp(vars[i - 1].name, name) == 0) return &vars[i - 1];
    return NULL;
}

static const AstNode *find_node(AstKind kind, const char *name) {
    for (size_t i = 0; name && i < program_root->child_count; i++) {
        const AstNode *c = program_root->children[i];
        if (c->kind == kind && c->text && strcmp(c->text, name) == 0) return c;
    }
    return NULL;
}

static const AstNode *find_field(const char *cls_name, const char *field) {
    const AstNode *cls = find_node(AST_CLASS, cls_name);
    for (size_t i = 0; cls && i < cls->child_count; i++) {
        const AstNode *f = cls->children[i];
        if (f->kind == AST_FIELD && f->text && strcmp(f->text, field) == 0) return f;
    }
    return NULL;
}

static Unit unit_of(const AstNode *e);

/* a value going where `want` is declared: a number without unit takes it, a unit must match */
static void expect(const AstNode *decl, Unit got, const char *what) {
    Dim want;
    if (failed || !decl_unit(decl, &want) || got.kind != U_KNOWN) return;
    if (!dim_eq(&want, &got.d)) unit_error("%s is in %s, it receives %s", what, show(&want), show(&got.d));
}

static const char *literal_text(const AstNode *n) {
    return n && (n->kind == AST_FLOAT || n->kind == AST_INTEGER) && n->text ? n->text : NULL;
}

/* + - % and comparisons: the same unit on both sides */
static Unit same_unit(Unit l, Unit r, const char *op) {
    if (failed) return none();
    if (l.kind == U_KNOWN && r.kind == U_KNOWN) {
        if (!dim_eq(&l.d, &r.d)) {
            const char *verb = !strcmp(op, "+") ? "add" : !strcmp(op, "-") ? "subtract" : "compare";
            if (!strcmp(op, "%")) verb = "take the remainder of";
            unit_error("cannot %s %s and %s", verb, show(&l.d), show(&r.d));
            return none();
        }
        return l;
    }
    if ((l.kind == U_KNOWN && r.kind == U_BARE) || (r.kind == U_KNOWN && l.kind == U_BARE)) {
        const Unit *k = l.kind == U_KNOWN ? &l : &r, *b = l.kind == U_KNOWN ? &r : &l;
        const char *lit = literal_text(b->node);
        if (k->d.n == 0) return *k;                     /* a number without unit next to a pure number */
        if (lit) unit_error("%s has no unit next to %s: write %s[%s]", lit, show(&k->d), lit, show(&k->d));
        else unit_error("a number without unit next to %s", show(&k->d));
        return none();
    }
    if (l.kind == U_KNOWN) return l;                     /* with 0 */
    if (r.kind == U_KNOWN) return r;
    if (l.kind == U_NONE || r.kind == U_NONE) return none();
    Unit u = none();
    u.kind = l.kind == U_ZERO && r.kind == U_ZERO ? U_ZERO : U_BARE;
    return u;
}

static Unit product(Unit l, Unit r, int sign) {
    if (l.kind == U_NONE || r.kind == U_NONE) return none();
    if (l.kind != U_KNOWN && r.kind != U_KNOWN) { Unit u = none(); u.kind = U_BARE; return u; }
    Dim a = l.kind == U_KNOWN ? l.d : (Dim){ 0 }, b = r.kind == U_KNOWN ? r.d : (Dim){ 0 }, out;
    dim_mul(&out, &a, &b, sign);
    if (out.overflow) { unit_error("unit too complex: %s %s %s", show(&a), sign > 0 ? "*" : "/", show(&b)); return none(); }
    return known(&out);
}

/* the arguments of a call against the parameters of the function (from index `from`) */
static void check_args(const AstNode *call, const AstNode *fn, size_t from, const char *shown) {
    size_t params = fn->child_count ? fn->child_count - 1 : 0;
    for (size_t i = from; i < call->child_count; i++) {
        Unit got = unit_of(call->children[i]);
        if (i < params && fn->children[i]->kind == AST_PARAM) {
            char what[256];
            snprintf(what, sizeof(what), "argument %zu of %s (%s)", i + 1 - from, shown, fn->children[i]->text);
            expect(fn->children[i], got, what);
        }
    }
}

static int is_one_of(const char *name, const char *const *list) {
    for (size_t i = 0; list[i]; i++) if (strcmp(name, list[i]) == 0) return 1;
    return 0;
}

/* a whole number written in the program: 2, -3 */
static int literal_int(const AstNode *n, int *k) {
    int sign = 1;
    if (n && n->kind == AST_UNARY && n->text && !strcmp(n->text, "-") && n->child_count == 1) { sign = -1; n = n->children[0]; }
    if (!n || !n->text || (n->kind != AST_INTEGER && n->kind != AST_FLOAT)) return 0;
    char *end;
    double v = strtod(n->text, &end);
    if (*end || v != (double)(long)v || v > 1000 || v < -1000) return 0;
    *k = sign * (int)v;
    return 1;
}

static Unit builtin_unit(const AstNode *e) {
    static const char *const pure[] = {
        "exp", "exp2", "expm1", "log", "log2", "log10", "log1p", "sin", "cos", "tan", "asin", "acos", "atan",
        "sinh", "cosh", "tanh", "asinh", "acosh", "atanh", "erf", "erfc", "gamma", "lgamma", NULL
    };
    static const char *const keep[] = { "abs", "floor", "ceil", "trunc", NULL };
    static const char *const pair[] = { "min", "max", "hypot", "fmod", NULL };
    const char *name = e->text + 1;
    size_t argc = e->child_count;
    /* V0.53: a number as a measure, value(m), uncertainty(m): the same unit; measure(v, u) like v ± u */
    if ((strcmp(name, "to_measure") == 0 || strcmp(name, "value") == 0 || strcmp(name, "uncertainty") == 0 ||
         strcmp(name, "measure") == 0) && argc == 1)
        return unit_of(e->children[0]);
    if (strcmp(name, "measure") == 0 && argc == 2) {
        Unit v = unit_of(e->children[0]), u = unit_of(e->children[1]);
        return plus_minus(v, u);
    }
    if (strncmp(name, "bn_", 3) == 0 || strcmp(name, "convert") == 0 || strncmp(name, "slice", 5) == 0) {
        Unit first = argc ? unit_of(e->children[0]) : none();
        for (size_t i = 1; i < argc; i++) unit_of(e->children[i]);
        return strncmp(name, "slice", 5) == 0 ? first : none();
    }
    if ((strcmp(name, "sqrt") == 0 || strcmp(name, "cbrt") == 0) && argc == 1) {
        Unit u = unit_of(e->children[0]);
        if (u.kind != U_KNOWN) return u;
        int k = name[0] == 's' ? 2 : 3;
        Dim d = u.d;
        if (!dim_root(&d, k)) {
            unit_error("%s() of %s: every exponent of the unit must be a multiple of %d", name, show(&u.d), k);
            return none();
        }
        return known(&d);
    }
    if ((strcmp(name, "radians") == 0 || strcmp(name, "degrees") == 0) && argc == 1) {
        /* radians(30.0[deg]) is a number without unit (radians), degrees(x) is in deg */
        Unit u = unit_of(e->children[0]);
        if (u.kind == U_NONE) return none();
        Dim deg = { 0 };
        dim_add(&deg, "deg", 1);
        int is_deg = u.kind == U_KNOWN && dim_eq(&u.d, &deg);
        if (name[0] == 'r' && u.kind == U_KNOWN && u.d.n && !is_deg) {
            unit_error("radians() needs deg or a number without unit, it receives %s", show(&u.d));
            return none();
        }
        if (name[0] == 'd' && u.kind == U_KNOWN && u.d.n) {
            unit_error("degrees() needs radians (a number without unit), it receives %s", show(&u.d));
            return none();
        }
        return name[0] == 'r' ? dimensionless() : known(&deg);
    }
    if (is_one_of(name, pure) && argc == 1) {
        Unit u = unit_of(e->children[0]);
        if (u.kind == U_NONE) return none();
        if (u.kind == U_KNOWN && u.d.n) {
            unit_error("%s() needs a number without unit, it receives %s", name, show(&u.d));
            return none();
        }
        return dimensionless();
    }
    if (is_one_of(name, keep) && argc == 1) return unit_of(e->children[0]);
    if (is_one_of(name, pair) && argc == 2) {
        Unit a = unit_of(e->children[0]), b = unit_of(e->children[1]);
        return same_unit(a, b, "compare");
    }
    if (strcmp(name, "atan2") == 0 && argc == 2) {
        Unit a = unit_of(e->children[0]), b = unit_of(e->children[1]);
        Unit u = same_unit(a, b, "compare");
        return u.kind == U_NONE && (a.kind == U_NONE || b.kind == U_NONE) ? none() : dimensionless();
    }
    if (strcmp(name, "copysign") == 0 && argc == 2) {
        Unit a = unit_of(e->children[0]);
        unit_of(e->children[1]);
        return a;
    }
    if (strcmp(name, "pow") == 0 && argc == 2) {
        Unit x = unit_of(e->children[0]), y = unit_of(e->children[1]);
        if (y.kind == U_KNOWN && y.d.n) { unit_error("the exponent of pow() cannot have a unit (%s)", show(&y.d)); return none(); }
        int k;
        if (x.kind != U_KNOWN) return x.kind == U_NONE ? none() : x;
        if (literal_int(e->children[1], &k)) {
            Dim d = x.d;
            dim_scale(&d, k);
            return known(&d);
        }
        if (x.d.n) { unit_error("pow() of %s needs a whole exponent written in the program, e.g. pow(x, 2)", show(&x.d)); return none(); }
        return x;
    }
    if ((strcmp(name, "float") == 0 || strcmp(name, "float32") == 0) && argc == 1) {
        Unit u = unit_of(e->children[0]);
        const char *t = e->children[0]->type_name;
        if (u.kind == U_KNOWN || is_float_type(t)) return u;
        Unit b = none();
        b.kind = U_BARE;                               /* float(n): a number without unit */
        b.node = NULL;
        return b;
    }
    for (size_t i = 0; i < argc; i++) unit_of(e->children[i]);
    return none();
}

static Unit unit_of_at(const AstNode *e);

/* V0.49: the line of e while it is checked */
static Unit unit_of(const AstNode *e) {
    size_t saved_line = unit_line;
    const char *saved_file = unit_file;
    if (e && e->line) { unit_line = e->line; unit_file = e->file; }
    Unit u = unit_of_at(e);
    unit_line = saved_line;
    unit_file = saved_file;
    return u;
}

static Unit unit_of_at(const AstNode *e) {
    if (!e || failed) return none();
    switch (e->kind) {
        case AST_FLOAT:
        case AST_INTEGER: {
            Dim d;
            if (e->unit && dim_from_canonical(e->unit, &d)) return known(&d);
            Unit u = none();
            u.kind = e->text && strtod(e->text, NULL) == 0.0 ? U_ZERO : U_BARE;
            u.node = e;
            return u;
        }
        case AST_IDENTIFIER: {
            const Var *v = e->text ? find_var(e->text) : NULL;
            return v && v->has ? known(&v->d) : none();
        }
        case AST_MEMBER: {
            if (e->child_count != 1) return none();
            unit_of(e->children[0]);
            Dim d;
            const AstNode *f = find_field(e->children[0]->type_name, e->text);
            return f && decl_unit(f, &d) ? known(&d) : none();
        }
        case AST_INDEX: {
            if (e->child_count != 2) return none();
            Unit base = unit_of(e->children[0]);
            unit_of(e->children[1]);
            const char *t = e->children[0]->type_name;
            return t && strncmp(t, "list[", 5) == 0 ? base : none();
        }
        case AST_UNARY: {
            if (e->child_count != 1) return none();
            Unit u = unit_of(e->children[0]);
            return e->text && strcmp(e->text, "-") == 0 ? u : none();
        }
        case AST_BINARY: {
            if (e->child_count != 2 || !e->text) return none();
            const char *op = e->text;
            if (e->type_name && strncmp(e->type_name, "list[", 5) == 0 && !strcmp(op, "*")) {
                /* [0.0] * n : the list's unit */
                const AstNode *xs = e->children[0]->type_name && !strncmp(e->children[0]->type_name, "list[", 5)
                                    ? e->children[0] : e->children[1];
                Unit u = unit_of(xs);
                unit_of(xs == e->children[0] ? e->children[1] : e->children[0]);
                return u;
            }
            Unit l = unit_of(e->children[0]), r = unit_of(e->children[1]);
            if (failed) return none();
            if (strcmp(op, "\xc2\xb1") == 0) return plus_minus(l, r);        /* V0.53 */
            int cmp = !strcmp(op, "<") || !strcmp(op, "<=") || !strcmp(op, ">") || !strcmp(op, ">=") ||
                      !strcmp(op, "==") || !strcmp(op, "!=");
            if (cmp) {
                if (is_float_type(e->children[0]->type_name) || is_float_type(e->children[1]->type_name))
                    same_unit(l, r, op);
                return none();
            }
            if (!is_float_type(e->type_name)) return none();
            if (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "%")) return same_unit(l, r, op);
            if (!strcmp(op, "*")) return product(l, r, 1);
            if (!strcmp(op, "/")) return product(l, r, -1);
            return none();
        }
        case AST_LIST: {
            Unit all = none();
            int first = 1;
            for (size_t i = 0; i < e->child_count; i++) {
                Unit u = unit_of(e->children[i]);
                if (first) { all = u; first = 0; }
                else if (all.kind != U_NONE) all = same_unit(all, u, "compare");
            }
            return all;
        }
        case AST_CALL: {
            if (!e->text) return none();
            if (e->text[0] == '@') return builtin_unit(e);
            const AstNode *fn = find_node(AST_FUNCTION, e->text);
            if (!fn) {
                for (size_t i = 0; i < e->child_count; i++) unit_of(e->children[i]);
                return none();
            }
            /* a method call obj.f(x) is the call Class__f(obj, x) here */
            int method = fn->child_count > 1 && fn->children[0]->kind == AST_PARAM && fn->children[0]->text &&
                         strcmp(fn->children[0]->text, "self") == 0 && strstr(e->text, "__");
            char shown[256];
            if (method) {
                const char *cut = strstr(e->text, "__");
                snprintf(shown, sizeof(shown), "%.*s.%s", (int)(cut - e->text), e->text, cut + 2);
            } else {
                snprintf(shown, sizeof(shown), "%s", e->text);
            }
            check_args(e, fn, method ? 1 : 0, shown);
            Dim d;
            return decl_unit(fn, &d) ? known(&d) : none();
        }
        case AST_CONSTRUCT: {
            const AstNode *cls = find_node(AST_CLASS, e->text);
            size_t field = 0;
            for (size_t i = 0; i < e->child_count; i++) {
                Unit got = unit_of(e->children[i]);
                while (cls && field < cls->child_count && cls->children[field]->kind != AST_FIELD) field++;
                if (cls && field < cls->child_count) {
                    char what[256];
                    snprintf(what, sizeof(what), "%s.%s", e->text, cls->children[field]->text);
                    expect(cls->children[field], got, what);
                    field++;
                }
            }
            return none();
        }
        case AST_METHOD_CALL: {
            if (e->child_count == 0 || !e->text) return none();
            unit_of(e->children[0]);
            const char *cls = e->children[0]->type_name;
            char name[256];
            snprintf(name, sizeof(name), "%s__%s", cls ? cls : "", e->text);
            const AstNode *fn = find_node(AST_FUNCTION, name);
            if (!fn) {
                for (size_t i = 1; i < e->child_count; i++) unit_of(e->children[i]);
                return none();
            }
            char shown[256];
            snprintf(shown, sizeof(shown), "%s.%s", cls ? cls : "", e->text);
            check_args(e, fn, 1, shown);
            Dim d;
            return decl_unit(fn, &d) ? known(&d) : none();
        }
        default:
            for (size_t i = 0; i < e->child_count; i++) unit_of(e->children[i]);
            return none();
    }
}

/* the declaration that holds the unit of a write target: a variable or a field chain */
static void expect_var(const char *name, Unit got) {
    const Var *v = name ? find_var(name) : NULL;
    if (!v || !v->has || got.kind != U_KNOWN || failed) return;
    if (!dim_eq(&v->d, &got.d)) unit_error("%s is in %s, it receives %s", name, show(&v->d), show(&got.d));
}

static void expect_unit(Unit want, Unit got, const char *what) {
    if (want.kind != U_KNOWN || got.kind != U_KNOWN || failed) return;
    if (!dim_eq(&want.d, &got.d)) unit_error("%s is in %s, it receives %s", what, show(&want.d), show(&got.d));
}

static void check_block(const AstNode *block);

static void check_stmt_at(const AstNode *n);

static void check_stmt(const AstNode *n) {
    size_t saved_line = unit_line;
    const char *saved_file = unit_file;
    if (n && n->line) { unit_line = n->line; unit_file = n->file; }
    check_stmt_at(n);
    unit_line = saved_line;
    unit_file = saved_file;
}

static void check_stmt_at(const AstNode *n) {
    if (!n || failed) return;
    switch (n->kind) {
        case AST_BLOCK:
            check_block(n);
            return;
        case AST_LET: {
            Unit got = n->child_count ? unit_of(n->children[0]) : none();
            expect(n, got, n->text ? n->text : "the variable");
            push_var(n->text, n);
            return;
        }
        case AST_ASSIGN: {
            Unit got = n->child_count ? unit_of(n->children[0]) : none();
            expect_var(n->text, got);
            return;
        }
        case AST_INDEX_ASSIGN:
        case AST_LIST_APPEND: {
            /* xs[i] = v / xs.append(v): text is the list, or the last child is a field chain */
            size_t value = n->kind == AST_INDEX_ASSIGN ? 1 : 0;
            if (n->child_count <= value) return;
            if (n->kind == AST_INDEX_ASSIGN) unit_of(n->children[0]);
            Unit got = unit_of(n->children[value]);
            if (n->text) expect_var(n->text, got);
            else if (n->child_count > value + 1) {
                Unit want = unit_of(n->children[n->child_count - 1]);
                expect_unit(want, got, "the list");
            }
            return;
        }
        case AST_MEMBER_ASSIGN: {
            if (n->child_count != 2) return;
            unit_of(n->children[0]);
            Unit got = unit_of(n->children[1]);
            const AstNode *f = find_field(n->children[0]->type_name, n->text);
            char what[256];
            snprintf(what, sizeof(what), "%s.%s", n->children[0]->type_name ? n->children[0]->type_name : "", n->text);
            expect(f, got, what);
            return;
        }
        case AST_RETURN: {
            Unit got = n->child_count ? unit_of(n->children[0]) : none();
            expect(current_fn, got, "the value returned");
            return;
        }
        case AST_FOR: {
            for (size_t i = 0; i + 1 < n->child_count; i++) unit_of(n->children[i]);
            size_t mark = var_count;
            push_var(n->text, NULL);
            if (n->child_count) check_stmt(n->children[n->child_count - 1]);
            var_count = mark;
            return;
        }
        case AST_FOR_EACH: {
            if (n->child_count != 2) return;
            Unit list = unit_of(n->children[0]);
            size_t mark = var_count;
            push_var_unit(n->text, &list);
            check_stmt(n->children[1]);
            var_count = mark;
            return;
        }
        case AST_IF:
        case AST_WHILE:
            if (n->child_count) unit_of(n->children[0]);
            for (size_t i = 1; i < n->child_count; i++) check_stmt(n->children[i]);
            return;
        default:
            for (size_t i = 0; i < n->child_count; i++) unit_of(n->children[i]);
            return;
    }
}

static void check_block(const AstNode *block) {
    size_t mark = var_count;
    for (size_t i = 0; i < block->child_count && !failed; i++) check_stmt(block->children[i]);
    var_count = mark;
}

static int any_unit(const AstNode *n) {
    if (!n) return 0;
    if (n->unit) return 1;
    for (size_t i = 0; i < n->child_count; i++) if (any_unit(n->children[i])) return 1;
    return 0;
}

int units_check(AstNode *program) {
    if (!any_unit(program)) return 1;            /* no unit written: nothing to check */
    program_root = program;
    failed = 0;
    for (size_t i = 0; i < program->child_count && !failed; i++) {
        const AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION || fn->child_count == 0) continue;
        current_fn = fn;
        var_count = 0;
        for (size_t p = 0; p + 1 < fn->child_count; p++) push_var(fn->children[p]->text, fn->children[p]);
        check_stmt(fn->children[fn->child_count - 1]);
    }
    free(vars);
    vars = NULL;
    var_count = var_capacity = 0;
    current_fn = NULL;
    return !failed;
}
