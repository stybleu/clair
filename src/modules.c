#include "modules.h"
#include "parser.h"
#include "units.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* The modules that live in the compiler                              */
/* ------------------------------------------------------------------ */

typedef struct { const char *name; const char *value; const char *unit; } ModConst;   /* unit: V0.48 */

/* "inf" and "nan" are written __builtin_inf() / __builtin_nan("") by code generation */
static const ModConst math_consts[] = {
    { "pi", "3.141592653589793", "1" }, { "e", "2.718281828459045", "1" }, { "tau", "6.283185307179586", "1" },
    { "inf", "inf", NULL }, { "nan", "nan", NULL }, { NULL, NULL, NULL }
};

/* the functions of math, by their Python name; fabs is Clair's abs */
static const char *const math_funcs[] = {
    "sqrt", "exp", "exp2", "expm1", "log", "log10", "log2", "log1p", "pow",
    "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "hypot",
    "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
    "floor", "ceil", "trunc", "fabs", "fmod", "copysign", "cbrt",
    "erf", "erfc", "gamma", "lgamma", "degrees", "radians",
    "isqrt", "gcd", "pi", NULL
};

/* CODATA 2022 (NIST), SI units; exact values as defined, derived exact ones rounded to the nearest double */
static const ModConst phys_consts[] = {
    { "c", "299792458.0", "m/s" },                     /* m/s, exact */
    { "h", "6.62607015e-34", "J*s" },                  /* J s, exact */
    { "hbar", "1.0545718176461565e-34", "J*s" },       /* J s, h / 2 pi */
    { "e", "1.602176634e-19", "C" },                 /* C, exact */
    { "N_A", "6.02214076e23", "1/mol" },                 /* 1/mol, exact */
    { "k_B", "1.380649e-23", "J/K" },                  /* J/K, exact */
    { "R", "8.31446261815324", "J/(mol*K)" },                /* J/(mol K), N_A k_B */
    { "F", "96485.33212331001", "C/mol" },               /* C/mol, N_A e */
    { "sigma", "5.6703744191844294e-08", "W/(m^2*K^4)" },      /* W/(m2 K4), Stefan-Boltzmann */
    { "b_wien", "0.0028977719551851727", "m*K" },      /* m K */
    { "g0", "9.80665", "m/s^2" },                        /* m/s2, exact */
    { "atm", "101325.0", "Pa" },                      /* Pa, exact */
    { "eV", "1.602176634e-19", "J" },                /* J, exact */
    { "G", "6.67430e-11", "m^3/(kg*s^2)" },                     /* m3/(kg s2), +- 0.00015e-11 */
    { "m_e", "9.1093837139e-31", "kg" },              /* kg */
    { "m_p", "1.67262192595e-27", "kg" },             /* kg */
    { "m_n", "1.67492750056e-27", "kg" },             /* kg */
    { "m_u", "1.66053906892e-27", "kg" },             /* kg */
    { "eps0", "8.8541878188e-12", "F/m" },             /* F/m */
    { "mu0", "1.25663706127e-06", "N/A^2" },             /* N/A2 */
    { "alpha", "0.0072973525643", "1" },             /* fine structure */
    { "R_inf", "10973731.568157", "1/m" },             /* 1/m, Rydberg */
    { "a0", "5.29177210544e-11", "m" },              /* m, Bohr radius */
    { "l_P", "1.616255e-35", "m" },                  /* m, Planck length */
    { "m_P", "2.176434e-08", "kg" },                  /* kg, Planck mass */
    { "t_P", "5.391247e-44", "s" },                  /* s, Planck time */
    { "T_P", "1.416784e32", "K" },                   /* K, Planck temperature */
    { NULL, NULL, NULL }
};

/* SI units.  GM: JPL ephemeris DE440 (system of the planet with its moons, except the Earth and
   the Moon); radii and solar luminosity: IAU 2015 resolution B3 (nominal); au: IAU 2012 B2. */
