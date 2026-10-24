#include "codegen_c.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void indent(FILE *f, int depth) {
    for (int i = 0; i < depth; i++) fputs("    ", f);
}

static int is_list_type(const char *type) {
    return type && (
        strcmp(type, "list[int]") == 0 ||
        strcmp(type, "list[float]") == 0 ||
        strcmp(type, "list[bool]") == 0 ||
        strcmp(type, "list[string]") == 0
    );
}

static int is_dict_type(const char *type) {
    return type && (
        strcmp(type, "dict[string,int]") == 0 ||
        strcmp(type, "dict[string,float]") == 0 ||
        strcmp(type, "dict[string,bool]") == 0 ||
        strcmp(type, "dict[string,string]") == 0
    );
}


static int is_matrix_type(const char *type) {
    return type && (strcmp(type, "matrix[float]") == 0 || strcmp(type, "matrix[int]") == 0);
}

static int is_matrix_row_type(const char *type) {
    return type && strncmp(type, "matrix_row[", 11) == 0;
}

/* C element type of a matrix or of one of its rows */
static const char *matrix_element_c_type(const char *type) {
    if (!type) return NULL;
    if (strstr(type, "[float]")) return "double";
    if (strstr(type, "[int]")) return "int64_t";
    return NULL;
}

static const char *matrix_accessor_name(const char *type) {
    const char *e = matrix_element_c_type(type);
    if (!e) return NULL;
    return strcmp(e, "double") == 0 ? "clair_rt_matrix_at_f64_checked" : "clair_rt_matrix_at_i64_checked";
}

static const char *c_type_name(const char *clair_type) {
    static char class_type[512];

    if (!clair_type) return NULL;
    if (strcmp(clair_type, "int") == 0) return "int64_t";
    if (strcmp(clair_type, "float") == 0) return "double";
    if (strcmp(clair_type, "bool") == 0) return "bool";
    if (strcmp(clair_type, "string") == 0) return "const char *";
    if (is_list_type(clair_type)) return "ClairList";
    if (is_dict_type(clair_type)) return "ClairDict";
    if (is_matrix_type(clair_type)) return "ClairMatrix";

    int written = snprintf(class_type, sizeof(class_type), "ClairClass_%s *", clair_type);
    if (written < 0 || (size_t)written >= sizeof(class_type)) return NULL;
    return class_type;
}

static const char *list_element_c_type(const char *list_type) {
    if (!list_type) return NULL;
    if (strcmp(list_type, "list[int]") == 0) return "int64_t";
    if (strcmp(list_type, "list[float]") == 0) return "double";
    if (strcmp(list_type, "list[bool]") == 0) return "bool";
    if (strcmp(list_type, "list[string]") == 0) return "const char *";
    return NULL;
}

static const char *dict_value_c_type(const char *dict_type) {
    if (!dict_type) return NULL;
    if (strcmp(dict_type, "dict[string,int]") == 0) return "int64_t";
    if (strcmp(dict_type, "dict[string,float]") == 0) return "double";
    if (strcmp(dict_type, "dict[string,bool]") == 0) return "bool";
    if (strcmp(dict_type, "dict[string,string]") == 0) return "const char *";
    return NULL;
}


static const AstNode *codegen_program = NULL;

/* ------------------------------------------------------------------ */
/* Small string helper (used to build the bounds pre-checks).         */
/* ------------------------------------------------------------------ */

static char *str_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) return NULL;
    char *out = malloc((size_t)len + 1);
    if (!out) return NULL;
    va_start(ap, fmt);
    vsnprintf(out, (size_t)len + 1, fmt, ap);
    va_end(ap);
    return out;
}

static char *str_dup(const char *s) { return str_fmt("%s", s); }

/* ------------------------------------------------------------------ */
/* Constant folding of integer literals.                              */
/*                                                                    */
/* C evaluates `100000 * 100000` in 32-bit `int`, while Clair `int`   */
/* is 64-bit.  Folding literal-only +, -, * at compile time (with the */
/* same wrap-around as -fwrapv) gives the right value and emits one   */
/* plain literal, as a person would write it.                         */
/* ------------------------------------------------------------------ */

static int const_int_value(const AstNode *n, int64_t *out) {
    if (!n || !n->type_name || strcmp(n->type_name, "int") != 0) return 0;

    if (n->kind == AST_INTEGER && n->text) {
        char *end = NULL;
        unsigned long long v = strtoull(n->text, &end, 10);
        if (!end || *end != '\0') return 0;
        *out = (int64_t)(uint64_t)v;
        return 1;
    }

    if (n->kind == AST_UNARY && n->text && strcmp(n->text, "-") == 0 && n->child_count == 1) {
        int64_t v;
        if (!const_int_value(n->children[0], &v)) return 0;
        *out = (int64_t)(0u - (uint64_t)v);
        return 1;
    }

    if (n->kind == AST_BINARY && n->text && n->child_count == 2) {
        int64_t a, b;
        if (!const_int_value(n->children[0], &a) || !const_int_value(n->children[1], &b)) return 0;
        uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
        if (strcmp(n->text, "+") == 0) { *out = (int64_t)(ua + ub); return 1; }
        if (strcmp(n->text, "-") == 0) { *out = (int64_t)(ua - ub); return 1; }
        if (strcmp(n->text, "*") == 0) { *out = (int64_t)(ua * ub); return 1; }
        /* / and % fold only when they cannot fail; otherwise the runtime check stays */
        if (b != 0 && b != -1 && strcmp(n->text, "/") == 0) { *out = a / b; return 1; }
        if (b != 0 && b != -1 && strcmp(n->text, "%") == 0) { *out = a % b; return 1; }
        return 0;
    }

    return 0;
}

/* Integer / or % by a constant other than 0 and -1 can never fail. */
static int int_div_is_safe(const AstNode *n) {
    int64_t d;
    return n->kind == AST_BINARY && n->text &&
           (strcmp(n->text, "/") == 0 || strcmp(n->text, "%") == 0) && n->child_count == 2 &&
           const_int_value(n->children[1], &d) && d != 0 && d != -1;
}

static int is_int_type(const char *t) { return t && strcmp(t, "int") == 0; }

/* Builtins that can never stop the program (only int(float) and matrix() can). */
static int builtin_cannot_fail(const AstNode *call) {
    if (!call->text || call->text[0] != '@') return 0;
    if (strcmp(call->text, "@matrix") == 0) return 0;
    if (strcmp(call->text, "@int") == 0)
        return call->child_count == 1 && !(call->children[0]->type_name &&
                                           strcmp(call->children[0]->type_name, "float") == 0);
    return 1;
}

static void emit_int_literal(FILE *f, int64_t v) {
    if (v == INT64_MIN) fputs("INT64_MIN", f);
    else fprintf(f, "%lld", (long long)v);
}

/* ------------------------------------------------------------------ */
/* Loop versioning: bounds checks hoisted out of hot loops.           */
/*                                                                    */
/*   for i in range(a, b) { ... x[c0 + c1*i] ... }                    */
/*                                                                    */
/* When every such index provably stays inside its list for the whole */
/* range, the loop runs a copy that reads the raw array (Clang can    */
/* then vectorize it).  Otherwise the original checked loop runs, so  */
/* error messages and their timing are unchanged.                     */
/* ------------------------------------------------------------------ */

typedef struct {
    const AstNode *node;   /* AST_INDEX, AST_INDEX_ASSIGN or AST_MATRIX_SET made unchecked */
    const char *base;      /* list or matrix variable */
} FastAccess;

#define FAST_MAX 128
static FastAccess fast_access[FAST_MAX];
static size_t fast_access_count = 0;
static int fast_access_active = 0;

static const char *fast_access_base(const AstNode *n) {
    if (!fast_access_active) return NULL;
    for (size_t i = 0; i < fast_access_count; i++) {
        if (fast_access[i].node == n) return fast_access[i].base;
    }
    return NULL;
}

/* Functions that can never append to a list (directly or through calls). */
#define SAFE_FN_MAX 512
static const char *unsafe_fn[SAFE_FN_MAX];
static size_t unsafe_fn_count = 0;

static int fn_marked_unsafe(const char *name) {
    for (size_t i = 0; i < unsafe_fn_count; i++) {
        if (strcmp(unsafe_fn[i], name) == 0) return 1;
    }
    return 0;
}

static int call_is_safe(const AstNode *call) {
    if (!call->text) return 0;
    if (strcmp(call->text, "len") == 0 || strcmp(call->text, "input_int") == 0) return 1;
    if (unsafe_fn_count >= SAFE_FN_MAX) return 0;
    return !fn_marked_unsafe(call->text);
}

static int tree_may_grow_lists(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_LIST_APPEND) return 1;
    if (n->kind == AST_CALL && !call_is_safe(n)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (tree_may_grow_lists(n->children[i])) return 1;
    }
    return 0;
}

/*
 * Functions that may rebind an object field (`obj.f = x`), directly or
 * through calls.  A loop that reads self.w[i][j] can only hoist self->w
 * when nothing it calls can do that.
 */
static const char *field_writer_fn[SAFE_FN_MAX];
static size_t field_writer_count = 0;

static int fn_writes_fields(const char *name) {
    if (field_writer_count >= SAFE_FN_MAX) return 1;
    for (size_t i = 0; i < field_writer_count; i++) {
        if (strcmp(field_writer_fn[i], name) == 0) return 1;
    }
    return 0;
}

static int tree_may_rebind_fields(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_MEMBER_ASSIGN) return 1;
    if (n->kind == AST_CALL && n->text && n->text[0] != '@' &&
        strcmp(n->text, "len") != 0 && strcmp(n->text, "input_int") != 0 &&
        fn_writes_fields(n->text)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (tree_may_rebind_fields(n->children[i])) return 1;
    }
    return 0;
}

static void compute_field_writers(const AstNode *program) {
    field_writer_count = 0;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (size_t i = 0; i < program->child_count; i++) {
            const AstNode *fn = program->children[i];
            if (fn->kind != AST_FUNCTION || !fn->text || fn_writes_fields(fn->text)) continue;
            if (tree_may_rebind_fields(fn)) {
                if (field_writer_count < SAFE_FN_MAX) field_writer_fn[field_writer_count++] = fn->text;
                changed = 1;
            }
        }
    }
}

static void compute_safe_functions(const AstNode *program) {
    compute_field_writers(program);
    unsafe_fn_count = 0;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (size_t i = 0; i < program->child_count; i++) {
            const AstNode *fn = program->children[i];
            if (fn->kind != AST_FUNCTION || !fn->text || fn_marked_unsafe(fn->text)) continue;
            if (tree_may_grow_lists(fn)) {
                if (unsafe_fn_count < SAFE_FN_MAX) unsafe_fn[unsafe_fn_count++] = fn->text;
                changed = 1;
            }
        }
    }
}

/* Is `name` assigned or declared anywhere inside `n`? */
static int name_changes_in(const AstNode *n, const char *name) {
    if (!n) return 0;
    if ((n->kind == AST_ASSIGN || n->kind == AST_LET || n->kind == AST_FOR) &&
        n->text && strcmp(n->text, name) == 0) {
        return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (name_changes_in(n->children[i], name)) return 1;
    }
    return 0;
}

static int has_nested_loop(const AstNode *n) {
    if (!n) return 0;
    for (size_t i = 0; i < n->child_count; i++) {
        const AstNode *c = n->children[i];
        if (c->kind == AST_FOR || c->kind == AST_WHILE) return 1;
        if (has_nested_loop(c)) return 1;
    }
    return 0;
}

static int str_is_zero(const char *s) { return strcmp(s, "0") == 0; }
static int str_is_one(const char *s) { return strcmp(s, "1") == 0; }

/* Parses a decimal literal produced by affine_split (optionally negative). */
static int str_int(const char *s, int64_t *out) {
    const char *p = s;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    if (*p < '0' || *p > '9') return 0;
    uint64_t v = 0;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        v = v * 10u + (uint64_t)(*p - '0');
    }
    *out = (int64_t)(neg ? 0u - v : v);
    return 1;
}

/* Folds "a op b" when both are literals (wrapping like -fwrapv); NULL otherwise. */
static char *fold_strings(const char *a, char op, const char *b) {
    int64_t x, y;
    if (!str_int(a, &x) || !str_int(b, &y)) return NULL;
    uint64_t r = op == '+' ? (uint64_t)x + (uint64_t)y
               : op == '-' ? (uint64_t)x - (uint64_t)y
               : (uint64_t)x * (uint64_t)y;
    if ((int64_t)r == INT64_MIN) return NULL;
    return str_fmt("%lld", (long long)(int64_t)r);
}

static char *paren_if_needed(const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == ' ') return str_fmt("(%s)", s);
    }
    return str_dup(s);
}

/*
 * Splits an index expression into c0 + c1 * v, where c0 and c1 are C
 * expressions that do not change inside the loop.  Only literals,
 * loop-invariant int variables, +, - and * are accepted: they cannot
 * fail, so evaluating them before the loop is unobservable.
 */