static const ModConst astro_consts[] = {
    { "UA", "149597870700.0", "m" }, { "AU", "149597870700.0", "m" },      /* m, exact */
    { "pc", "3.085677581491367e16", "m" },                            /* m, au * 648000 / pi */
    { "al", "9460730472580800.0", "m" }, { "ly", "9460730472580800.0", "m" },  /* m, c * 365.25 days, exact */
    { "jour", "86400.0", "s" }, { "an", "31557600.0", "s" },               /* s, Julian year */
    { "GM_soleil", "1.32712440041279419e20", "m^3/s^2" },
    { "GM_mercure", "2.2031868551e13", "m^3/s^2" },
    { "GM_venus", "3.24858592e14", "m^3/s^2" },
    { "GM_terre", "3.98600435507e14", "m^3/s^2" },
    { "GM_lune", "4.902800118e12", "m^3/s^2" },
    { "GM_mars", "4.2828375816e13", "m^3/s^2" },
    { "GM_jupiter", "1.267127641e17", "m^3/s^2" },
    { "GM_saturne", "3.79405848418e16", "m^3/s^2" },
    { "GM_uranus", "5.7945564e15", "m^3/s^2" },
    { "GM_neptune", "6.83652710058e15", "m^3/s^2" },
    { "GM_pluton", "9.755e11", "m^3/s^2" },
    { "R_soleil", "6.957e8", "m" }, { "L_soleil", "3.828e26", "W" },
    { "R_terre", "6.3781e6", "m" }, { "R_terre_polaire", "6.3568e6", "m" },
    { "R_jupiter", "7.1492e7", "m" }, { "R_jupiter_polaire", "6.6854e7", "m" },
    { NULL, NULL, NULL }
};

static const ModConst *builtin_consts(const char *module) {
    if (strcmp(module, "math") == 0) return math_consts;
    if (strcmp(module, "phys") == 0) return phys_consts;
    if (strcmp(module, "astro") == 0) return astro_consts;
    return NULL;
}

static int is_builtin_module(const char *module) { return builtin_consts(module) != NULL; }

static const char *builtin_const(const char *module, const char *name) {
    const ModConst *table = builtin_consts(module);
    for (size_t i = 0; table && table[i].name; i++) if (strcmp(table[i].name, name) == 0) return table[i].value;
    return NULL;
}

static const char *builtin_unit(const char *module, const char *name) {
    const ModConst *table = builtin_consts(module);
    for (size_t i = 0; table && table[i].name; i++) if (strcmp(table[i].name, name) == 0) return table[i].unit;
    return NULL;
}

static int builtin_func(const char *module, const char *name) {
    if (strcmp(module, "math") != 0) return 0;
    for (size_t i = 0; math_funcs[i]; i++) if (strcmp(math_funcs[i], name) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* File modules                                                       */
/* ------------------------------------------------------------------ */

#define MOD_MAX 64
#define SCOPE_MAX 256

typedef struct {
    char *name;
    AstNode *program;        /* its classes and functions, until they join the program */
    char *functions[1024];   /* its own functions, by their name in the file */
    size_t function_count;
    char *classes[256];
    size_t class_count;
} Module;

static Module modules[MOD_MAX];
static size_t module_count = 0;
static char *base_dir = NULL;
static int failed = 0;

static char *dup_text(const char *s) {
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

static char *join(const char *a, const char *sep, const char *b) {
    char *d = malloc(strlen(a) + strlen(sep) + strlen(b) + 1);
    if (d) { strcpy(d, a); strcat(d, sep); strcat(d, b); }
    return d;
}

static void import_error(const char *where, const char *fmt, const char *a, const char *b) {
    fputs("import error: ", stderr);
    fprintf(stderr, fmt, a, b);
    if (where) fprintf(stderr, " (in %s)", where);
    fputc('\n', stderr);
    failed = 1;
}

static char *read_source(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(buf); return NULL; }
    buf[size] = '\0';
    return buf;
}

/* a method is the top-level function Class__name whose first parameter is self: Class */
static int is_method(const AstNode *program, const AstNode *fn) {
    if (fn->child_count < 2 || fn->children[0]->kind != AST_PARAM || !fn->children[0]->text ||
        strcmp(fn->children[0]->text, "self") != 0 || !fn->children[0]->type_name) return 0;
    const char *cls = fn->children[0]->type_name;
    size_t len = strlen(cls);
    if (strncmp(fn->text, cls, len) != 0 || strncmp(fn->text + len, "__", 2) != 0) return 0;
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *c = program->children[i];
        if (c->kind == AST_CLASS && c->text && strcmp(c->text, cls) == 0) return 1;
    }
    return 0;
}

static Module *find_module(const char *name) {
    for (size_t i = 0; i < module_count; i++) if (strcmp(modules[i].name, name) == 0) return &modules[i];
    return NULL;
}

static int module_has_function(const Module *m, const char *name) {
    for (size_t i = 0; i < m->function_count; i++) if (strcmp(m->functions[i], name) == 0) return 1;
    return 0;
}

static int module_has_class(const Module *m, const char *name) {
    for (size_t i = 0; i < m->class_count; i++) if (strcmp(m->classes[i], name) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* What a file can see: its aliases and its imported names            */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *file;                         /* for messages: prog.cl, orbite.cl */
    const char *module;                       /* NULL for the program, else its module name */
    const char *alias[SCOPE_MAX];             /* m -> math, o -> orbite */
    const char *alias_module[SCOPE_MAX];
    size_t alias_count;
    const char *name[SCOPE_MAX];              /* GM_mars -> astro.GM_mars, g -> orbite.f */
    const char *name_module[SCOPE_MAX];
    const char *name_member[SCOPE_MAX];
    size_t name_count;
    char *own[1024];                          /* this file's own functions */
    size_t own_count;
    const AstNode *fn;                        /* the function being rewritten */
    char where[256];
} Scope;

static int process_file(AstNode *program, const char *module, const char *file);

/* reads, parses and registers a file module (once); NULL after an error */
static Module *load_module(const char *name, const char *from) {
    Module *m = find_module(name);
    if (m) return m;
    if (module_count >= MOD_MAX) {
        import_error(from, "too many modules (at most %d)%s", "64", "");
        return NULL;
    }
    size_t len = strlen(base_dir) + strlen(name) + 5;
    char *path = malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s%s.cl", base_dir, name);
    char *source = read_source(path);
    if (!source) {
        import_error(from, "no module named '%s' (no file %s)", name, path);
        free(path);
        return NULL;
    }
    Parser parser;
    parser_init(&parser, source);
    AstNode *program = parser_parse_program(&parser);
    free(source);
    if (!program || parser.had_error) {
        fprintf(stderr, "import error: the error above is in %s\n", path);
        failed = 1;
        ast_free(program);
        free(path);
        return NULL;
    }
    free(path);
    m = &modules[module_count++];
    memset(m, 0, sizeof(*m));
    m->name = dup_text(name);
    m->program = program;
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *c = program->children[i];
        if (c->kind == AST_FUNCTION && !is_method(program, c) && strcmp(c->text, "main") != 0 && m->function_count < 1024)
            m->functions[m->function_count++] = dup_text(c->text);
        if (c->kind == AST_CLASS && m->class_count < 256) m->classes[m->class_count++] = dup_text(c->text);
    }
    char file[256];
    snprintf(file, sizeof(file), "%s.cl", name);
    if (!process_file(program, name, file)) return NULL;
    return m;
}

static const char *scope_alias(const Scope *s, const char *alias) {
    for (size_t i = 0; i < s->alias_count; i++) if (strcmp(s->alias[i], alias) == 0) return s->alias_module[i];
    return NULL;
}

static int scope_name(const Scope *s, const char *name, const char **module, const char **member) {
    for (size_t i = 0; i < s->name_count; i++) {
        if (strcmp(s->name[i], name) == 0) { *module = s->name_module[i]; *member = s->name_member[i]; return 1; }
    }
    return 0;
}

static int scope_own(const Scope *s, const char *name) {
    for (size_t i = 0; i < s->own_count; i++) if (strcmp(s->own[i], name) == 0) return 1;
    return 0;
}

/* a parameter, let, for or comprehension variable of that name somewhere in the function */
static int declares(const AstNode *n, const char *name) {
    if (!n) return 0;
    if ((n->kind == AST_PARAM || n->kind == AST_LET || n->kind == AST_FOR || n->kind == AST_FOR_EACH) &&
        n->text && strcmp(n->text, name) == 0) return 1;
    if (n->kind == AST_CALL && n->text && strcmp(n->text, "@comprehension") == 0 && n->child_count > 0 &&
        n->children[0]->kind == AST_IDENTIFIER && n->children[0]->text && strcmp(n->children[0]->text, name) == 0)
        return 1;
    for (size_t i = 0; i < n->child_count; i++) if (declares(n->children[i], name)) return 1;
    return 0;
}

static void set_text(AstNode *n, const char *text) {
    char *d = dup_text(text);
    if (!d) { failed = 1; return; }
    free(n->text);
    n->text = d;
}

/* the node becomes the number of a constant, with its unit (V0.48) */
static void make_number(AstNode *n, const char *module, const char *member) {
    for (size_t i = 0; i < n->child_count; i++) ast_free(n->children[i]);
    n->child_count = 0;
    n->kind = AST_FLOAT;
    free(n->type_name);
    n->type_name = NULL;
    const char *unit = builtin_unit(module, member);      /* before set_text: member may be n->text */
    set_text(n, builtin_const(module, member));
    free(n->unit);
    n->unit = NULL;
    if (unit) {
        char err[160];
        n->unit = units_normalize(unit, err, sizeof(err));
        if (!n->unit) failed = 1;
    }
}

/* receiver.method(args) becomes the call name(args) */
static void make_call(AstNode *n, const char *name) {
    ast_free(n->children[0]);
    for (size_t i = 1; i < n->child_count; i++) n->children[i - 1] = n->children[i];
    n->child_count--;
    n->kind = AST_CALL;
    set_text(n, name);
}

/* how a call to member of module is written: math.sqrt (a builtin), orbite.f, or Point (a class) */
static char *call_name(const char *module, const char *member) {
    if (strcmp(module, "math") == 0) return join("math.", "", strcmp(member, "fabs") == 0 ? "abs" : member);
    Module *m = find_module(module);
    if (m && module_has_class(m, member)) return dup_text(member);
    return join(module, ".", member);
}

/* member of module, used as fn(...) (call) or as a value: checked against what the module has */
static int member_ok(Scope *s, const char *module, const char *member, int call) {
    if (is_builtin_module(module)) {
        int is_fn = builtin_func(module, member), is_const = builtin_const(module, member) != NULL;
        if (!is_fn && !is_const) {
            import_error(s->where, "module '%s' has no '%s'", module, member);
            return 0;
        }
        if (call && !is_fn && strcmp(member, "pi") != 0) {
            import_error(s->where, "%s.%s is a constant, not a function", module, member);
            return 0;
        }
        if (!call && !is_const) {
            import_error(s->where, "%s.%s is a function: call it with its arguments", module, member);
            return 0;
        }
        return 1;
    }
    Module *m = find_module(module);
    if (!m || (!module_has_function(m, member) && !module_has_class(m, member))) {
        import_error(s->where, "module '%s' has no function or class '%s'", module, member);
        return 0;
    }
    if (!call) {
        import_error(s->where, "%s.%s is a function or a class: a module has no variables", module, member);
        return 0;
    }
    return 1;
}

static void rewrite(Scope *s, AstNode *n) {
    if (!n || failed) return;
    const AstNode *fn = s->fn;
    switch (n->kind) {
        case AST_CALL:
            if (n->text && n->text[0] != '@') {
                const char *module, *member;
                if (s->module && scope_own(s, n->text)) {
                    char *full = join(s->module, ".", n->text);       /* f inside orbite.cl is orbite.f */
                    if (full) { free(n->text); n->text = full; }
                } else if (scope_name(s, n->text, &module, &member)) {
                    if (!member_ok(s, module, member, 1)) return;
                    char *full = call_name(module, member);
                    if (full) { free(n->text); n->text = full; }
                }
            }
            break;
        case AST_METHOD_CALL:
            if (n->child_count > 0 && n->children[0]->kind == AST_IDENTIFIER && n->children[0]->text &&
                !declares(fn, n->children[0]->text)) {
                const char *module = scope_alias(s, n->children[0]->text);
                if (module) {
                    if (!member_ok(s, module, n->text, 1)) return;
                    char *full = call_name(module, n->text);
                    if (!full) { failed = 1; return; }
                    make_call(n, full);
                    free(full);
                }
            }
            break;
        case AST_MEMBER:
            if (n->child_count == 1 && n->children[0]->kind == AST_IDENTIFIER && n->children[0]->text &&
                !declares(fn, n->children[0]->text)) {
                const char *module = scope_alias(s, n->children[0]->text);
                if (module) {
                    if (!member_ok(s, module, n->text, 0)) return;
                    make_number(n, module, n->text);
                    return;
                }
            }
            break;
        case AST_MEMBER_ASSIGN:            /* phys.G = 1.0 : children are the object, then the value */
            if (n->child_count == 2 && n->children[0]->kind == AST_IDENTIFIER && n->children[0]->text &&
                !declares(fn, n->children[0]->text) && scope_alias(s, n->children[0]->text)) {
                import_error(s->where, "%s.%s: the values of a module cannot be changed", n->children[0]->text, n->text);
                return;
            }
            break;
        case AST_IDENTIFIER:
            if (n->text && !declares(fn, n->text)) {
                const char *module, *member;
                if (scope_name(s, n->text, &module, &member)) {
                    if (!member_ok(s, module, member, 0)) return;
                    make_number(n, module, member);
                    return;
                }
            }
            break;
        default:
            break;
    }
    for (size_t i = 0; i < n->child_count; i++) rewrite(s, n->children[i]);
}

/* adds what one import statement brings to the scope of the file */
static int add_import(Scope *s, const AstNode *imp) {
    const char *module = imp->text;
    int builtin = is_builtin_module(module);
    if (!builtin && s->module && strcmp(module, s->module) == 0) {
        import_error(s->file, "module '%s' imports itself%s", module, "");
        return 0;
    }
    if (!builtin && !load_module(module, s->file)) return 0;
    module = builtin ? module : find_module(module)->name;     /* lives as long as the module table */
    if (imp->type_name && strcmp(imp->type_name, "from") == 0) {
        for (size_t i = 0; i < imp->child_count; i++) {
            const AstNode *x = imp->children[i];
            const char *as = x->child_count ? x->children[0]->text : x->text;
            int known = builtin ? (builtin_func(module, x->text) || builtin_const(module, x->text))
                                : (module_has_function(find_module(module), x->text) || module_has_class(find_module(module), x->text));
            if (!known) {
                import_error(s->file, "cannot import '%s' from '%s'", x->text, module);
                return 0;
            }
            const char *m2, *mem2;
            if (scope_name(s, as, &m2, &mem2) || scope_alias(s, as)) {
                import_error(s->file, "'%s' is imported twice%s", as, "");
                return 0;
            }
            if (scope_own(s, as)) {
                import_error(s->file, "'%s' is imported and also defined in this file%s", as, "");
                return 0;
            }
            if (s->name_count >= SCOPE_MAX) { import_error(s->file, "too many imported names%s%s", "", ""); return 0; }
            s->name[s->name_count] = as;
            s->name_module[s->name_count] = module;
            s->name_member[s->name_count++] = x->text;
        }
        return 1;
    }
    const char *as = imp->child_count ? imp->children[0]->text : imp->text;
    const char *m2, *mem2;
    if (scope_alias(s, as) || scope_name(s, as, &m2, &mem2)) {
        import_error(s->file, "'%s' is imported twice%s", as, "");
        return 0;
    }
    if (s->alias_count >= SCOPE_MAX) { import_error(s->file, "too many imports%s%s", "", ""); return 0; }
    s->alias[s->alias_count] = as;
    s->alias_module[s->alias_count++] = module;
    return 1;
}

static int process_file(AstNode *program, const char *module, const char *file) {
    Scope *s = calloc(1, sizeof(Scope));
    if (!s) return 0;
    s->file = file;
    s->module = module;
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *c = program->children[i];
        if (c->kind == AST_FUNCTION && !is_method(program, c) && s->own_count < 1024) s->own[s->own_count++] = c->text;
    }
    int ok = 1;
    for (size_t i = 0; i < program->child_count && ok; i++) {
        if (program->children[i]->kind == AST_IMPORT) ok = add_import(s, program->children[i]);
    }
    /* every function and method of the file (not a module's own main) sees through the imports */
    for (size_t i = 0; ok && !failed && i < program->child_count; i++) {
        AstNode *c = program->children[i];
        if (c->kind != AST_FUNCTION || (module && strcmp(c->text, "main") == 0)) continue;
        snprintf(s->where, sizeof(s->where), "%s() of %s", c->text, file);
        s->fn = c;
        rewrite(s, c);
    }
    if (failed) ok = 0;
    /* now the imports and the module's main can go, and the module's functions get their full name */
    /* names first: is_method looks at every class of the file, nothing may be freed yet */
    for (size_t i = 0; module && i < program->child_count; i++) {
        AstNode *c = program->children[i];
        if (c->kind == AST_FUNCTION && strcmp(c->text, "main") != 0 && !is_method(program, c)) {
            char *full = join(module, ".", c->text);
            if (full) { free(c->text); c->text = full; }
        }
    }
    size_t out = 0;
    for (size_t i = 0; i < program->child_count; i++) {
        AstNode *c = program->children[i];
        if (c->kind == AST_IMPORT || (module && c->kind == AST_FUNCTION && strcmp(c->text, "main") == 0)) {
            ast_free(c);
            continue;
        }
        program->children[out++] = c;
    }
    program->child_count = out;
    free(s);
    return ok;
}