static int affine_split(const AstNode *e, const char *v, const AstNode *body, char **c0, char **c1) {
    *c0 = *c1 = NULL;
    if (!e || !e->type_name || strcmp(e->type_name, "int") != 0) return 0;

    int64_t k;
    if (const_int_value(e, &k)) {
        if (k == INT64_MIN) return 0;
        *c0 = str_fmt("%lld", (long long)k);
        *c1 = str_dup("0");
        return *c0 && *c1;
    }

    if (e->kind == AST_IDENTIFIER && e->text) {
        if (strcmp(e->text, v) == 0) {
            *c0 = str_dup("0");
            *c1 = str_dup("1");
            return 1;
        }
        if (name_changes_in(body, e->text)) return 0;
        *c0 = str_dup(e->text);
        *c1 = str_dup("0");
        return 1;
    }

    if (e->kind == AST_UNARY && e->text && strcmp(e->text, "-") == 0 && e->child_count == 1) {
        char *a0, *a1;
        if (!affine_split(e->children[0], v, body, &a0, &a1)) return 0;
        char *p0 = paren_if_needed(a0), *p1 = paren_if_needed(a1);
        *c0 = str_is_zero(a0) ? str_dup("0") : str_fmt("-%s", p0);
        *c1 = str_is_zero(a1) ? str_dup("0") : str_fmt("-%s", p1);
        free(a0); free(a1); free(p0); free(p1);
        return 1;
    }

    if (e->kind == AST_BINARY && e->text && e->child_count == 2) {
        int add = strcmp(e->text, "+") == 0, sub = strcmp(e->text, "-") == 0;
        int mul = strcmp(e->text, "*") == 0;
        if (!add && !sub && !mul) return 0;

        char *a0, *a1, *b0, *b1;
        if (!affine_split(e->children[0], v, body, &a0, &a1)) return 0;
        if (!affine_split(e->children[1], v, body, &b0, &b1)) { free(a0); free(a1); return 0; }

        int ok = 1;
        if (add || sub) {
            const char *op = add ? "+" : "-";
            char *pb0 = paren_if_needed(b0), *pb1 = paren_if_needed(b1);
            char *f0 = fold_strings(a0, add ? '+' : '-', b0), *f1 = fold_strings(a1, add ? '+' : '-', b1);
            *c0 = f0 ? f0 : str_is_zero(b0) ? str_dup(a0)
                : str_is_zero(a0) ? (add ? str_dup(b0) : str_fmt("-%s", pb0))
                : str_fmt("%s %s %s", a0, op, pb0);
            *c1 = f1 ? f1 : str_is_zero(b1) ? str_dup(a1)
                : str_is_zero(a1) ? (add ? str_dup(b1) : str_fmt("-%s", pb1))
                : str_fmt("%s %s %s", a1, op, pb1);
            free(pb0); free(pb1);
        } else {
            /* (a0 + a1 v) * (b0 + b1 v): one side must be invariant */
            const char *k0, *x0, *x1;
            if (str_is_zero(a1)) { k0 = a0; x0 = b0; x1 = b1; }
            else if (str_is_zero(b1)) { k0 = b0; x0 = a0; x1 = a1; }
            else { ok = 0; k0 = x0 = x1 = NULL; }
            if (ok) {
                char *pk = paren_if_needed(k0), *px0 = paren_if_needed(x0), *px1 = paren_if_needed(x1);
                char *f0 = fold_strings(k0, '*', x0), *f1 = fold_strings(k0, '*', x1);
                *c0 = f0 ? f0 : (str_is_zero(k0) || str_is_zero(x0)) ? str_dup("0")
                    : str_is_one(k0) ? str_dup(x0) : str_is_one(x0) ? str_dup(k0)
                    : str_fmt("%s * %s", pk, px0);
                *c1 = f1 ? f1 : (str_is_zero(k0) || str_is_zero(x1)) ? str_dup("0")
                    : str_is_one(k0) ? str_dup(x1) : str_is_one(x1) ? str_dup(k0)
                    : str_fmt("%s * %s", pk, px1);
                free(pk); free(px0); free(px1);
            }
        }
        free(a0); free(a1); free(b0); free(b1);
        if (!ok || !*c0 || !*c1) { free(*c0); free(*c1); *c0 = *c1 = NULL; return 0; }
        return 1;
    }

    return 0;
}

typedef struct {
    const char *base;       /* name used in clair_data_<base> */
    const char *list_type;
    char *c0;
    char *c1;
    int dim;        /* 0: list index, 1: matrix row index, 2: matrix column index */
} BoundsCheck;

typedef struct {
    BoundsCheck checks[FAST_MAX];
    size_t check_count;
    FastAccess accesses[FAST_MAX];
    size_t access_count;
    const char *bases[FAST_MAX];       /* name used in clair_data_<base> */
    const char *base_exprs[FAST_MAX];  /* C expression of the list or matrix: v, self->w */
    const char *base_types[FAST_MAX];
    size_t base_count;
    char *owned[FAST_MAX * 2];         /* strings built for field chains */
    size_t owned_count;
} FastPlan;

static void fast_plan_free(FastPlan *plan) {
    for (size_t i = 0; i < plan->check_count; i++) {
        free(plan->checks[i].c0);
        free(plan->checks[i].c1);
    }
    for (size_t i = 0; i < plan->owned_count; i++) free(plan->owned[i]);
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
}

static const char *fast_plan_expr(const FastPlan *plan, const char *base) {
    for (size_t i = 0; i < plan->base_count; i++) {
        if (strcmp(plan->bases[i], base) == 0) return plan->base_exprs[i];
    }
    return base;
}

static void fast_plan_add_check(FastPlan *plan, const char *base, const char *type, char *c0, char *c1, int dim) {
    for (size_t i = 0; i < plan->check_count; i++) {
        BoundsCheck *b = &plan->checks[i];
        if (b->dim == dim && strcmp(b->base, base) == 0 && strcmp(b->c0, c0) == 0 && strcmp(b->c1, c1) == 0) {
            free(c0); free(c1);
            return;
        }
    }
    plan->checks[plan->check_count++] = (BoundsCheck){ base, type, c0, c1, dim };
}

static void fast_plan_add_base(FastPlan *plan, const char *base, const char *expr, const char *type) {
    for (size_t i = 0; i < plan->base_count; i++) {
        if (strcmp(plan->bases[i], base) == 0) return;
    }
    plan->bases[plan->base_count] = base;
    plan->base_exprs[plan->base_count] = expr;
    plan->base_types[plan->base_count++] = type;
}

/* m[r][c]: both indices proven in range, the access becomes data[r * cols + c] */
static void fast_plan_add_matrix(FastPlan *plan, const AstNode *node, const char *base, const char *expr,
                                 const char *type, char *r0, char *r1, char *k0, char *k1) {
    if (plan->access_count >= FAST_MAX || plan->check_count + 2 > FAST_MAX || plan->base_count >= FAST_MAX) {
        free(r0); free(r1); free(k0); free(k1);
        return;
    }
    plan->accesses[plan->access_count++] = (FastAccess){ node, base };
    fast_plan_add_base(plan, base, expr, type);
    fast_plan_add_check(plan, base, type, r0, r1, 1);
    fast_plan_add_check(plan, base, type, k0, k1, 2);
}

static void fast_plan_add(FastPlan *plan, const AstNode *node, const char *base, const char *expr,
                          const char *list_type, char *c0, char *c1) {
    if (plan->access_count >= FAST_MAX || plan->check_count >= FAST_MAX || plan->base_count >= FAST_MAX) {
        free(c0); free(c1);
        return;
    }
    plan->accesses[plan->access_count++] = (FastAccess){ node, base };
    fast_plan_add_base(plan, base, expr, list_type);
    fast_plan_add_check(plan, base, list_type, c0, c1, 0);
}

/* Does the loop body call a function that may reassign an object field? */
static int tree_calls_field_writer(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_CALL && n->text && n->text[0] != '@' &&
        strcmp(n->text, "len") != 0 && strcmp(n->text, "input_int") != 0 &&
        fn_writes_fields(n->text)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (tree_calls_field_writer(n->children[i])) return 1;
    }
    return 0;
}

static int tree_assigns_field(const AstNode *n, const char *field) {
    if (!n) return 0;
    if (n->kind == AST_MEMBER_ASSIGN && n->text && strcmp(n->text, field) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (tree_assigns_field(n->children[i], field)) return 1;
    }
    return 0;
}

/*
 * Resolves the list or matrix being indexed to a stable base: a variable the
 * body never reassigns, or a field chain (self.w, net.layer.b) that the body
 * cannot rebind (no assignment to those fields, no call to a function that
 * assigns fields).
 * Returns 1 and sets the clair_data_<name> suffix and the C expression.
 */
static int fast_base_of(FastPlan *plan, const AstNode *e, const char *v, const AstNode *body,
                        const char **name, const char **expr) {
    if (e->kind == AST_IDENTIFIER && e->text) {
        if (name_changes_in(body, e->text) || strcmp(e->text, v) == 0) return 0;
        *name = *expr = e->text;
        return 1;
    }
    if (e->kind != AST_MEMBER || plan->owned_count + 2 > FAST_MAX * 2) return 0;
    if (tree_calls_field_writer(body)) return 0;

    char suffix[256] = "", cexpr[256] = "";
    const AstNode *chain[32];
    size_t depth = 0;
    const AstNode *cur = e;
    while (cur->kind == AST_MEMBER && cur->child_count == 1 && cur->text && depth < 32) {
        if (tree_assigns_field(body, cur->text)) return 0;
        chain[depth++] = cur;
        cur = cur->children[0];
    }
    if (cur->kind != AST_IDENTIFIER || !cur->text || depth == 32) return 0;
    if (name_changes_in(body, cur->text) || strcmp(cur->text, v) == 0) return 0;

    snprintf(suffix, sizeof(suffix), "%s", cur->text);
    snprintf(cexpr, sizeof(cexpr), "%s", cur->text);
    for (size_t i = depth; i > 0; i--) {
        size_t sl = strlen(suffix), cl = strlen(cexpr);
        snprintf(suffix + sl, sizeof(suffix) - sl, "__%s", chain[i - 1]->text);
        snprintf(cexpr + cl, sizeof(cexpr) - cl, "->%s", chain[i - 1]->text);
    }
    char *ns = str_dup(suffix), *ne = str_dup(cexpr);
    if (!ns || !ne) { free(ns); free(ne); return 0; }
    plan->owned[plan->owned_count++] = ns;
    plan->owned[plan->owned_count++] = ne;
    *name = ns;
    *expr = ne;
    return 1;
}

static void fast_plan_collect(FastPlan *plan, const AstNode *n, const char *v, const AstNode *body) {
    if (!n) return;

    /* list read x[i] / write x[i] = value (x a variable or a field chain) */
    const AstNode *index = NULL, *base_node = NULL;
    const char *list_type = NULL;
    if (n->kind == AST_INDEX && n->child_count == 2 && is_list_type(n->children[0]->type_name)) {
        base_node = n->children[0];
        list_type = n->children[0]->type_name;
        index = n->children[1];
    } else if (n->kind == AST_INDEX_ASSIGN && is_list_type(n->type_name) &&
               ((n->child_count == 2 && n->text) || (n->child_count == 3 && !n->text))) {
        list_type = n->type_name;
        index = n->children[0];
        base_node = n->text ? NULL : n->children[2];
    }
    if (list_type) {
        const char *name = NULL, *expr = NULL;
        int ok = base_node ? fast_base_of(plan, base_node, v, body, &name, &expr)
                           : (!name_changes_in(body, n->text) && strcmp(n->text, v) != 0 &&
                              (name = expr = n->text) != NULL);
        char *c0, *c1;
        if (ok && affine_split(index, v, body, &c0, &c1)) fast_plan_add(plan, n, name, expr, list_type, c0, c1);
    }

    /* matrix read m[r][c] or write m[r][c] = value */
    const AstNode *row_index = NULL, *col_index = NULL, *mnode = NULL;
    const char *mtype = NULL;
    int mvar = 0;
    if (n->kind == AST_INDEX && n->child_count == 2 && n->children[0]->kind == AST_INDEX &&
        is_matrix_row_type(n->children[0]->type_name)) {
        mnode = n->children[0]->children[0];
        mtype = mnode->type_name;
        row_index = n->children[0]->children[1];
        col_index = n->children[1];
    } else if (n->kind == AST_MATRIX_SET && is_matrix_type(n->type_name) &&
               ((n->child_count == 3 && n->text) || (n->child_count == 4 && !n->text))) {
        mvar = n->text != NULL;
        mnode = mvar ? NULL : n->children[3];
        mtype = n->type_name;
        row_index = n->children[0];
        col_index = n->children[1];
    }
    if (mtype) {
        const char *name = NULL, *expr = NULL;
        int ok = mnode ? fast_base_of(plan, mnode, v, body, &name, &expr)
                       : (!name_changes_in(body, n->text) && strcmp(n->text, v) != 0 &&
                          (name = expr = n->text) != NULL);
        char *r0, *r1, *k0, *k1;
        if (ok && affine_split(row_index, v, body, &r0, &r1)) {
            if (affine_split(col_index, v, body, &k0, &k1)) {
                fast_plan_add_matrix(plan, n, name, expr, mtype, r0, r1, k0, k1);
            } else {
                free(r0); free(r1);
            }
        }
    }

    for (size_t i = 0; i < n->child_count; i++) fast_plan_collect(plan, n->children[i], v, body);
}

/* Builds the plan for one `for` loop; returns 0 when the loop must stay checked only. */
static int fast_plan_for(const AstNode *loop, FastPlan *plan) {
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
    if (!loop || loop->kind != AST_FOR || !loop->text) return 0;
    if (loop->child_count != 2 && loop->child_count != 3) return 0;  /* step must be 1 */

    const AstNode *body = loop->children[loop->child_count - 1];
    if (has_nested_loop(body)) return 0;          /* innermost loops only: keeps code size small */
    if (tree_may_grow_lists(body)) return 0;      /* append/realloc would move the data */
    if (name_changes_in(body, loop->text)) return 0;

    fast_plan_collect(plan, body, loop->text, body);
    if (plan->access_count == 0) { fast_plan_free(plan); return 0; }
    return 1;
}

static int program_has_fast_loop(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_FOR) {
        FastPlan plan;
        int ok = fast_plan_for(n, &plan);
        fast_plan_free(&plan);
        if (ok) return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (program_has_fast_loop(n->children[i])) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Expression precedence, so the C reads like hand-written code       */
/* (parentheses only where C needs them).                             */
/* ------------------------------------------------------------------ */

enum {
    PREC_OR = 4, PREC_AND = 5, PREC_EQUALITY = 9, PREC_RELATIONAL = 10,
    PREC_ADDITIVE = 12, PREC_MULTIPLICATIVE = 13, PREC_UNARY = 14, PREC_POSTFIX = 16
};

static int is_string_type(const char *t) { return t && strcmp(t, "string") == 0; }

static int expr_prec(const AstNode *n) {
    int64_t k;
    switch (n->kind) {
        case AST_INDEX:
            return fast_access_base(n) ? PREC_POSTFIX : PREC_UNARY;  /* `*accessor(...)` */
        case AST_CALL:
            if (n->text && strcmp(n->text, "len") == 0) return PREC_UNARY;    /* (int64_t)x->len */
            if (n->text && strcmp(n->text, "@float") == 0) return PREC_UNARY; /* (double)x */
            if (n->text && strcmp(n->text, "@int") == 0 && builtin_cannot_fail(n)) return PREC_UNARY;
            return PREC_POSTFIX;
        case AST_UNARY:
            if (const_int_value(n, &k) && k >= 0) return PREC_POSTFIX;
            return PREC_UNARY;
        case AST_BINARY: {
            if (const_int_value(n, &k)) return k >= 0 ? PREC_POSTFIX : PREC_UNARY;
            const char *op = n->text;
            const char *lt = n->children[0]->type_name, *rt = n->children[1]->type_name;
            if (strcmp(op, "or") == 0) return PREC_OR;
            if (strcmp(op, "and") == 0) return PREC_AND;
            if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) return PREC_EQUALITY;
            if (strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
                strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) return PREC_RELATIONAL;
            if (strcmp(op, "+") == 0 && is_string_type(lt) && is_string_type(rt)) return PREC_POSTFIX;
            if ((strcmp(op, "/") == 0 || strcmp(op, "%") == 0) && is_int_type(n->type_name) &&
                !int_div_is_safe(n)) return PREC_POSTFIX;                  /* checked helper call */
            if (strcmp(op, "%") == 0 && !is_int_type(n->type_name)) return PREC_POSTFIX;  /* fmod() */
            if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return PREC_ADDITIVE;
            return PREC_MULTIPLICATIVE;
        }
        default:
            return PREC_POSTFIX;
    }
}

static int is_comparison_prec(int p) { return p == PREC_EQUALITY || p == PREC_RELATIONAL; }


static int stack_list_node_contains(
    const AstNode *root,
    const AstNode *target
) {
    if (!root) return 0;
    if (root == target) return 1;

    for (size_t i = 0; i < root->child_count; i++) {
        if (stack_list_node_contains(root->children[i], target)) {
            return 1;
        }
    }

    return 0;
}

static const AstNode *stack_list_find_function(
    const AstNode *decl
) {
    if (!codegen_program || !decl) return NULL;

    for (size_t i = 0; i < codegen_program->child_count; i++) {
        const AstNode *child = codegen_program->children[i];
        if (child->kind != AST_FUNCTION) continue;

        if (stack_list_node_contains(child, decl)) {
            return child;
        }
    }

    return NULL;
}

/*
 * STACK LISTS V3
 *
 * Scope-aware local alias tracking.
 *
 * V2 followed variable spellings across the complete function.
 * V3 follows the lexical binding introduced by one exact AST_LET.
 *
 * Rules mirrored from semantic.c:
 *
 *   - a let initializer is evaluated before its new binding exists;
 *   - if/else bodies introduce scopes;
 *   - while bodies introduce scopes;
 *   - for introduces a scope containing its loop variable and body;
 *   - leaving a nested block restores the previous outer binding;
 *   - safe local list aliases may share stack-backed storage.
 */

static int stack_list_binding_uses_are_safe(
    const AstNode *decl
);

static int stack_list_block_uses_are_safe(
    const AstNode *block,
    const AstNode *decl,
    const char *name,
    int visible
);

static int stack_list_expr_uses_are_safe(
    const AstNode *n,
    const char *name,
    int visible,
    const AstNode *parent,
    size_t child_index)
{
    if (!n) return 1;
    if (!visible) return 1;
    if (!name) return 0;

    if (n->kind == AST_IDENTIFIER &&
        n->text &&
        strcmp(n->text, name) == 0) {

        /*
         * Safe read:
         *
         *     values[i]
         */
        if (parent &&
            parent->kind == AST_INDEX &&
            child_index == 0) {
            return 1;
        }

        /*
         * Safe observation:
         *
         *     len(values)
         */
        if (parent &&
            parent->kind == AST_CALL &&
            parent->text &&
            strcmp(parent->text, "len") == 0 &&
            parent->child_count == 1 &&
            child_index == 0) {
            return 1;
        }

        /*
         * Any other direct expression use can escape the pointer
         * or create an ownership path unsupported by stack storage.
         *
         * Alias declarations are handled specially by the block walker.
         */
        return 0;
    }

    for (size_t i = 0; i < n->child_count; i++) {
        if (!stack_list_expr_uses_are_safe(
                n->children[i],
                name,
                visible,
                n,
                i)) {
            return 0;
        }
    }

    return 1;
}

static int stack_list_stmt_uses_are_safe(
    const AstNode *n,
    const AstNode *decl,
    const char *name,
    int visible)
{
    if (!n) return 1;

    switch (n->kind) {
        case AST_IF:
            if (n->child_count != 2 &&
                n->child_count != 3) {
                return 0;
            }

            if (!stack_list_expr_uses_are_safe(
                    n->children[0],
                    name,
                    visible,
                    n,
                    0)) {
                return 0;
            }

            if (!stack_list_block_uses_are_safe(
                    n->children[1],
                    decl,
                    name,
                    visible)) {
                return 0;
            }

            if (n->child_count == 3 &&
                !stack_list_block_uses_are_safe(
                    n->children[2],
                    decl,
                    name,
                    visible)) {
                return 0;
            }

            return 1;

        case AST_WHILE:
            if (n->child_count != 2) {
                return 0;
            }

            if (!stack_list_expr_uses_are_safe(
                    n->children[0],
                    name,
                    visible,
                    n,
                    0)) {
                return 0;
            }

            return stack_list_block_uses_are_safe(
                n->children[1],
                decl,
                name,
                visible
            );

        case AST_FOR: {
            if (!n->text ||
                n->child_count < 2 ||
                n->child_count > 4) {
                return 0;
            }

            size_t argc = n->child_count - 1;

            /*
             * range(...) arguments are evaluated before the loop
             * variable enters scope.
             */
            for (size_t i = 0; i < argc; i++) {
                if (!stack_list_expr_uses_are_safe(
                        n->children[i],
                        name,
                        visible,
                        n,
                        i)) {
                    return 0;
                }
            }

            int body_visible = visible;

            /*
             * The loop variable shadows an outer binding having the
             * same spelling for the entire loop body.
             */
            if (strcmp(n->text, name) == 0) {
                body_visible = 0;
            }

            return stack_list_block_uses_are_safe(
                n->children[n->child_count - 1],
                decl,
                name,
                body_visible
            );
        }

        case AST_ASSIGN:
            /*
             * Rebinding the tracked binding changes the object
             * associated with this variable.
             *
             * If visible == 0, the assignment refers to a shadowing
             * declaration and is unrelated to the tracked binding.
             */
            if (visible &&
                n->text &&
                strcmp(n->text, name) == 0) {
                return 0;
            }
            break;

        case AST_LIST_APPEND:
            /*
             * append() can increase capacity/reallocate storage.
             * Stack-backed list storage therefore cannot be used.
             */
            if (visible &&
                n->text &&
                strcmp(n->text, name) == 0) {
                return 0;
            }
            break;

        case AST_LET:
            /*
             * Declarations must be handled sequentially by the block
             * walker because they change lexical bindings.
             */
            return 0;

        default:
            break;
    }

    /*
     * AST_INDEX_ASSIGN intentionally does not reject an assignment
     * merely because n->text is the tracked list.
     *
     *     values[i] = x
     *
     * mutates an existing element without changing list capacity.
     *
     * Expressions used as index/value are still recursively checked.
     */
    for (size_t i = 0; i < n->child_count; i++) {
        if (!stack_list_expr_uses_are_safe(
                n->children[i],
                name,
                visible,
                n,
                i)) {
            return 0;
        }
    }

    return 1;
}

/*
 * Sequential lexical block traversal.
 *
 * visible == 1 means references having spelling `name` currently
 * resolve to the exact AST_LET declaration being analysed.
 *
 * Since visible is passed by value, returning from a nested block
 * automatically restores the outer binding.
 */
static int stack_list_block_uses_are_safe(
    const AstNode *block,
    const AstNode *decl,
    const char *name,
    int visible)
{
    if (!block || block->kind != AST_BLOCK) return 0;
    if (!decl || !name) return 0;

    for (size_t i = 0; i < block->child_count; i++) {
        const AstNode *stmt = block->children[i];

        if (!stmt) return 0;

        if (stmt->kind == AST_LET) {
            if (!stmt->text ||
                !stmt->type_name ||
                stmt->child_count != 1) {
                return 0;
            }

            const AstNode *init = stmt->children[0];

            /*
             * Exact declaration being tracked.
             *
             * Its initializer executes before the new declaration is
             * visible.  Afterwards this precise binding becomes active.
             */
            if (stmt == decl) {
                if (!stack_list_expr_uses_are_safe(
                        init,
                        name,
                        visible,
                        stmt,
                        0)) {
                    return 0;
                }

                visible = 1;
                continue;
            }

            /*
             * Local list alias:
             *
             *     let alias: list[int] = values
             *
             * The alias must itself satisfy the complete lexical
             * safety analysis.
             *
             * This also correctly supports:
             *
             *     let values: list[int] = values
             *
             * in a nested block: the RHS resolves to the outer
             * binding before the new local binding shadows it.
             */
            if (visible &&
                init &&
                init->kind == AST_IDENTIFIER &&
                init->text &&
                strcmp(init->text, name) == 0) {

                if (!is_list_type(stmt->type_name) ||
                    !decl->type_name ||
                    strcmp(stmt->type_name,
                           decl->type_name) != 0) {
                    return 0;
                }

                if (!stack_list_binding_uses_are_safe(stmt)) {
                    return 0;
                }
            } else {
                if (!stack_list_expr_uses_are_safe(
                        init,
                        name,
                        visible,
                        stmt,
                        0)) {
                    return 0;
                }
            }

            /*
             * Any declaration having the same spelling shadows the
             * tracked binding for all following statements in this block.
             */
            if (strcmp(stmt->text, name) == 0) {
                visible = 0;
            }

            continue;
        }

        if (!stack_list_stmt_uses_are_safe(
                stmt,
                decl,
                name,
                visible)) {
            return 0;
        }
    }

    return 1;
}

/*
 * Analyse the complete lexical lifetime of one exact AST_LET.
 */
static int stack_list_binding_uses_are_safe(
    const AstNode *decl)
{
    if (!decl ||
        decl->kind != AST_LET ||
        !decl->text ||
        !decl->type_name ||
        !is_list_type(decl->type_name)) {
        return 0;
    }

    const AstNode *fn = stack_list_find_function(decl);

    if (!fn ||
        fn->kind != AST_FUNCTION ||
        fn->child_count == 0) {
        return 0;
    }

    const AstNode *body =
        fn->children[fn->child_count - 1];

    if (!body || body->kind != AST_BLOCK) {
        return 0;
    }

    /*
     * Start invisible.  Sequential traversal activates the binding
     * exactly when it reaches `decl`.
     */
    return stack_list_block_uses_are_safe(
        body,
        decl,
        decl->text,
        0
    );
}

static int stack_list_decl_is_safe(const AstNode *n) {
    if (!n || n->kind != AST_LET) return 0;
    if (!n->text || !n->type_name) return 0;
    if (!is_list_type(n->type_name)) return 0;
    if (n->child_count != 1) return 0;

    const AstNode *init = n->children[0];

    /*
     * Stack allocation currently applies only to an actual list
     * literal declaration, not an alias declaration itself.
     */
    if (!init || init->kind != AST_LIST) return 0;

    /*
     * Empty lists remain heap-backed because there is no fixed
     * non-zero backing array to expose.
     */
    if (init->child_count == 0) return 0;

    return stack_list_binding_uses_are_safe(n);
}

static void emit_c_string(FILE *f, const char *s) {
    fputc('"', f);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '\\': fputs("\\\\", f); break;
            case '"': fputs("\\\"", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default: fputc(c, f); break;
        }
    }
    fputc('"', f);
}

static size_t function_param_count(const AstNode *fn) {
    if (fn->child_count == 0) return 0;
    return fn->child_count - 1;
}

static const AstNode *function_body(const AstNode *fn) {
    if (fn->child_count == 0) return NULL;
    return fn->children[fn->child_count - 1];
}

static int emit_param_list(FILE *f, const AstNode *fn) {
    size_t count = function_param_count(fn);
    if (count == 0) {
        fputs("void", f);
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        const AstNode *param = fn->children[i];
        if (param->kind != AST_PARAM || !param->text || !param->type_name) return 0;
        const char *type = c_type_name(param->type_name);
        if (!type) return 0;
        if (i != 0) fputs(", ", f);
        fprintf(f, "%s %s", type, param->text);
    }
    return 1;
}

static int emit_expr(FILE *f, const AstNode *n);

static int is_negation(const AstNode *n) {
    int64_t k;
    if (const_int_value(n, &k)) return k < 0;
    return n->kind == AST_UNARY && n->text && strcmp(n->text, "-") == 0;
}