static int same_class_twice(const AstNode *program) {
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *a = program->children[i];
        if (a->kind != AST_CLASS) continue;
        for (size_t j = i + 1; j < program->child_count; j++) {
            const AstNode *b = program->children[j];
            if (b->kind == AST_CLASS && strcmp(a->text, b->text) == 0) {
                import_error(NULL, "class '%s' is defined twice (in two files)%s", a->text, "");
                return 1;
            }
        }
    }
    return 0;
}

int modules_resolve(AstNode *program, const char *input_path) {
    int any = 0;
    for (size_t i = 0; i < program->child_count; i++) any |= program->children[i]->kind == AST_IMPORT;
    if (!any) return 1;                         /* nothing to do: the program stays exactly as parsed */

    const char *slash = strrchr(input_path, '/');
    size_t dir_len = slash ? (size_t)(slash - input_path) + 1 : 0;
    base_dir = malloc(dir_len + 1);
    if (!base_dir) return 0;
    memcpy(base_dir, input_path, dir_len);
    base_dir[dir_len] = '\0';
    module_count = 0;
    failed = 0;

    const char *file = slash ? slash + 1 : input_path;
    int ok = process_file(program, NULL, file);

    /* the modules' classes and functions come first, in the order they were read */
    size_t extra = 0;
    for (size_t m = 0; m < module_count; m++) extra += modules[m].program ? modules[m].program->child_count : 0;
    if (ok && extra) {
        AstNode **all = malloc((extra + program->child_count) * sizeof(AstNode *));
        if (!all) ok = 0;
        else {
            size_t n = 0;
            for (size_t m = 0; m < module_count; m++) {
                AstNode *mp = modules[m].program;
                for (size_t i = 0; mp && i < mp->child_count; i++) all[n++] = mp->children[i];
                if (mp) mp->child_count = 0;
            }
            for (size_t i = 0; i < program->child_count; i++) all[n++] = program->children[i];
            free(program->children);
            program->children = all;
            program->child_count = program->child_capacity = n;
        }
    }
    if (ok && same_class_twice(program)) ok = 0;
    for (size_t m = 0; m < module_count; m++) {
        ast_free(modules[m].program);
        free(modules[m].name);
        for (size_t i = 0; i < modules[m].function_count; i++) free(modules[m].functions[i]);
        for (size_t i = 0; i < modules[m].class_count; i++) free(modules[m].classes[i]);
    }
    module_count = 0;
    free(base_dir);
    base_dir = NULL;
    return ok && !failed;
}