/* Emits a child expression, adding parentheses only when C needs them. */
static int emit_operand(FILE *f, const AstNode *child, int parent_prec, int is_right) {
    int prec = expr_prec(child);
    int wrap = prec < parent_prec ||
               (is_right && prec == parent_prec) ||                       /* a - (b - c), a + (b + c) */
               (is_comparison_prec(prec) && is_comparison_prec(parent_prec)) ||
               ((prec == PREC_AND || prec == PREC_OR) &&                  /* a || (b && c) */
                (parent_prec == PREC_AND || parent_prec == PREC_OR) && prec != parent_prec) ||
               (is_right && parent_prec == PREC_ADDITIVE && is_negation(child));  /* a - (-b) */
    if (wrap) fputc('(', f);
    if (!emit_expr(f, child)) return 0;
    if (wrap) fputc(')', f);
    return 1;
}

static const char *list_accessor_name(const char *list_type) {
    if (!list_type) return NULL;
    if (strcmp(list_type, "list[int]") == 0) return "clair_rt_list_at_i64_checked";
    if (strcmp(list_type, "list[float]") == 0) return "clair_rt_list_at_f64_checked";
    if (strcmp(list_type, "list[bool]") == 0) return "clair_rt_list_at_bool_checked";
    if (strcmp(list_type, "list[string]") == 0) return "clair_rt_list_at_str_checked";
    return NULL;
}

static int emit_expr(FILE *f, const AstNode *n) {
    switch (n->kind) {
        case AST_INTEGER:
            fputs(n->text, f);
            return 1;

        case AST_FLOAT:
            fputs(n->text, f);
            return 1;

        case AST_BOOL:
            if (!n->text) return 0;
            if (strcmp(n->text, "true") == 0) fputs("true", f);
            else if (strcmp(n->text, "false") == 0) fputs("false", f);
            else return 0;
            return 1;

        case AST_IDENTIFIER:
            fputs(n->text, f);
            return 1;

        case AST_STRING:
            emit_c_string(f, n->text);
            return 1;

        case AST_LIST: {
            if (!n->type_name || !is_list_type(n->type_name)) return 0;
            const char *element_type = list_element_c_type(n->type_name);
            if (!element_type) return 0;
            if (n->child_count == 0) {
                fprintf(f, "clair_rt_list_make(NULL, 0, sizeof(%s))", element_type);
                return 1;
            }
            fprintf(f, "clair_rt_list_make((%s[]){", element_type);
            for (size_t i = 0; i < n->child_count; i++) {
                if (i != 0) fputs(", ", f);
                if (!emit_expr(f, n->children[i])) return 0;
            }
            fprintf(f, "}, %zu, sizeof(%s))", n->child_count, element_type);
            return 1;
        }

        case AST_DICT: {
            if (!n->type_name || !is_dict_type(n->type_name) ||
                (n->child_count % 2) != 0) return 0;
            const char *value_type = dict_value_c_type(n->type_name);
            if (!value_type) return 0;
            size_t count = n->child_count / 2;

            if (count == 0) {
                fprintf(f, "clair_rt_dict_make(NULL, NULL, 0, sizeof(%s))", value_type);
                return 1;
            }

            fputs("clair_rt_dict_make((const char *[]){", f);
            for (size_t i = 0; i < count; i++) {
                if (i != 0) fputs(", ", f);
                if (!emit_expr(f, n->children[i * 2])) return 0;
            }
            fprintf(f, "}, (%s[]){", value_type);
            for (size_t i = 0; i < count; i++) {
                if (i != 0) fputs(", ", f);
                if (!emit_expr(f, n->children[i * 2 + 1])) return 0;
            }
            fprintf(f, "}, %zu, sizeof(%s))", count, value_type);
            return 1;
        }

        case AST_INDEX: {
            if (n->child_count != 2 || !n->type_name || !n->children[0]->type_name) return 0;

            if (is_matrix_row_type(n->children[0]->type_name)) {
                const AstNode *row = n->children[0];
                const char *fast_base = fast_access_base(n);
                if (fast_base) {
                    /* both indices proven in range: plain row-major access */
                    int64_t col0;
                    fprintf(f, "clair_data_%s[", fast_base);
                    if (!emit_operand(f, row->children[1], PREC_MULTIPLICATIVE, 0)) return 0;
                    fprintf(f, " * clair_cols_%s", fast_base);
                    if (!(const_int_value(n->children[1], &col0) && col0 == 0)) {
                        fputs(" + ", f);
                        if (!emit_operand(f, n->children[1], PREC_ADDITIVE, 1)) return 0;
                    }
                    fputc(']', f);
                    return 1;
                }
                fprintf(f, "*%s(", matrix_accessor_name(row->type_name));
                if (!emit_expr(f, row->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, row->children[1])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                return 1;
            }
            const char *element_type = c_type_name(n->type_name);
            if (!element_type || is_list_type(n->type_name) || is_dict_type(n->type_name)) return 0;

            if (is_list_type(n->children[0]->type_name)) {
                const char *fast_base = fast_access_base(n);
                if (fast_base) {
                    /* bounds already proven before the loop: plain array access */
                    fprintf(f, "clair_data_%s[", fast_base);
                    if (!emit_expr(f, n->children[1])) return 0;
                    fputc(']', f);
                    return 1;
                }
                const char *accessor = list_accessor_name(n->children[0]->type_name);
                if (!accessor) return 0;
                fprintf(f, "*%s(", accessor);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                return 1;
            }

            if (is_dict_type(n->children[0]->type_name)) {
                fprintf(f, "(*(%s *)clair_rt_dict_at(", element_type);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs("))", f);
                return 1;
            }

            return 0;
        }

        case AST_CONSTRUCT: {
            if (!n->text || !n->type_name) return 0;
            fprintf(f, "((ClairClass_%s *)clair_rt_object_make(&(ClairClass_%s){",
                    n->text, n->text);
            if (n->child_count == 0) {
                fputs("0", f);
            } else {
                for (size_t i = 0; i < n->child_count; i++) {
                    if (i != 0) fputs(", ", f);
                    if (!emit_expr(f, n->children[i])) return 0;
                }
            }
            fprintf(f, "}, sizeof(ClairClass_%s)))", n->text);
            return 1;
        }

        case AST_MEMBER:
            if (n->child_count != 1 || !n->text) return 0;
            if (!emit_operand(f, n->children[0], PREC_POSTFIX, 0)) return 0;
            fprintf(f, "->%s", n->text);
            return 1;

        case AST_CALL:
            if (n->text && n->text[0] == '@') {
                const char *name = n->text + 1;
                if (strcmp(name, "matrix") == 0) {
                    fprintf(f, "clair_rt_matrix_make(");
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, n->children[1])) return 0;
                    fprintf(f, ", sizeof(%s))", matrix_element_c_type(n->type_name));
                    return 1;
                }
                if (strcmp(name, "rows") == 0 || strcmp(name, "cols") == 0) {
                    if (!emit_operand(f, n->children[0], PREC_POSTFIX, 0)) return 0;
                    fprintf(f, "->%s", name);
                    return 1;
                }
                const char *t0 = n->child_count > 0 ? n->children[0]->type_name : NULL;
                if (strcmp(name, "float") == 0) {
                    fputs("(double)", f);
                    return emit_operand(f, n->children[0], PREC_UNARY, 0);
                }
                if (strcmp(name, "int") == 0) {
                    if (t0 && strcmp(t0, "float") == 0) {
                        fputs("clair_rt_float_to_int(", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputc(')', f);
                        return 1;
                    }
                    fputs("(int64_t)", f);
                    return emit_operand(f, n->children[0], PREC_UNARY, 0);
                }
                const char *c_name = name;
                if (strcmp(name, "abs") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_abs_i64" : "fabs";
                if (strcmp(name, "min") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_min_i64" : "fmin";
                if (strcmp(name, "max") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_max_i64" : "fmax";
                fprintf(f, "%s(", c_name);
                for (size_t i = 0; i < n->child_count; i++) {
                    if (i != 0) fputs(", ", f);
                    if (!emit_expr(f, n->children[i])) return 0;
                }
                fputc(')', f);
                return 1;
            }
            if (n->text && strcmp(n->text, "input_int") == 0) {
                if (n->child_count != 0) return 0;
                fputs("clair_rt_input_int()", f);
                return 1;
            }
            if (n->text && strcmp(n->text, "len") == 0) {
                if (n->child_count != 1 || !n->children[0]->type_name) return 0;
                if (strcmp(n->children[0]->type_name, "string") == 0) {
                    fputs("(int64_t)strlen(", f);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputc(')', f);
                    return 1;
                }
                if (is_list_type(n->children[0]->type_name) || is_dict_type(n->children[0]->type_name)) {
                    fputs("(int64_t)", f);
                    if (!emit_operand(f, n->children[0], PREC_POSTFIX, 0)) return 0;
                    fputs("->len", f);
                    return 1;
                }
                return 0;
            }
            fprintf(f, "clair_fn_%s(", n->text);
            for (size_t i = 0; i < n->child_count; i++) {
                if (i != 0) fputs(", ", f);
                if (!emit_expr(f, n->children[i])) return 0;
            }
            fputc(')', f);
            return 1;

        case AST_BINARY: {
            if (n->child_count != 2 || !n->text) return 0;
            const char *op = n->text;

            int64_t folded;
            if (const_int_value(n, &folded)) {
                emit_int_literal(f, folded);
                return 1;
            }


            if (strcmp(op, "%") == 0) {
                if (!is_int_type(n->type_name)) {
                    fputs("fmod(", f);
                } else if (int_div_is_safe(n)) {
                    if (!emit_operand(f, n->children[0], PREC_MULTIPLICATIVE, 0)) return 0;
                    fputs(" % ", f);
                    return emit_operand(f, n->children[1], PREC_MULTIPLICATIVE, 1);
                } else {
                    fputs("clair_rt_int_mod_checked(", f);
                }
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                return 1;
            }

            if (strcmp(op, "/") == 0 &&
                n->type_name &&
                strcmp(n->type_name, "int") == 0 &&
                n->children[0] && n->children[1] &&
                n->children[0]->type_name &&
                n->children[1]->type_name &&
                strcmp(n->children[0]->type_name, "int") == 0 &&
                strcmp(n->children[1]->type_name, "int") == 0) {
                if (int_div_is_safe(n)) {
                    /* constant divisor other than 0 and -1: cannot fail */
                    if (!emit_operand(f, n->children[0], PREC_MULTIPLICATIVE, 0)) return 0;
                    fputs(" / ", f);
                    return emit_operand(f, n->children[1], PREC_MULTIPLICATIVE, 1);
                }
                fputs("clair_rt_int_div_checked(", f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                return 1;
            }

if (strcmp(op, "+") == 0 &&
                n->children[0]->type_name && n->children[1]->type_name &&
                strcmp(n->children[0]->type_name, "string") == 0 &&
                strcmp(n->children[1]->type_name, "string") == 0) {
                fputs("clair_rt_string_concat(", f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                return 1;
            }

            if ((strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) &&
                n->children[0]->type_name && n->children[1]->type_name &&
                strcmp(n->children[0]->type_name, "string") == 0 &&
                strcmp(n->children[1]->type_name, "string") == 0) {
                fputs("strcmp(", f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fprintf(f, ") %s 0", strcmp(op, "==") == 0 ? "==" : "!=");
                return 1;
            }

            int prec = expr_prec(n);
            if (strcmp(op, "and") == 0) op = "&&";
            else if (strcmp(op, "or") == 0) op = "||";

            if (!emit_operand(f, n->children[0], prec, 0)) return 0;
            fprintf(f, " %s ", op);
            if (!emit_operand(f, n->children[1], prec, 1)) return 0;
            return 1;
        }

        case AST_UNARY: {
            if (n->child_count != 1 || !n->text) return 0;
            int64_t folded;
            if (const_int_value(n, &folded)) {
                emit_int_literal(f, folded);
                return 1;
            }
            const char *op = strcmp(n->text, "not") == 0 ? "!" : n->text;
            fputs(op, f);
            const AstNode *operand = n->children[0];
            /* `- -x` would read as a decrement; keep `-(-x)` */
            int wrap = expr_prec(operand) < PREC_UNARY || operand->kind == AST_UNARY;
            if (wrap) fputc('(', f);
            if (!emit_expr(f, operand)) return 0;
            if (wrap) fputc(')', f);
            return 1;
        }

        default:
            return 0;
    }
}

static size_t range_counter = 0;
static size_t list_mutation_counter = 0;
static size_t shadow_init_counter = 0;
static size_t stack_list_counter = 0;


static int clair_simd_v4_identifier(
    const AstNode *n, const char *name
) {
    return n && n->kind == AST_IDENTIFIER &&
           n->text && strcmp(n->text, name) == 0;
}

static int clair_simd_v4_one(const AstNode *n) {
    return n && n->kind == AST_INTEGER &&
           n->text && strcmp(n->text, "1") == 0;
}

static int clair_simd_v4_match_map(
    const AstNode *n,
    const char **list_out,
    const char **index_out
) {
    if (!n || n->kind != AST_WHILE ||
        n->child_count != 2) return 0;

    const AstNode *cond = n->children[0];
    const AstNode *body = n->children[1];

    if (!cond || cond->kind != AST_BINARY ||
        cond->child_count != 2 ||
        !cond->text || strcmp(cond->text, "<") != 0)
        return 0;

    const AstNode *index = cond->children[0];
    const AstNode *len = cond->children[1];

    if (!index || index->kind != AST_IDENTIFIER ||
        !index->text || !index->type_name ||
        strcmp(index->type_name, "int") != 0)
        return 0;

    if (!len || len->kind != AST_CALL ||
        !len->text || strcmp(len->text, "len") != 0 ||
        len->child_count != 1)
        return 0;

    const AstNode *list = len->children[0];

    if (!list || list->kind != AST_IDENTIFIER ||
        !list->text || !list->type_name ||
        strcmp(list->type_name, "list[int]") != 0)
        return 0;

    if (!body || body->kind != AST_BLOCK ||
        body->child_count != 2)
        return 0;

    const AstNode *write = body->children[0];
    const AstNode *step = body->children[1];

    if (!write || write->kind != AST_INDEX_ASSIGN ||
        !write->type_name ||
        strcmp(write->type_name, "list[int]") != 0 ||
        !write->text ||
        strcmp(write->text, list->text) != 0 ||
        write->child_count != 2)
        return 0;

    if (!clair_simd_v4_identifier(
            write->children[0], index->text))
        return 0;

    const AstNode *value = write->children[1];

    if (!value || value->kind != AST_BINARY ||
        !value->text || strcmp(value->text, "+") != 0 ||
        value->child_count != 2 ||
        !clair_simd_v4_one(value->children[1]))
        return 0;

    const AstNode *read = value->children[0];

    if (!read || read->kind != AST_INDEX ||
        read->child_count != 2 ||
        !read->type_name ||
        strcmp(read->type_name, "int") != 0 ||
        !clair_simd_v4_identifier(
            read->children[0], list->text) ||
        !clair_simd_v4_identifier(
            read->children[1], index->text))
        return 0;

    if (!step || step->kind != AST_ASSIGN ||
        !step->text ||
        strcmp(step->text, index->text) != 0 ||
        step->child_count != 1)
        return 0;

    const AstNode *increment = step->children[0];

    if (!increment ||
        increment->kind != AST_BINARY ||
        !increment->text ||
        strcmp(increment->text, "+") != 0 ||
        increment->child_count != 2 ||
        !clair_simd_v4_identifier(
            increment->children[0], index->text) ||
        !clair_simd_v4_one(increment->children[1]))
        return 0;

    *list_out = list->text;
    *index_out = index->text;
    return 1;
}

static size_t clair_simd_v4_loop_counter = 0;

static int emit_stmt(FILE *f, const AstNode *n, int depth);

/* Emits the target of a write: the variable in `text`, or the field chain stored as an extra last child. */
static int emit_write_target(FILE *f, const AstNode *n, size_t plain_children) {
    if (n->text) {
        fputs(n->text, f);
        return 1;
    }
    if (n->child_count != plain_children + 1) return 0;
    return emit_operand(f, n->children[plain_children], PREC_POSTFIX, 0);
}

/* Can evaluating `n` stop the program or run user code (calls, bounds, division)? */
static int node_may_fail(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_CALL && !(n->text && strcmp(n->text, "len") == 0) && !builtin_cannot_fail(n)) return 1;
    if (n->kind == AST_INDEX) return 1;
    if (n->kind == AST_BINARY && n->text && (strcmp(n->text, "/") == 0 || strcmp(n->text, "%") == 0) &&
        is_int_type(n->type_name) && !int_div_is_safe(n)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_may_fail(n->children[i])) return 1;
    }
    return 0;
}

static int emit_block(FILE *f, const AstNode *block, int depth) {
    if (!block || block->kind != AST_BLOCK) return 0;
    for (size_t i = 0; i < block->child_count; i++) {
        if (!emit_stmt(f, block->children[i], depth)) return 0;
    }
    return 1;
}

static int emit_stmt(FILE *f, const AstNode *n, int depth) {
    indent(f, depth);

    switch (n->kind) {
        case AST_LET: {
            if (!n->type_name) return 0;
            const char *type = c_type_name(n->type_name);
            if (!type) return 0;

            if (stack_list_decl_is_safe(n)) {
                const AstNode *list = n->children[0];
                const char *element_type =
                    list_element_c_type(n->type_name);
                if (!element_type) return 0;

                size_t id = stack_list_counter++;

                fprintf(
                    f,
                    "%s __clair_stack_list_data_%zu[%zu] = {",
                    element_type,
                    id,
                    list->child_count
                );

                for (size_t i = 0; i < list->child_count; i++) {
                    if (i != 0) fputs(", ", f);
                    if (!emit_expr(f, list->children[i])) return 0;
                }

                fputs("};\n", f);

                indent(f, depth);
                fprintf(
                    f,
                    "struct ClairListObj "
                    "__clair_stack_list_obj_%zu = { "
                    "__clair_stack_list_data_%zu, %zu, %zu, "
                    "sizeof(%s) };\n",
                    id,
                    id,
                    list->child_count,
                    list->child_count,
                    element_type
                );

                indent(f, depth);
                fprintf(
                    f,
                    "ClairList %s = "
                    "&__clair_stack_list_obj_%zu;\n",
                    n->text,
                    id
                );

                return 1;
            }

            /*
             * Clair lexical rule:
             *
             *     let x: T = x
             *
             * resolves the RHS to an outer x because the new
             * binding becomes visible only after its initializer.
             *
             * C differs: in
             *
             *     T x = x;
             *
             * the RHS already names the new uninitialized x.
             * Evaluate through a uniquely named temporary first.
             */
            if (n->children[0] &&
                n->children[0]->kind == AST_IDENTIFIER &&
                n->children[0]->text &&
                n->text &&
                strcmp(n->children[0]->text, n->text) == 0) {
                size_t id = shadow_init_counter++;

                fprintf(
                    f,
                    "%s __clair_shadow_init_%zu = ",
                    type,
                    id
                );

                if (!emit_expr(f, n->children[0])) return 0;
                fputs(";\n", f);

                indent(f, depth);

                const char *shadow_type =
                    c_type_name(n->type_name);
                if (!shadow_type) return 0;

                fprintf(
                    f,
                    "%s %s = __clair_shadow_init_%zu;\n",
                    shadow_type,
                    n->text,
                    id
                );

                return 1;
            }

            fprintf(f, "%s %s = ", type, n->text);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;
        }

        case AST_ASSIGN:
            fprintf(f, "%s = ", n->text);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_EXPR_STMT:
            if (n->child_count != 1) return 0;
            fputs("(void)", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_MEMBER_ASSIGN:
            if (n->child_count != 2 || !n->text) return 0;
            if (!emit_operand(f, n->children[0], PREC_POSTFIX, 0)) return 0;
            fprintf(f, "->%s = ", n->text);
            if (!emit_expr(f, n->children[1])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_MATRIX_SET: {
            if ((n->child_count != 3 && n->child_count != 4) || !n->type_name) return 0;
            const char *fast_base = fast_access_base(n);
            if (fast_base) {
                int64_t col0;
                fprintf(f, "clair_data_%s[", fast_base);
                if (!emit_operand(f, n->children[0], PREC_MULTIPLICATIVE, 0)) return 0;
                fprintf(f, " * clair_cols_%s", fast_base);
                if (!(const_int_value(n->children[1], &col0) && col0 == 0)) {
                    fputs(" + ", f);
                    if (!emit_operand(f, n->children[1], PREC_ADDITIVE, 1)) return 0;
                }
                fputs("] = ", f);
                if (!emit_expr(f, n->children[2])) return 0;
                fputs(";\n", f);
                return 1;
            }
            const char *accessor = matrix_accessor_name(n->type_name);
            if (!node_may_fail(n->children[0]) && !node_may_fail(n->children[1]) && !node_may_fail(n->children[2])) {
                fprintf(f, "*%s(", accessor);
                if (!emit_write_target(f, n, 3)) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs(") = ", f);
                if (!emit_expr(f, n->children[2])) return 0;
                fputs(";\n", f);
                return 1;
            }
            /* same order as lists: indices, then value, then the bounds check */
            size_t id = list_mutation_counter++;
            fputs("{\n", f);
            indent(f, depth + 1);
            fprintf(f, "int64_t __clair_row_%zu = ", id);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            indent(f, depth + 1);
            fprintf(f, "int64_t __clair_col_%zu = ", id);
            if (!emit_expr(f, n->children[1])) return 0;
            fputs(";\n", f);
            indent(f, depth + 1);
            fprintf(f, "%s __clair_value_%zu = ", matrix_element_c_type(n->type_name), id);
            if (!emit_expr(f, n->children[2])) return 0;
            fputs(";\n", f);
            indent(f, depth + 1);
            fprintf(f, "*%s(", accessor);
            if (!emit_write_target(f, n, 3)) return 0;
            fprintf(f, ", __clair_row_%zu, __clair_col_%zu) = __clair_value_%zu;\n", id, id, id);
            indent(f, depth);
            fputs("}\n", f);
            return 1;
        }

        case AST_INDEX_ASSIGN: {
            if (!n->type_name) return 0;
            size_t id;

            if (is_list_type(n->type_name)) {
                const char *element_type = list_element_c_type(n->type_name);
                if (!element_type) return 0;

                const char *fast_base = fast_access_base(n);
                if (fast_base) {
                    fprintf(f, "clair_data_%s[", fast_base);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs("] = ", f);
                    if (!emit_expr(f, n->children[1])) return 0;
                    fputs(";\n", f);
                    return 1;
                }

                /*
                 * When neither side can fail or call a function, the natural
                 * one-line form behaves exactly like the evaluate-index,
                 * evaluate-value, then check order used below.
                 */
                if (!node_may_fail(n->children[0]) && !node_may_fail(n->children[1])) {
                    fprintf(f, "*%s(", list_accessor_name(n->type_name));
                    if (!emit_write_target(f, n, 2)) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs(") = ", f);
                    if (!emit_expr(f, n->children[1])) return 0;
                    fputs(";\n", f);
                    return 1;
                }
                id = list_mutation_counter++;
                fputs("{\n", f);
                indent(f, depth + 1);
                fprintf(f, "int64_t __clair_list_index_%zu = ", id);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(";\n", f);
                indent(f, depth + 1);
                fprintf(f, "%s __clair_list_value_%zu = ", element_type, id);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs(";\n", f);
                indent(f, depth + 1);
                fprintf(f, "*%s(", list_accessor_name(n->type_name));
                if (!emit_write_target(f, n, 2)) return 0;
                fprintf(f, ", __clair_list_index_%zu) = __clair_list_value_%zu;\n", id, id);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }

            if (is_dict_type(n->type_name)) {
                const char *value_type = dict_value_c_type(n->type_name);
                if (!value_type) return 0;
                id = list_mutation_counter++;
                fputs("{\n", f);
                indent(f, depth + 1);
                fprintf(f, "const char *__clair_dict_key_%zu = ", id);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(";\n", f);
                indent(f, depth + 1);
                fprintf(f, "%s __clair_dict_value_%zu = ", value_type, id);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs(";\n", f);
                indent(f, depth + 1);
                fprintf(f, "*(%s *)clair_rt_dict_set_slot(", value_type);
                if (!emit_write_target(f, n, 2)) return 0;
                fprintf(f, ", __clair_dict_key_%zu) = __clair_dict_value_%zu;\n", id, id);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }

            return 0;
        }

        case AST_LIST_APPEND: {
            if (!n->type_name || !is_list_type(n->type_name)) return 0;
            const char *element_type = list_element_c_type(n->type_name);
            if (!element_type) return 0;
            size_t id = list_mutation_counter++;
            fputs("{\n", f);
            indent(f, depth + 1);
            fprintf(f, "%s __clair_list_append_value_%zu = ", element_type, id);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            indent(f, depth + 1);
            fprintf(f, "*(%s *)clair_rt_list_append_slot(", element_type);
            if (!emit_write_target(f, n, 1)) return 0;
            fprintf(f, ") = __clair_list_append_value_%zu;\n", id);
            indent(f, depth);
            fputs("}\n", f);
            return 1;
        }

        case AST_RETURN:
            fputs("return ", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_BREAK:
            fputs("break;\n", f);
            return 1;

        case AST_CONTINUE:
            fputs("continue;\n", f);
            return 1;

        case AST_PRINT: {
            const AstNode *value = n->children[0];
            if (!value->type_name) return 0;
            if (strcmp(value->type_name, "string") == 0) {
                fputs("puts(", f);
                if (!emit_expr(f, value)) return 0;
                fputs(");\n", f);
            } else if (strcmp(value->type_name, "float") == 0) {
                fputs("printf(\"%.15g\\n\", (double)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (strcmp(value->type_name, "bool") == 0) {
                fputs("printf(\"%d\\n\", (int)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (strcmp(value->type_name, "int") == 0) {
                fputs("printf(\"%\" PRId64 \"\\n\", (int64_t)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else {
                return 0;
            }
            return 1;
        }

        case AST_WHILE:

            {
                const char *v4_list = NULL;
                const char *v4_index = NULL;

                if (clair_simd_v4_match_map(
                        n, &v4_list, &v4_index)) {

                    size_t id = clair_simd_v4_loop_counter++;

                    fprintf(f,
                        "if (%s == 0 && %s != NULL && "
                        "%s->len >= 16 && %s->elem_size == sizeof(int64_t) && "
                        "%s->len <= (size_t)INT64_MAX && "
                        "(%s->len == 0 || %s->data != NULL)) {\n",
                        v4_index, v4_list, v4_list,
                        v4_list, v4_list, v4_list, v4_list);

                    indent(f, depth + 1);
                    fprintf(f,
                        "size_t __clair_v4_len_%zu = %s->len;\n",
                        id, v4_list);

                    indent(f, depth + 1);
                    fprintf(f,
                        "int64_t *__clair_v4_data_%zu = "
                        "(int64_t *)%s->data;\n",
                        id, v4_list);

                    indent(f, depth + 1);
                    fprintf(f,
                        "for (size_t __clair_v4_k_%zu = 0; "
                        "__clair_v4_k_%zu < __clair_v4_len_%zu; "
                        "++__clair_v4_k_%zu) {\n",
                        id, id, id, id);

                    indent(f, depth + 2);
                    fprintf(f,
                        "__clair_v4_data_%zu[__clair_v4_k_%zu] = "
                        "__clair_v4_data_%zu[__clair_v4_k_%zu] + 1;\n",
                        id, id, id, id);

                    indent(f, depth + 1);
                    fputs("}\n", f);

                    indent(f, depth + 1);
                    fprintf(f,
                        "%s = (int64_t)__clair_v4_len_%zu;\n",
                        v4_index, id);

                    indent(f, depth);
                    fputs("} else {\n", f);

                    indent(f, depth + 1);
                    fputs("while (", f);

                    if (!emit_expr(f, n->children[0]))
                        return 0;

                    fputs(") {\n", f);

                    if (!emit_block(
                            f, n->children[1], depth + 2))
                        return 0;

                    indent(f, depth + 1);
                    fputs("}\n", f);

                    indent(f, depth);
                    fputs("}\n", f);

                    return 1;
                }
            }

            if (n->child_count != 2) return 0;
            fputs("while (", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(") {\n", f);
            if (!emit_block(f, n->children[1], depth + 1)) return 0;
            indent(f, depth);
            fputs("}\n", f);
            return 1;

        case AST_FOR: {
            if (!n->text || n->child_count < 2 || n->child_count > 4) return 0;

            size_t argc = n->child_count - 1;
            const AstNode *body = n->children[n->child_count - 1];
            const char *var = n->text;

            int64_t step = 1;
            int literal_step = argc < 3 || (const_int_value(n->children[2], &step) && step != 0);

            if (literal_step) {
                /*
                 * Hand-written shape:  for (int64_t i = 0; i < n; i++)
                 * range() is evaluated once, so start/stop are copied into
                 * locals unless they are literals or variables the body
                 * never changes.
                 */
                const AstNode *start_node = argc == 1 ? NULL : n->children[0];
                const AstNode *stop_node = n->children[argc == 1 ? 0 : 1];
                int64_t k;
                int start_simple = !start_node || const_int_value(start_node, &k) ||
                    (start_node->kind == AST_IDENTIFIER && !name_changes_in(body, start_node->text) &&
                     strcmp(start_node->text, var) != 0);
                int stop_simple = const_int_value(stop_node, &k) ||
                    (stop_node->kind == AST_IDENTIFIER && !name_changes_in(body, stop_node->text) &&
                     strcmp(stop_node->text, var) != 0);

                FastPlan plan;
                int fast = step == 1 && fast_plan_for(n, &plan);
                int need_block = !start_simple || !stop_simple;

                char *start_text = NULL, *stop_text = NULL;
                int first_line = 1;  /* emit_stmt already indented it */
                #define LINE_INDENT(DEPTH) do { if (first_line) first_line = 0; else indent(f, (DEPTH)); } while (0)
                if (need_block) {
                    LINE_INDENT(depth);
                    fputs("{\n", f);
                    if (!start_simple) {
                        indent(f, depth + 1);
                        fprintf(f, "int64_t clair_start_%s = ", var);
                        if (!emit_expr(f, start_node)) return 0;
                        fputs(";\n", f);
                        start_text = str_fmt("clair_start_%s", var);
                    }
                    if (!stop_simple) {
                        indent(f, depth + 1);
                        fprintf(f, "int64_t clair_stop_%s = ", var);
                        if (!emit_expr(f, stop_node)) return 0;
                        fputs(";\n", f);
                        stop_text = str_fmt("clair_stop_%s", var);
                    }
                }

                /* Emits `for (...)` header: start and stop are simple or hoisted. */
                #define EMIT_FOR_HEADER(DEPTH) do {                                         \
                    LINE_INDENT(DEPTH);                                                    \
                    fprintf(f, "for (int64_t %s = ", var);                                 \
                    if (start_text) fputs(start_text, f);                                  \
                    else if (!start_node) fputs("0", f);                                   \
                    else if (!emit_expr(f, start_node)) return 0;                          \
                    fprintf(f, "; %s %s ", var, step > 0 ? "<" : ">");                     \
                    if (stop_text) fputs(stop_text, f);                                    \
                    else if (!emit_operand(f, stop_node, PREC_RELATIONAL, 1)) return 0;    \
                    if (step == 1) fprintf(f, "; %s++) {\n", var);                         \
                    else if (step == -1) fprintf(f, "; %s--) {\n", var);                   \
                    else if (step > 0) fprintf(f, "; %s += %lld) {\n", var, (long long)step); \
                    else if (step != INT64_MIN) fprintf(f, "; %s -= %lld) {\n", var, -(long long)step); \
                    else fprintf(f, "; %s += INT64_MIN) {\n", var);                        \
                } while (0)

                int inner = need_block ? depth + 1 : depth;

                if (fast) {
                    /* bounds of every listed access, proven once for the whole range */
                    LINE_INDENT(inner);
                    fputs("if (", f);
                    for (size_t i = 0; i < plan.check_count; i++) {
                        BoundsCheck *b = &plan.checks[i];
                        if (i != 0) {
                            fputs(" &&\n", f);
                            indent(f, inner + 1);
                        }
                        const char *bexpr = fast_plan_expr(&plan, b->base);
                        if (b->dim == 0) fprintf(f, "clair_rt_affine_ok(%s, %s, %s, ", bexpr, b->c0, b->c1);
                        else fprintf(f, "clair_rt_affine_in(%s, %s, ", b->c0, b->c1);
                        if (start_text) fputs(start_text, f);
                        else if (!start_node) fputs("0", f);
                        else if (!emit_expr(f, start_node)) return 0;
                        fputs(", ", f);
                        if (stop_text) fputs(stop_text, f);
                        else if (!emit_expr(f, stop_node)) return 0;
                        if (b->dim == 1) fprintf(f, ", %s->rows", bexpr);
                        if (b->dim == 2) fprintf(f, ", %s->cols", bexpr);
                        fputc(')', f);
                    }
                    fputs(") {\n", f);

                    for (size_t i = 0; i < plan.base_count; i++) {
                        int matrix = is_matrix_type(plan.base_types[i]);
                        const char *elem = matrix ? matrix_element_c_type(plan.base_types[i])
                                                  : list_element_c_type(plan.base_types[i]);
                        indent(f, inner + 1);
                        fprintf(f, "%s *clair_data_%s = (%s *)%s->data;\n",
                                elem, plan.bases[i], elem, plan.base_exprs[i]);
                        if (matrix) {
                            indent(f, inner + 1);
                            fprintf(f, "int64_t clair_cols_%s = %s->cols;\n", plan.bases[i], plan.base_exprs[i]);
                        }
                    }

                    EMIT_FOR_HEADER(inner + 1);
                    for (size_t i = 0; i < plan.access_count; i++) fast_access[i] = plan.accesses[i];
                    fast_access_count = plan.access_count;
                    fast_access_active = 1;
                    int ok = emit_block(f, body, inner + 2);
                    fast_access_active = 0;
                    fast_access_count = 0;
                    if (!ok) return 0;
                    indent(f, inner + 1);
                    fputs("}\n", f);

                    indent(f, inner);
                    fputs("} else {\n", f);
                    EMIT_FOR_HEADER(inner + 1);
                    if (!emit_block(f, body, inner + 2)) return 0;
                    indent(f, inner + 1);
                    fputs("}\n", f);
                    indent(f, inner);
                    fputs("}\n", f);
                    fast_plan_free(&plan);
                } else {
                    EMIT_FOR_HEADER(inner);
                    if (!emit_block(f, body, inner + 1)) return 0;
                    indent(f, inner);
                    fputs("}\n", f);
                }
                #undef EMIT_FOR_HEADER
                #undef LINE_INDENT

                if (need_block) {
                    indent(f, depth);
                    fputs("}\n", f);
                }
                free(start_text);
                free(stop_text);
                return 1;
            }

            /* Step only known at run time (or zero): general form with its checks. */
            size_t id = range_counter++;
            fputs("{\n", f);

            indent(f, depth + 1);
            fprintf(f, "int64_t __clair_range_%zu_start = ", id);
            if (argc == 1) {
                fputs("0", f);
            } else if (!emit_expr(f, n->children[0])) {
                return 0;
            }
            fputs(";\n", f);

            indent(f, depth + 1);
            fprintf(f, "int64_t __clair_range_%zu_stop = ", id);
            if (!emit_expr(f, n->children[argc == 1 ? 0 : 1])) return 0;
            fputs(";\n", f);

            indent(f, depth + 1);
            fprintf(f, "int64_t __clair_range_%zu_step = ", id);
            if (argc < 3) {
                fputs("1", f);
            } else if (!emit_expr(f, n->children[2])) {
                return 0;
            }
            fputs(";\n", f);

            indent(f, depth + 1);
            fprintf(f, "if (__clair_range_%zu_step == 0) {\n", id);
            indent(f, depth + 2);
            fputs("fputs(\"Clair runtime error: range() step cannot be zero\\n\", stderr);\n", f);
            indent(f, depth + 2);
            fputs("exit(EXIT_FAILURE);\n", f);
            indent(f, depth + 1);
            fputs("}\n", f);

            indent(f, depth + 1);
            fprintf(f, "for (int64_t %s = __clair_range_%zu_start; ", n->text, id);
            fprintf(f, "(__clair_range_%zu_step > 0) ? (%s < __clair_range_%zu_stop) : (%s > __clair_range_%zu_stop); ",
                    id, n->text, id, n->text, id);
            fprintf(f, "%s += __clair_range_%zu_step) {\n", n->text, id);

            if (!emit_block(f, body, depth + 2)) return 0;

            indent(f, depth + 1);
            fputs("}\n", f);
            indent(f, depth);
            fputs("}\n", f);
            return 1;
        }

        case AST_IF:
            if (n->child_count != 2 && n->child_count != 3) return 0;
            fputs("if (", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(") {\n", f);
            if (!emit_block(f, n->children[1], depth + 1)) return 0;
            indent(f, depth);
            fputc('}', f);
            if (n->child_count == 3) {
                fputs(" else {\n", f);
                if (!emit_block(f, n->children[2], depth + 1)) return 0;
                indent(f, depth);
                fputc('}', f);
            }
            fputc('\n', f);
            return 1;

        default:
            return 0;
    }
}

static int emit_function_signature(FILE *f, const AstNode *fn, int prototype) {
    int is_main = strcmp(fn->text, "main") == 0;

    if (!fn->type_name) return 0;

    if (is_main) {
        if (function_param_count(fn) != 0 || strcmp(fn->type_name, "int") != 0) return 0;
        fputs("int main(void)", f);
    } else {
        const char *return_type = c_type_name(fn->type_name);
        if (!return_type) return 0;
        /*
         * User functions are `static` so Clang knows every call site (it can
         * inline and drop dead code), and carry a `clair_fn_` prefix so they
         * can never collide with C library names such as abs or exit.
         */
        fprintf(f, "static %s clair_fn_%s(", return_type, fn->text);
        if (!emit_param_list(f, fn)) return 0;
        fputc(')', f);
    }

    if (prototype) fputs(";\n", f);
    return 1;
}


static int node_uses_string_concat(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_BINARY && n->text && n->type_name &&
        strcmp(n->text, "+") == 0 && strcmp(n->type_name, "string") == 0) {
        return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_string_concat(n->children[i])) return 1;
    }
    return 0;
}

static int node_uses_list(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_LIST || n->kind == AST_INDEX_ASSIGN || n->kind == AST_LIST_APPEND ||
        (n->kind == AST_INDEX && n->child_count == 2 && is_list_type(n->children[0]->type_name)) ||
        is_list_type(n->type_name)) {
        return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_list(n->children[i])) return 1;
    }
    return 0;
}

static int node_uses_dict(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_DICT || is_dict_type(n->type_name)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_dict(n->children[i])) return 1;
    }
    return 0;
}

static int emit_dict_runtime(FILE *f) {
    fputs(
        "typedef struct ClairDictObj { const char **keys; void *values; size_t len; size_t capacity; size_t elem_size; } ClairDictObj;\n"
        "typedef ClairDictObj *ClairDict;\n\n"
        "static ClairDict *clair_rt_dicts = NULL;\n"
        "static size_t clair_rt_dict_count = 0;\n"
        "static size_t clair_rt_dict_capacity = 0;\n"
        "static int clair_rt_dict_cleanup_registered = 0;\n\n"
        "static void clair_rt_cleanup_dicts(void) {\n"
        "    for (size_t i = 0; i < clair_rt_dict_count; i++) {\n"
        "        if (clair_rt_dicts[i]) {\n"
        "            free(clair_rt_dicts[i]->keys);\n"
        "            free(clair_rt_dicts[i]->values);\n"
        "            free(clair_rt_dicts[i]);\n"
        "        }\n"
        "    }\n"
        "    free(clair_rt_dicts);\n"
        "    clair_rt_dicts = NULL;\n"
        "    clair_rt_dict_count = 0;\n"
        "    clair_rt_dict_capacity = 0;\n"
        "}\n\n"
        "static void clair_rt_dict_runtime_error(const char *message) {\n"
        "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
        "    exit(EXIT_FAILURE);\n"
        "}\n\n"
        "static void clair_rt_track_dict(ClairDict dict) {\n"
        "    if (!clair_rt_dict_cleanup_registered) {\n"
        "        if (atexit(clair_rt_cleanup_dicts) != 0) clair_rt_dict_runtime_error(\"cannot register dictionary cleanup\");\n"
        "        clair_rt_dict_cleanup_registered = 1;\n"
        "    }\n"
        "    if (clair_rt_dict_count == clair_rt_dict_capacity) {\n"
        "        size_t new_capacity = clair_rt_dict_capacity ? clair_rt_dict_capacity * 2 : 16;\n"
        "        if (new_capacity < clair_rt_dict_capacity || new_capacity > SIZE_MAX / sizeof(*clair_rt_dicts)) {\n"
        "            clair_rt_dict_runtime_error(\"dictionary allocation registry overflow\");\n"
        "        }\n"
        "        ClairDict *new_items = realloc(clair_rt_dicts, new_capacity * sizeof(*new_items));\n"
        "        if (!new_items) clair_rt_dict_runtime_error(\"out of memory while tracking dictionaries\");\n"
        "        clair_rt_dicts = new_items;\n"
        "        clair_rt_dict_capacity = new_capacity;\n"
        "    }\n"
        "    clair_rt_dicts[clair_rt_dict_count++] = dict;\n"
        "}\n\n"
        "static void *clair_rt_dict_at(ClairDict dict, const char *key) {\n"
        "    if (!dict) clair_rt_dict_runtime_error(\"null dictionary\");\n"
        "    if (!key) clair_rt_dict_runtime_error(\"null dictionary key\");\n"
        "    for (size_t i = 0; i < dict->len; i++) {\n"
        "        if (strcmp(dict->keys[i], key) == 0) {\n"
        "            return (unsigned char *)dict->values + (i * dict->elem_size);\n"
        "        }\n"
        "    }\n"
        "    fprintf(stderr, \"Clair runtime error: dictionary key '%s' not found\\n\", key);\n"
        "    exit(EXIT_FAILURE);\n"
        "}\n\n"
        "static void *clair_rt_dict_set_slot(ClairDict dict, const char *key) {\n"
        "    if (!dict) clair_rt_dict_runtime_error(\"null dictionary\");\n"
        "    if (!key) clair_rt_dict_runtime_error(\"null dictionary key\");\n"
        "    if (dict->elem_size == 0) clair_rt_dict_runtime_error(\"invalid dictionary value size\");\n"
        "    for (size_t i = 0; i < dict->len; i++) {\n"
        "        if (strcmp(dict->keys[i], key) == 0) {\n"
        "            return (unsigned char *)dict->values + (i * dict->elem_size);\n"
        "        }\n"
        "    }\n"
        "    if (dict->len == dict->capacity) {\n"
        "        size_t new_capacity = dict->capacity ? dict->capacity * 2 : 4;\n"
        "        if (new_capacity < dict->capacity || new_capacity > SIZE_MAX / sizeof(*dict->keys) ||\n"
        "            new_capacity > SIZE_MAX / dict->elem_size) {\n"
        "            clair_rt_dict_runtime_error(\"dictionary capacity overflow\");\n"
        "        }\n"
        "        const char **new_keys = realloc(dict->keys, new_capacity * sizeof(*new_keys));\n"
        "        if (!new_keys) clair_rt_dict_runtime_error(\"out of memory while growing dictionary keys\");\n"
        "        dict->keys = new_keys;\n"
        "        void *new_values = realloc(dict->values, new_capacity * dict->elem_size);\n"
        "        if (!new_values) clair_rt_dict_runtime_error(\"out of memory while growing dictionary values\");\n"
        "        dict->values = new_values;\n"
        "        dict->capacity = new_capacity;\n"
        "    }\n"
        "    size_t index = dict->len++;\n"
        "    dict->keys[index] = key;\n"
        "    return (unsigned char *)dict->values + (index * dict->elem_size);\n"
        "}\n\n"
        "static ClairDict clair_rt_dict_make(const char *const *keys, const void *values, size_t len, size_t elem_size) {\n"
        "    if (elem_size == 0) clair_rt_dict_runtime_error(\"invalid dictionary value size\");\n"
        "    ClairDict result = malloc(sizeof(*result));\n"
        "    if (!result) clair_rt_dict_runtime_error(\"out of memory while creating dictionary\");\n"
        "    result->keys = NULL;\n"
        "    result->values = NULL;\n"
        "    result->len = 0;\n"
        "    result->capacity = 0;\n"
        "    result->elem_size = elem_size;\n"
        "    clair_rt_track_dict(result);\n"
        "    for (size_t i = 0; i < len; i++) {\n"
        "        void *slot = clair_rt_dict_set_slot(result, keys[i]);\n"
        "        memcpy(slot, (const unsigned char *)values + (i * elem_size), elem_size);\n"
        "    }\n"
        "    return result;\n"
        "}\n\n",
        f
    );
    return 1;
}


static int emit_class_declarations(FILE *f, const AstNode *program) {
    /* Forward declarations allow classes to reference other class objects. */
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *cls = program->children[i];
        if (cls->kind == AST_CLASS) {
            fprintf(f, "typedef struct ClairClass_%s ClairClass_%s;\n", cls->text, cls->text);
        }
    }

    int had_class = 0;
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *cls = program->children[i];
        if (cls->kind != AST_CLASS) continue;
        had_class = 1;

        fprintf(f, "struct ClairClass_%s {\n", cls->text);
        if (cls->child_count == 0) {
            fputs("    unsigned char __clair_empty;\n", f);
        } else {
            for (size_t j = 0; j < cls->child_count; j++) {
                const AstNode *field = cls->children[j];
                const char *type = c_type_name(field->type_name);
                if (!type) return 0;
                fprintf(f, "    %s %s;\n", type, field->text);
            }
        }
        fputs("};\n\n", f);
    }

    if (had_class) fputc('\n', f);
    return 1;
}

static int node_uses_call_named(const AstNode *n, const char *name) {
    if (!n) return 0;
    if (n->kind == AST_CALL && n->text && strcmp(n->text, name) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_call_named(n->children[i], name)) return 1;
    }
    return 0;
}

static int node_uses_int_division(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_BINARY && n->text && strcmp(n->text, "/") == 0 &&
        n->type_name && strcmp(n->type_name, "int") == 0 && !int_div_is_safe(n)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_int_division(n->children[i])) return 1;
    }
    return 0;
}

static int node_uses_matrix(const AstNode *n) {
    if (!n) return 0;
    if (is_matrix_type(n->type_name)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_matrix(n->children[i])) return 1;
    }
    return 0;
}

/* element-access kinds actually used, so only those accessors are emitted */
static int node_uses_matrix_kind(const AstNode *n, const char *suffix) {
    if (!n) return 0;
    if ((n->kind == AST_MATRIX_SET && n->type_name && strstr(n->type_name, suffix)) ||
        (n->kind == AST_INDEX && n->child_count == 2 && is_matrix_row_type(n->children[0]->type_name) &&
         strstr(n->children[0]->type_name, suffix))) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_matrix_kind(n->children[i], suffix)) return 1;
    }
    return 0;
}

static int node_uses_int_mod(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_BINARY && n->text && strcmp(n->text, "%") == 0 &&
        is_int_type(n->type_name) && !int_div_is_safe(n)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_int_mod(n->children[i])) return 1;
    }
    return 0;
}

static int node_uses_int_builtin(const AstNode *n, const char *name) {
    if (!n) return 0;
    if (n->kind == AST_CALL && n->text && strcmp(n->text, name) == 0 && is_int_type(n->type_name)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_int_builtin(n->children[i], name)) return 1;
    }
    return 0;
}

static int node_uses_math(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_CALL && n->text && n->text[0] == '@' &&
        strcmp(n->text, "@int") != 0 && strcmp(n->text, "@float") != 0 &&
        strcmp(n->text, "@matrix") != 0 && strcmp(n->text, "@rows") != 0 && strcmp(n->text, "@cols") != 0 &&
        !(is_int_type(n->type_name) &&
          (strcmp(n->text, "@abs") == 0 || strcmp(n->text, "@min") == 0 || strcmp(n->text, "@max") == 0))) return 1;
    if (n->kind == AST_BINARY && n->text && strcmp(n->text, "%") == 0 && !is_int_type(n->type_name)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_math(n->children[i])) return 1;
    }
    return 0;
}

static int node_indexes_list_type(const AstNode *n, const char *list_type) {
    if (!n) return 0;
    if (n->kind == AST_INDEX && n->child_count == 2 && n->children[0]->type_name &&
        strcmp(n->children[0]->type_name, list_type) == 0) return 1;
    if (n->kind == AST_INDEX_ASSIGN && n->type_name && strcmp(n->type_name, list_type) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_indexes_list_type(n->children[i], list_type)) return 1;
    }
    return 0;
}

static int node_uses_kind(const AstNode *n, AstKind kind) {
    if (!n) return 0;
    if (n->kind == kind) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_kind(n->children[i], kind)) return 1;
    }
    return 0;
}


int codegen_c_file(const AstNode *program, const char *path) {
    codegen_program = program;
    stack_list_counter = 0;
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    int uses_input_int = node_uses_call_named(program, "input_int");
    compute_safe_functions(program);

    fputs("/* Generated by the Clair compiler. */\n", f);
    fputs(
        "#include <inttypes.h>\n"
        "#include <stdbool.h>\n"
        "#include <stdint.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n",
        f
    );
    if (uses_input_int) fputs("#include <errno.h>\n", f);
    if (node_uses_math(program)) fputs("#include <math.h>\n", f);
    fputc('\n', f);

    if (node_uses_int_mod(program)) {
        fputs(
            "static int64_t clair_rt_int_mod_checked(int64_t a, int64_t b) {\n"
            "    if (b == 0) {\n"
            "        fputs(\"Clair runtime error: modulo by zero\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    return b == -1 ? 0 : a % b;  /* INT64_MIN % -1 would trap in C */\n"
            "}\n\n",
            f
        );
    }
    if (node_uses_int_builtin(program, "@abs"))
        fputs("static inline int64_t clair_rt_abs_i64(int64_t x) { return x < 0 ? -x : x; }\n\n", f);
    if (node_uses_int_builtin(program, "@min"))
        fputs("static inline int64_t clair_rt_min_i64(int64_t a, int64_t b) { return a < b ? a : b; }\n\n", f);
    if (node_uses_int_builtin(program, "@max"))
        fputs("static inline int64_t clair_rt_max_i64(int64_t a, int64_t b) { return a > b ? a : b; }\n\n", f);
    if (node_uses_call_named(program, "@int")) {
        fputs(
            "static int64_t clair_rt_float_to_int(double x) {\n"
            "    if (!(x >= -9223372036854775808.0 && x < 9223372036854775808.0)) {  /* also rejects NaN */\n"
            "        fputs(\"Clair runtime error: int() argument out of range\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    return (int64_t)x;  /* truncates toward zero, like Python int() */\n"
            "}\n\n",
            f
        );
    }

    if (node_uses_int_division(program)) {
        fputs(
            "static int64_t clair_rt_int_div_checked(int64_t a, int64_t b) {\n"
            "    if (b == 0) {\n"
            "        fputs(\"Clair runtime error: division by zero\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    if (a == INT64_MIN && b == -1) {\n"
            "        fputs(\"Clair runtime error: integer division overflow\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    return a / b;\n"
            "}\n\n",
            f
        );
    }

    if (uses_input_int) {
        fputs(
            "static int64_t clair_rt_input_int(void) {\n"
            "    char buffer[256];\n"
            "    if (!fgets(buffer, sizeof(buffer), stdin)) {\n"
            "        fprintf(stderr, \"Clair runtime error: input_int() could not read input\\n\");\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    errno = 0;\n"
            "    char *end = NULL;\n"
            "    intmax_t value = strtoimax(buffer, &end, 10);\n"
            "    while (*end == ' ' || *end == '\\t' || *end == '\\r' || *end == '\\n') end++;\n"
            "    if (end == buffer || errno == ERANGE || value < INT64_MIN || value > INT64_MAX || *end != '\\0') {\n"
            "        fprintf(stderr, \"Clair runtime error: input_int() expected a valid integer\\n\");\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    return (int64_t)value;\n"
            "}\n\n",
            f
        );
    }

    if (node_uses_list(program)) {
        fputs(
            "typedef struct ClairListObj { void *data; size_t len; size_t capacity; size_t elem_size; } ClairListObj;\n"
            "typedef ClairListObj *ClairList;\n\n",
            f
        );

        int uses_list_literal = node_uses_kind(program, AST_LIST);
        int uses_list_at = node_uses_kind(program, AST_INDEX) || node_uses_kind(program, AST_INDEX_ASSIGN);
        int uses_list_append = node_uses_kind(program, AST_LIST_APPEND);

        if (uses_list_literal) {
            fputs(
                "static ClairList *clair_rt_lists = NULL;\n"
                "static size_t clair_rt_list_count = 0;\n"
                "static size_t clair_rt_list_capacity = 0;\n"
                "static int clair_rt_list_cleanup_registered = 0;\n\n"
                "static void clair_rt_cleanup_lists(void) {\n"
                "    for (size_t i = 0; i < clair_rt_list_count; i++) {\n"
                "        if (clair_rt_lists[i]) { free(clair_rt_lists[i]->data); free(clair_rt_lists[i]); }\n"
                "    }\n"
                "    free(clair_rt_lists);\n"
                "    clair_rt_lists = NULL;\n"
                "    clair_rt_list_count = 0;\n"
                "    clair_rt_list_capacity = 0;\n"
                "}\n\n"
                "static void clair_rt_list_runtime_error(const char *message, ClairList owned) {\n"
                "    if (owned) { free(owned->data); free(owned); }\n"
                "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
                "    exit(EXIT_FAILURE);\n"
                "}\n\n"
                "static void clair_rt_track_list(ClairList list) {\n"
                "    if (!clair_rt_list_cleanup_registered) {\n"
                "        if (atexit(clair_rt_cleanup_lists) != 0) clair_rt_list_runtime_error(\"cannot register list cleanup\", list);\n"
                "        clair_rt_list_cleanup_registered = 1;\n"
                "    }\n"
                "    if (clair_rt_list_count == clair_rt_list_capacity) {\n"
                "        size_t new_capacity = clair_rt_list_capacity ? clair_rt_list_capacity * 2 : 16;\n"
                "        if (new_capacity < clair_rt_list_capacity || new_capacity > SIZE_MAX / sizeof(*clair_rt_lists)) {\n"
                "            clair_rt_list_runtime_error(\"list allocation registry overflow\", list);\n"
                "        }\n"
                "        ClairList *new_items = realloc(clair_rt_lists, new_capacity * sizeof(*new_items));\n"
                "        if (!new_items) clair_rt_list_runtime_error(\"out of memory while tracking lists\", list);\n"
                "        clair_rt_lists = new_items;\n"
                "        clair_rt_list_capacity = new_capacity;\n"
                "    }\n"
                "    clair_rt_lists[clair_rt_list_count++] = list;\n"
                "}\n\n"
                "static ClairList clair_rt_list_make(const void *values, size_t len, size_t elem_size) {\n"
                "    if (elem_size == 0) { fputs(\"Clair runtime error: invalid list element size\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "    ClairList result = malloc(sizeof(*result));\n"
                "    if (!result) { fputs(\"Clair runtime error: out of memory while creating list\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "    result->data = NULL;\n"
                "    result->len = len;\n"
                "    result->capacity = len;\n"
                "    result->elem_size = elem_size;\n"
                "    if (len > 0) {\n"
                "        if (len > SIZE_MAX / elem_size) clair_rt_list_runtime_error(\"list allocation overflow\", result);\n"
                "        result->data = malloc(len * elem_size);\n"
                "        if (!result->data) clair_rt_list_runtime_error(\"out of memory while creating list\", result);\n"
                "        memcpy(result->data, values, len * elem_size);\n"
                "    }\n"
                "    clair_rt_track_list(result);\n"
                "    return result;\n"
                "}\n\n",
                f
            );
        }

        if (uses_list_at) {
            fputs(
                "static _Noreturn void clair_rt_index_error(const void *list, int64_t index, size_t len) {\n"
                "    if (!list) fputs(\"Clair runtime error: null list\\n\", stderr);\n"
                "    else fprintf(stderr, \"Clair runtime error: list index %\" PRId64 \" out of bounds for length %zu\\n\", index, len);\n"
                "    exit(EXIT_FAILURE);\n"
                "}\n\n",
                f
            );
            static const char *const kinds[4][3] = {
                { "list[int]", "i64", "int64_t" },
                { "list[float]", "f64", "double" },
                { "list[bool]", "bool", "bool" },
                { "list[string]", "str", "const char *" },
            };
            for (size_t k = 0; k < 4; k++) {
                if (!node_indexes_list_type(program, kinds[k][0])) continue;
                fprintf(f,
                    "static inline %s *clair_rt_list_at_%s_checked(ClairList list, int64_t index) {\n"
                    "    if (!list || index < 0 || (uint64_t)index >= list->len) clair_rt_index_error(list, index, list ? list->len : 0);\n"
                    "    return (%s *)list->data + index;\n"
                    "}\n\n",
                    kinds[k][2], kinds[k][1], kinds[k][2]);
            }
        }

        if (uses_list_append) {
            fputs(
                "static void *clair_rt_list_append_slot(ClairList list) {\n"
                "    if (!list) { fputs(\"Clair runtime error: null list\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "    if (list->elem_size == 0) { fputs(\"Clair runtime error: invalid list element size\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "    if (list->len == list->capacity) {\n"
                "        size_t new_capacity = list->capacity ? list->capacity * 2 : 4;\n"
                "        if (new_capacity < list->capacity || new_capacity > SIZE_MAX / list->elem_size) {\n"
                "            fputs(\"Clair runtime error: list capacity overflow\\n\", stderr);\n"
                "            exit(EXIT_FAILURE);\n"
                "        }\n"
                "        void *new_data = realloc(list->data, new_capacity * list->elem_size);\n"
                "        if (!new_data) { fputs(\"Clair runtime error: out of memory while growing list\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "        list->data = new_data;\n"
                "        list->capacity = new_capacity;\n"
                "    }\n"
                "    void *slot = (unsigned char *)list->data + (list->len * list->elem_size);\n"
                "    list->len++;\n"
                "    return slot;\n"
                "}\n\n",
                f
            );
        }
    }

    if (node_uses_dict(program)) {
        if (!emit_dict_runtime(f)) {
            fclose(f);
            return 0;
        }
    }

    int uses_matrix = node_uses_matrix(program);
    if (uses_matrix) {
        fputs(
            "/* A matrix is one contiguous row-major block, like hand-written C. */\n"
            "typedef struct ClairMatrixObj { void *data; int64_t rows; int64_t cols; struct ClairMatrixObj *next; } ClairMatrixObj;\n"
            "typedef ClairMatrixObj *ClairMatrix;\n\n",
            f
        );
    }

    if (!emit_class_declarations(f, program)) {
        fclose(f);
        return 0;
    }

    int uses_objects = node_uses_kind(program, AST_CONSTRUCT);
    int uses_concat = node_uses_string_concat(program);

    if (uses_objects || uses_concat || uses_matrix) {
        /*
         * Bump-pointer arena shared by objects and concatenated strings.
         * One allocation is a pointer increment instead of malloc() plus a
         * registry insert; the few large chunks are released at exit.
         */
        fputs(
            "/* 32-byte header keeps every allocation 16-byte aligned */\n"
            "typedef struct ClairArenaChunk { struct ClairArenaChunk *next; size_t used; size_t size; size_t pad; } ClairArenaChunk;\n"
            "static ClairArenaChunk *clair_rt_arena = NULL;\n\n"
            "static void clair_rt_arena_cleanup(void) {\n"
            "    while (clair_rt_arena) { ClairArenaChunk *next = clair_rt_arena->next; free(clair_rt_arena); clair_rt_arena = next; }\n"
            "}\n\n"
            "static __attribute__((noinline)) void *clair_rt_arena_grow(size_t size) {\n"
            "    static int registered = 0;\n"
            "    if (!registered) {\n"
            "        if (atexit(clair_rt_arena_cleanup) != 0) { fputs(\"Clair runtime error: cannot register memory cleanup\\n\", stderr); exit(EXIT_FAILURE); }\n"
            "        registered = 1;\n"
            "    }\n"
            "    size_t chunk = (size_t)1 << 20;\n"
            "    if (size > SIZE_MAX - sizeof(ClairArenaChunk) - 16) { fputs(\"Clair runtime error: allocation overflow\\n\", stderr); exit(EXIT_FAILURE); }\n"
            "    if (size + 16 > chunk) chunk = size + 16;\n"
            "    ClairArenaChunk *c = malloc(sizeof(ClairArenaChunk) + chunk);\n"
            "    if (!c) { fputs(\"Clair runtime error: out of memory\\n\", stderr); exit(EXIT_FAILURE); }\n"
            "    c->next = clair_rt_arena;\n"
            "    c->used = 0;\n"
            "    c->size = chunk;\n"
            "    clair_rt_arena = c;\n"
            "    c->used = size;\n"
            "    return (unsigned char *)(c + 1);\n"
            "}\n\n"
            "static inline void *clair_rt_arena_alloc(size_t size) {\n"
            "    size = (size + 15) & ~(size_t)15;\n"
            "    ClairArenaChunk *c = clair_rt_arena;\n"
            "    if (__builtin_expect(c != NULL && c->size - c->used >= size, 1)) {\n"
            "        void *p = (unsigned char *)(c + 1) + c->used;\n"
            "        c->used += size;\n"
            "        return p;\n"
            "    }\n"
            "    return clair_rt_arena_grow(size);\n"
            "}\n\n",
            f
        );
    }

    if (uses_matrix) {
        fputs(
            "static ClairMatrix clair_rt_matrices = NULL;\n\n"
            "static void clair_rt_matrix_cleanup(void) {\n"
            "    for (ClairMatrix m = clair_rt_matrices; m; m = m->next) free(m->data);\n"
            "    clair_rt_matrices = NULL;\n"
            "}\n\n"
            "static ClairMatrix clair_rt_matrix_make(int64_t rows, int64_t cols, size_t elem_size) {\n"
            "    if (rows < 0 || cols < 0) {\n"
            "        fputs(\"Clair runtime error: matrix() size cannot be negative\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    if (cols != 0 && (uint64_t)rows > SIZE_MAX / elem_size / (uint64_t)cols) {\n"
            "        fputs(\"Clair runtime error: matrix() is too large\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    ClairMatrix m = clair_rt_arena_alloc(sizeof(*m));\n"
            "    /* calloc: zeroed lazily by the system, same memory layout as hand-written C */\n"
            "    m->data = calloc((size_t)rows * (size_t)cols + 1, elem_size);\n"
            "    if (!m->data) {\n"
            "        fputs(\"Clair runtime error: out of memory while creating matrix\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    m->rows = rows;\n"
            "    m->cols = cols;\n"
            "    if (!clair_rt_matrices && atexit(clair_rt_matrix_cleanup) != 0) {\n"
            "        fputs(\"Clair runtime error: cannot register matrix cleanup\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    m->next = clair_rt_matrices;\n"
            "    clair_rt_matrices = m;\n"
            "    return m;\n"
            "}\n\n"
            "static _Noreturn void clair_rt_matrix_index_error(ClairMatrix m, int64_t i, int64_t j) {\n"
            "    fprintf(stderr, \"Clair runtime error: matrix index [%\" PRId64 \"][%\" PRId64 \"] out of bounds for shape %\" PRId64 \"x%\" PRId64 \"\\n\",\n"
            "            i, j, m->rows, m->cols);\n"
            "    exit(EXIT_FAILURE);\n"
            "}\n\n",
            f
        );
        static const char *const mkinds[2][3] = {
            { "[float]", "f64", "double" },
            { "[int]", "i64", "int64_t" },
        };
        for (size_t k = 0; k < 2; k++) {
            if (!node_uses_matrix_kind(program, mkinds[k][0])) continue;
            fprintf(f,
                "static inline %s *clair_rt_matrix_at_%s_checked(ClairMatrix m, int64_t i, int64_t j) {\n"
                "    if ((uint64_t)i >= (uint64_t)m->rows || (uint64_t)j >= (uint64_t)m->cols) clair_rt_matrix_index_error(m, i, j);\n"
                "    return (%s *)m->data + (i * m->cols + j);\n"
                "}\n\n",
                mkinds[k][2], mkinds[k][1], mkinds[k][2]);
        }
    }

    if (program_has_fast_loop(program)) {
        fputs(
            "/* True when c0 + c1 * i stays in [0, limit) for every i in [first, stop). */\n"
            "static inline bool clair_rt_affine_in(int64_t c0, int64_t c1, int64_t first, int64_t stop, int64_t limit) {\n"
            "    if (first >= stop) return true;\n"
            "    int64_t lo, hi;\n"
            "    if (__builtin_mul_overflow(c1, first, &lo) || __builtin_add_overflow(lo, c0, &lo)) return false;\n"
            "    if (__builtin_mul_overflow(c1, stop - 1, &hi) || __builtin_add_overflow(hi, c0, &hi)) return false;\n"
            "    if (lo > hi) { int64_t t = lo; lo = hi; hi = t; }\n"
            "    return lo >= 0 && hi < limit;\n"
            "}\n\n",
            f
        );
        if (node_uses_list(program)) {
            fputs(
                "/* True when list[c0 + c1 * i] is in bounds for every i in [first, stop). */\n"
                "static inline bool clair_rt_affine_ok(ClairList list, int64_t c0, int64_t c1, int64_t first, int64_t stop) {\n"
                "    return list && list->len <= (size_t)INT64_MAX &&\n"
                "           clair_rt_affine_in(c0, c1, first, stop, (int64_t)list->len);\n"
                "}\n\n",
                f
            );
        }
    }

    if (uses_objects) {
        fputs(
            "static inline void *clair_rt_object_make(const void *value, size_t size) {\n"
            "    void *object = clair_rt_arena_alloc(size);\n"
            "    memcpy(object, value, size);\n"
            "    return object;\n"
            "}\n\n",
            f
        );
    }

    if (uses_concat) {
        fputs(
            "static const char *clair_rt_string_concat(const char *left, const char *right) {\n"
            "    size_t left_len = strlen(left);\n"
            "    size_t right_len = strlen(right);\n"
            "    if (right_len > SIZE_MAX - left_len - 32) {\n"
            "        fprintf(stderr, \"Clair runtime error: string concatenation overflow\\n\");\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    size_t total = left_len + right_len;\n"
            "    char *result = clair_rt_arena_alloc(total + 1);\n"
            "    memcpy(result, left, left_len);\n"
            "    memcpy(result + left_len, right, right_len);\n"
            "    result[total] = '\\0';\n"
            "    return result;\n"
            "}\n\n",
            f
        );
    }

    range_counter = 0;
    list_mutation_counter = 0;

    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION || strcmp(fn->text, "main") == 0) continue;  /* main needs no prototype */
        if (!emit_function_signature(f, fn, 1)) {
            fclose(f);
            return 0;
        }
    }
    fputc('\n', f);

    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION) continue;
        if (!emit_function_signature(f, fn, 0)) {
            fclose(f);
            return 0;
        }
        fputs(" {\n", f);

        const AstNode *body = function_body(fn);
        if (!emit_block(f, body, 1)) {
            fclose(f);
            return 0;
        }

        fputs("}\n\n", f);
    }

    fclose(f);
    return 1;
}
