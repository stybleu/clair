#include "codegen_c.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void indent(FILE *f, int depth) {
    for (int i = 0; i < depth; i++) fputs("    ", f);
}

/*
 * Element types of lists and matrices: Clair name, C type, accessor suffix.
 * Machine integers (u8 .. u64, i8 .. i32) are the C fixed-width types.
 */
static const char *const element_kinds[][3] = {
    { "int", "int64_t", "i64" }, { "float", "double", "f64" }, { "float32", "float", "f32" },
    { "bool", "bool", "bool" }, { "string", "const char *", "str" },
    { "i8", "int8_t", "s8" }, { "i16", "int16_t", "s16" }, { "i32", "int32_t", "s32" },
    { "u8", "uint8_t", "u8" }, { "u16", "uint16_t", "u16" }, { "u32", "uint32_t", "u32" }, { "u64", "uint64_t", "u64" },
};
#define ELEMENT_KIND_COUNT (sizeof(element_kinds) / sizeof(element_kinds[0]))

/* row of element_kinds for "list[u8]" / "matrix[u8]" / "matrix_row[u8]", or -1 */
static int element_kind_of(const char *type, const char *prefix) {
    size_t len = strlen(prefix);
    if (!type || strncmp(type, prefix, len) != 0) return -1;
    for (size_t i = 0; i < ELEMENT_KIND_COUNT; i++) {
        size_t n = strlen(element_kinds[i][0]);
        if (strncmp(type + len, element_kinds[i][0], n) == 0 && strcmp(type + len + n, "]") == 0) return (int)i;
    }
    return -1;
}

static int is_list_type(const char *type) { return element_kind_of(type, "list[") >= 0; }

/* u8 .. u64, i8 .. i32 (not int): their bits, 0 otherwise */
static int small_machine_bits(const char *t) {
    if (!t) return 0;
    if (strcmp(t, "i8") == 0 || strcmp(t, "u8") == 0) return 8;
    if (strcmp(t, "i16") == 0 || strcmp(t, "u16") == 0) return 16;
    if (strcmp(t, "i32") == 0 || strcmp(t, "u32") == 0) return 32;
    if (strcmp(t, "u64") == 0) return 64;
    return 0;
}

/* int and the small ones */
static int machine_type_bits(const char *t) {
    if (t && strcmp(t, "int") == 0) return 64;
    return small_machine_bits(t);
}

static int machine_is_unsigned(const char *t) { return small_machine_bits(t) && t[0] == 'u'; }

static int is_dict_type(const char *type) {
    return type && (
        strcmp(type, "dict[string,int]") == 0 ||
        strcmp(type, "dict[string,float]") == 0 ||
        strcmp(type, "dict[string,bool]") == 0 ||
        strcmp(type, "dict[string,string]") == 0
    );
}


static int is_matrix_type(const char *type) {
    int k = element_kind_of(type, "matrix[");
    return k >= 0 && strcmp(element_kinds[k][0], "bool") != 0 && strcmp(element_kinds[k][0], "string") != 0;
}

static int is_matrix_row_type(const char *type) {
    return type && strncmp(type, "matrix_row[", 11) == 0;
}

/* C element type of a matrix or of one of its rows */
static int matrix_kind(const char *type) {
    int k = element_kind_of(type, "matrix[");
    return k >= 0 ? k : element_kind_of(type, "matrix_row[");
}

static const char *matrix_element_c_type(const char *type) {
    int k = matrix_kind(type);
    return k >= 0 ? element_kinds[k][1] : NULL;
}

static const char *matrix_accessor_name(const char *type) {
    static char name[64];
    int k = matrix_kind(type);
    if (k < 0) return NULL;
    snprintf(name, sizeof(name), "clair_rt_matrix_at_%s_checked", element_kinds[k][2]);
    return name;
}

/* Big integers int128 .. int1024 and uint128 .. uint1024: bits, or 0 for any other type. */
static int big_bits(const char *type) {
    if (!type) return 0;
    if (type[0] == 'u') type++;
    if (strcmp(type, "int128") == 0) return 128;
    if (strcmp(type, "int256") == 0) return 256;
    if (strcmp(type, "int512") == 0) return 512;
    if (strcmp(type, "int1024") == 0) return 1024;
    return 0;
}

/* 'i' for int128 .. int1024 (clair_i256_add), 'u' for uint128 .. uint1024 (clair_u256_add) */
static char big_tag(const char *type) {
    return type && type[0] == 'u' ? 'u' : 'i';
}

static const char *c_type_name(const char *clair_type) {
    static char class_type[512];

    if (!clair_type) return NULL;
    if (strcmp(clair_type, "int128") == 0) return "ClairInt128";
    if (strcmp(clair_type, "int256") == 0) return "ClairInt256";
    if (strcmp(clair_type, "int512") == 0) return "ClairInt512";
    if (strcmp(clair_type, "int1024") == 0) return "ClairInt1024";
    if (strcmp(clair_type, "uint128") == 0) return "ClairUInt128";
    if (strcmp(clair_type, "uint256") == 0) return "ClairUInt256";
    if (strcmp(clair_type, "uint512") == 0) return "ClairUInt512";
    if (strcmp(clair_type, "uint1024") == 0) return "ClairUInt1024";
    if (strcmp(clair_type, "int") == 0) return "int64_t";
    if (strcmp(clair_type, "float") == 0) return "double";
    if (strcmp(clair_type, "float32") == 0) return "float";
    if (strcmp(clair_type, "bool") == 0) return "bool";
    if (small_machine_bits(clair_type)) {
        for (size_t i = 0; i < ELEMENT_KIND_COUNT; i++) {
            if (strcmp(clair_type, element_kinds[i][0]) == 0) return element_kinds[i][1];
        }
    }
    if (strcmp(clair_type, "string") == 0) return "const char *";
    if (is_list_type(clair_type)) return "ClairList";
    if (is_dict_type(clair_type)) return "ClairDict";
    if (is_matrix_type(clair_type)) return "ClairMatrix";

    int written = snprintf(class_type, sizeof(class_type), "ClairClass_%s *", clair_type);
    if (written < 0 || (size_t)written >= sizeof(class_type)) return NULL;
    return class_type;
}

static const char *list_element_c_type(const char *list_type) {
    int k = element_kind_of(list_type, "list[");
    return k >= 0 ? element_kinds[k][1] : NULL;
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

/* --sans-verif: lists and matrices are accessed without bounds checks */
static int unchecked_mode = 0;

void codegen_set_unchecked(int unchecked) { unchecked_mode = unchecked; }

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
    if (n->kind == AST_UNARY && n->text && strcmp(n->text, "~") == 0 && n->child_count == 1) {
        int64_t v;
        if (!const_int_value(n->children[0], &v)) return 0;
        *out = ~v;
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
        if (strcmp(n->text, "&") == 0) { *out = a & b; return 1; }
        if (strcmp(n->text, "|") == 0) { *out = a | b; return 1; }
        if (strcmp(n->text, "^") == 0) { *out = a ^ b; return 1; }
        /* 1 << 20: the count must be known and between 0 and 63 */
        if (b >= 0 && b < 64 && strcmp(n->text, "<<") == 0) { *out = (int64_t)(ua << b); return 1; }
        if (b >= 0 && b < 64 && strcmp(n->text, ">>") == 0) { *out = a >> b; return 1; }
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
    if (strcmp(call->text, "@convert") == 0) return 0;     /* narrowing big integers is checked */
    if (strncmp(call->text, "@slice", 6) == 0) return 0;   /* a step of 0 stops the program */
    if (big_bits(call->type_name)) return 0;               /* abs, min, max, pow of big integers */
    if (strcmp(call->text, "@int") == 0 || small_machine_bits(call->text + 1)) {
        /* int(x) and u8(x) of a float, and int(x) of a u64, are checked */
        const char *t = call->child_count == 1 ? call->children[0]->type_name : NULL;
        if (call->child_count != 1) return 0;
        if (t && strcmp(call->text, "@int") == 0 && strcmp(t, "u64") == 0) return 0;
        return !(t && (strcmp(t, "float") == 0 || strcmp(t, "float32") == 0));
    }
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

/* Inside a copy specialised for a known row width, clair_cols_<base> is printed as that width. */
typedef struct { const char *base; int64_t width; } WidthOverride;
static WidthOverride width_override[FAST_MAX];
static size_t width_override_count = 0;

static void emit_cols(FILE *f, const char *base) {
    for (size_t i = 0; i < width_override_count; i++) {
        if (strcmp(width_override[i].base, base) == 0) {
            fprintf(f, "%lld", (long long)width_override[i].width);
            return;
        }
    }
    fprintf(f, "clair_cols_%s", base);
}

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

/* Functions that may write an element of a list or matrix (directly or through calls). */
static const char *list_writer_fn[SAFE_FN_MAX];
static size_t list_writer_count = 0;

static int fn_writes_lists(const char *name) {
    if (list_writer_count >= SAFE_FN_MAX) return 1;
    for (size_t i = 0; i < list_writer_count; i++) {
        if (strcmp(list_writer_fn[i], name) == 0) return 1;
    }
    return 0;
}

static int tree_writes_lists(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_INDEX_ASSIGN || n->kind == AST_MATRIX_SET || n->kind == AST_LIST_APPEND ||
        n->kind == AST_METHOD_CALL) return 1;
    if (n->kind == AST_CALL && n->text && n->text[0] != '@' && strcmp(n->text, "len") != 0 &&
        strcmp(n->text, "input_int") != 0 && fn_writes_lists(n->text)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (tree_writes_lists(n->children[i])) return 1;
    }
    return 0;
}

static void compute_list_writers(const AstNode *program) {
    list_writer_count = 0;
    int changed = 1;
    while (changed) {
        changed = 0;
        for (size_t i = 0; i < program->child_count; i++) {
            const AstNode *fn = program->children[i];
            if (fn->kind != AST_FUNCTION || !fn->text || fn_writes_lists(fn->text)) continue;
            if (tree_writes_lists(fn)) {
                if (list_writer_count < SAFE_FN_MAX) list_writer_fn[list_writer_count++] = fn->text;
                changed = 1;
            }
        }
    }
}

static void compute_safe_functions(const AstNode *program) {
    compute_field_writers(program);
    compute_list_writers(program);
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
    if ((n->kind == AST_ASSIGN || n->kind == AST_LET || n->kind == AST_FOR || n->kind == AST_FOR_EACH) &&
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
        if (c->kind == AST_FOR || c->kind == AST_WHILE || c->kind == AST_FOR_EACH) return 1;
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
    char *c1;       /* coefficient of the loop variable */
    char *c2;       /* coefficient of the inner loop variable (double loops), or NULL */
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
    const AstNode *inner_loop;         /* double loop: the inner for, whose range is checked too */
} FastPlan;

/* Set while emitting the unchecked copy of a double loop: its inner loop needs no plan of its own. */
static int fast_suppress = 0;

static void fast_plan_free(FastPlan *plan) {
    for (size_t i = 0; i < plan->check_count; i++) {
        free(plan->checks[i].c0);
        free(plan->checks[i].c1);
        free(plan->checks[i].c2);
    }
    for (size_t i = 0; i < plan->owned_count; i++) free(plan->owned[i]);
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
    plan->inner_loop = NULL;
}

static const char *fast_plan_expr(const FastPlan *plan, const char *base) {
    for (size_t i = 0; i < plan->base_count; i++) {
        if (strcmp(plan->bases[i], base) == 0) return plan->base_exprs[i];
    }
    return base;
}

static void fast_plan_add_check(FastPlan *plan, const char *base, const char *type,
                                char *c0, char *c1, char *c2, int dim) {
    for (size_t i = 0; i < plan->check_count; i++) {
        BoundsCheck *b = &plan->checks[i];
        if (b->dim == dim && strcmp(b->base, base) == 0 && strcmp(b->c0, c0) == 0 && strcmp(b->c1, c1) == 0 &&
            ((!b->c2 && !c2) || (b->c2 && c2 && strcmp(b->c2, c2) == 0))) {
            free(c0); free(c1); free(c2);
            return;
        }
    }
    plan->checks[plan->check_count++] = (BoundsCheck){ base, type, c0, c1, c2, dim };
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
                                 const char *type, char *r0, char *r1, char *r2, char *k0, char *k1, char *k2) {
    if (plan->access_count >= FAST_MAX || plan->check_count + 2 > FAST_MAX || plan->base_count >= FAST_MAX) {
        free(r0); free(r1); free(r2); free(k0); free(k1); free(k2);
        return;
    }
    plan->accesses[plan->access_count++] = (FastAccess){ node, base };
    fast_plan_add_base(plan, base, expr, type);
    fast_plan_add_check(plan, base, type, r0, r1, r2, 1);
    fast_plan_add_check(plan, base, type, k0, k1, k2, 2);
}

static void fast_plan_add(FastPlan *plan, const AstNode *node, const char *base, const char *expr,
                          const char *list_type, char *c0, char *c1, char *c2) {
    if (plan->access_count >= FAST_MAX || plan->check_count >= FAST_MAX || plan->base_count >= FAST_MAX) {
        free(c0); free(c1); free(c2);
        return;
    }
    plan->accesses[plan->access_count++] = (FastAccess){ node, base };
    fast_plan_add_base(plan, base, expr, list_type);
    fast_plan_add_check(plan, base, list_type, c0, c1, c2, 0);
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
        if (ok && affine_split(index, v, body, &c0, &c1)) fast_plan_add(plan, n, name, expr, list_type, c0, c1, NULL);
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
                fast_plan_add_matrix(plan, n, name, expr, mtype, r0, r1, NULL, k0, k1, NULL);
            } else {
                free(r0); free(r1);
            }
        }
    }

    for (size_t i = 0; i < n->child_count; i++) fast_plan_collect(plan, n->children[i], v, body);
}

/*
 * When every access to matrix `base` in the loop uses a constant column
 * (m[i][0], m[i][1]) the loop likely walks a fixed-width matrix (2-D points,
 * RGB...).  Returns that width (largest column + 1), or 0.
 */
static int64_t fast_plan_const_width(const FastPlan *plan, const char *base) {
    int64_t width = 0;
    for (size_t i = 0; i < plan->access_count; i++) {
        const FastAccess *a = &plan->accesses[i];
        if (strcmp(a->base, base) != 0) continue;
        const AstNode *col = NULL;
        if (a->node->kind == AST_MATRIX_SET) col = a->node->children[1];
        else if (a->node->kind == AST_INDEX && a->node->children[0]->kind == AST_INDEX) col = a->node->children[1];
        else return 0;
        int64_t k;
        if (!const_int_value(col, &k) || k < 0 || k > 64) return 0;
        if (k + 1 > width) width = k + 1;
    }
    return width;
}

/* Builds the plan for one `for` loop; returns 0 when the loop must stay checked only. */
static int fast_plan_for(const AstNode *loop, FastPlan *plan) {
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
    plan->inner_loop = NULL;
    if (fast_suppress) return 0;
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

/*
 * The counting loop written by hand:   while v < stop { ...; v = v + 1 }
 * v changes only through that last statement, stop never changes, so the
 * body sees v going through [v at entry, stop) exactly like a for loop.
 */
static int while_counter_pattern(const AstNode *loop, const char **var, const AstNode **stop) {
    if (!loop || loop->kind != AST_WHILE || loop->child_count != 2) return 0;
    const AstNode *cond = loop->children[0], *body = loop->children[1];
    if (cond->kind != AST_BINARY || !cond->text || strcmp(cond->text, "<") != 0 || cond->child_count != 2) return 0;
    const AstNode *lhs = cond->children[0], *rhs = cond->children[1];
    if (lhs->kind != AST_IDENTIFIER || !lhs->text || !is_int_type(lhs->type_name)) return 0;
    if (!body || body->kind != AST_BLOCK || body->child_count < 2) return 0;

    const AstNode *last = body->children[body->child_count - 1];
    if (last->kind != AST_ASSIGN || !last->text || strcmp(last->text, lhs->text) != 0 || last->child_count != 1) return 0;
    const AstNode *inc = last->children[0];
    int64_t one;
    if (inc->kind != AST_BINARY || !inc->text || strcmp(inc->text, "+") != 0) return 0;
    int v_left = inc->children[0]->kind == AST_IDENTIFIER && strcmp(inc->children[0]->text, lhs->text) == 0 &&
                 const_int_value(inc->children[1], &one) && one == 1;
    int v_right = inc->children[1]->kind == AST_IDENTIFIER && strcmp(inc->children[1]->text, lhs->text) == 0 &&
                  const_int_value(inc->children[0], &one) && one == 1;
    if (!v_left && !v_right) return 0;

    for (size_t i = 0; i + 1 < body->child_count; i++) {
        if (name_changes_in(body->children[i], lhs->text)) return 0;
    }

    int64_t k;
    int stable = const_int_value(rhs, &k) ||
        (rhs->kind == AST_IDENTIFIER && rhs->text && strcmp(rhs->text, lhs->text) != 0 &&
         !name_changes_in(body, rhs->text)) ||
        (rhs->kind == AST_CALL && rhs->text && strcmp(rhs->text, "len") == 0 && rhs->child_count == 1 &&
         rhs->children[0]->kind == AST_IDENTIFIER && is_list_type(rhs->children[0]->type_name) &&
         !name_changes_in(body, rhs->children[0]->text));
    if (!stable) return 0;

    *var = lhs->text;
    *stop = rhs;
    return 1;
}

static int fast_plan_while(const AstNode *loop, FastPlan *plan, const char **var, const AstNode **stop) {
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
    plan->inner_loop = NULL;
    if (fast_suppress) return 0;
    if (!while_counter_pattern(loop, var, stop)) return 0;
    const AstNode *body = loop->children[1];
    if (has_nested_loop(body)) return 0;
    if (tree_may_grow_lists(body)) return 0;   /* also keeps len(list) in the condition stable */

    fast_plan_collect(plan, body, *var, body);
    if (plan->access_count == 0) { fast_plan_free(plan); return 0; }
    return 1;
}


/* ------------------------------------------------------------------ */
/* Double loops: one bounds proof for  for k { ...; for j { ... } }   */
/* ------------------------------------------------------------------ */

/* x op y on coefficient strings ("0" is zero), folding literals. */
static char *coef_add(const char *a, const char *b, int subtract) {
    char *folded = fold_strings(a, subtract ? '-' : '+', b);
    if (folded) return folded;
    if (str_is_zero(b)) return str_dup(a);
    char *pb = paren_if_needed(b);
    char *r = str_is_zero(a) ? (subtract ? str_fmt("-%s", pb) : str_dup(b))
                             : str_fmt("%s %s %s", a, subtract ? "-" : "+", pb);
    free(pb);
    return r;
}

static char *coef_mul(const char *k, const char *x) {
    char *folded = fold_strings(k, '*', x);
    if (folded) return folded;
    if (str_is_zero(k) || str_is_zero(x)) return str_dup("0");
    if (str_is_one(k)) return str_dup(x);
    if (str_is_one(x)) return str_dup(k);
    char *pk = paren_if_needed(k), *px = paren_if_needed(x);
    char *r = str_fmt("%s * %s", pk, px);
    free(pk); free(px);
    return r;
}

static void coef_free3(char *c[3]) { for (int i = 0; i < 3; i++) { free(c[i]); c[i] = NULL; } }

/*
 * Splits an index into c[0] + c[1] * k + c[2] * j with coefficients that do
 * not change in the outer loop body (same rules as affine_split).
 */
static int affine2_split(const AstNode *e, const char *k, const char *j, const AstNode *body, char *c[3]) {
    c[0] = c[1] = c[2] = NULL;
    if (!e || !e->type_name || strcmp(e->type_name, "int") != 0) return 0;

    int64_t lit;
    if (const_int_value(e, &lit)) {
        if (lit == INT64_MIN) return 0;
        c[0] = str_fmt("%lld", (long long)lit); c[1] = str_dup("0"); c[2] = str_dup("0");
        return 1;
    }
    if (e->kind == AST_IDENTIFIER && e->text) {
        if (strcmp(e->text, k) == 0) { c[0] = str_dup("0"); c[1] = str_dup("1"); c[2] = str_dup("0"); return 1; }
        if (strcmp(e->text, j) == 0) { c[0] = str_dup("0"); c[1] = str_dup("0"); c[2] = str_dup("1"); return 1; }
        if (name_changes_in(body, e->text)) return 0;
        c[0] = str_dup(e->text); c[1] = str_dup("0"); c[2] = str_dup("0");
        return 1;
    }
    if (e->kind == AST_UNARY && e->text && strcmp(e->text, "-") == 0 && e->child_count == 1) {
        char *a[3];
        if (!affine2_split(e->children[0], k, j, body, a)) return 0;
        for (int i = 0; i < 3; i++) c[i] = coef_add("0", a[i], 1);
        coef_free3(a);
        return 1;
    }
    if (e->kind == AST_BINARY && e->text && e->child_count == 2) {
        int add = strcmp(e->text, "+") == 0, sub = strcmp(e->text, "-") == 0, mul = strcmp(e->text, "*") == 0;
        if (!add && !sub && !mul) return 0;
        char *a[3], *b[3];
        if (!affine2_split(e->children[0], k, j, body, a)) return 0;
        if (!affine2_split(e->children[1], k, j, body, b)) { coef_free3(a); return 0; }
        int ok = 1;
        if (add || sub) {
            for (int i = 0; i < 3; i++) c[i] = coef_add(a[i], b[i], sub);
        } else if (str_is_zero(a[1]) && str_is_zero(a[2])) {
            for (int i = 0; i < 3; i++) c[i] = coef_mul(a[0], b[i]);
        } else if (str_is_zero(b[1]) && str_is_zero(b[2])) {
            for (int i = 0; i < 3; i++) c[i] = coef_mul(b[0], a[i]);
        } else {
            ok = 0;   /* k * j is not affine */
        }
        coef_free3(a); coef_free3(b);
        if (!ok || !c[0] || !c[1] || !c[2]) { coef_free3(c); return 0; }
        return 1;
    }
    return 0;
}

/* A range bound the outer loop cannot change and that cannot fail: literals, stable names, len/rows/cols. */
static int nest_bound_is_invariant(const AstNode *e, const char *k, const AstNode *body) {
    if (!e) return 1;   /* omitted start: 0 */
    int64_t lit;
    if (const_int_value(e, &lit)) return 1;
    if (e->kind == AST_IDENTIFIER && e->text) return strcmp(e->text, k) != 0 && !name_changes_in(body, e->text);
    if (e->kind == AST_CALL && e->text && e->child_count == 1 &&
        (strcmp(e->text, "len") == 0 || strcmp(e->text, "@rows") == 0 || strcmp(e->text, "@cols") == 0)) {
        const AstNode *arg = e->children[0];
        if (arg->kind == AST_IDENTIFIER) return strcmp(arg->text, k) != 0 && !name_changes_in(body, arg->text);
        if (arg->kind == AST_MEMBER) {
            if (tree_calls_field_writer(body)) return 0;
            const AstNode *cur = arg;
            while (cur->kind == AST_MEMBER && cur->child_count == 1) {
                if (tree_assigns_field(body, cur->text)) return 0;
                cur = cur->children[0];
            }
            return cur->kind == AST_IDENTIFIER && strcmp(cur->text, k) != 0 && !name_changes_in(body, cur->text);
        }
        return 0;
    }
    if (e->kind == AST_BINARY && e->text && e->child_count == 2 &&
        (strcmp(e->text, "+") == 0 || strcmp(e->text, "-") == 0 || strcmp(e->text, "*") == 0)) {
        return nest_bound_is_invariant(e->children[0], k, body) && nest_bound_is_invariant(e->children[1], k, body);
    }
    return 0;
}

static const AstNode *nest_find_inner(const AstNode *n, size_t *count) {
    const AstNode *found = NULL;
    for (size_t i = 0; i < n->child_count; i++) {
        const AstNode *c = n->children[i];
        if (c->kind == AST_FOR || c->kind == AST_WHILE || c->kind == AST_FOR_EACH) {
            (*count)++;
            found = c;
            continue;   /* its own body is checked by the caller */
        }
        const AstNode *deeper = nest_find_inner(c, count);
        if (deeper) found = deeper;
    }
    return found;
}

static void nest_collect(FastPlan *plan, const AstNode *n, const char *k, const char *j,
                         const AstNode *outer_body, int inside) {
    if (!n) return;
    if (!inside && n == plan->inner_loop) {
        const AstNode *inner_body = n->children[n->child_count - 1];
        nest_collect(plan, inner_body, k, j, outer_body, 1);
        return;
    }
    if (!inside) {
        /* statements of the outer body: indices in k only, like a simple loop */
        const AstNode *index = NULL, *base_node = NULL;
        const char *list_type = NULL;
        if (n->kind == AST_INDEX && n->child_count == 2 && is_list_type(n->children[0]->type_name)) {
            base_node = n->children[0]; list_type = base_node->type_name; index = n->children[1];
        } else if (n->kind == AST_INDEX_ASSIGN && is_list_type(n->type_name) &&
                   ((n->child_count == 2 && n->text) || (n->child_count == 3 && !n->text))) {
            list_type = n->type_name; index = n->children[0]; base_node = n->text ? NULL : n->children[2];
        }
        if (list_type) {
            const char *name = NULL, *expr = NULL;
            int ok = base_node ? fast_base_of(plan, base_node, k, outer_body, &name, &expr)
                               : (!name_changes_in(outer_body, n->text) && (name = expr = n->text) != NULL);
            char *c0, *c1;
            if (ok && affine_split(index, k, outer_body, &c0, &c1)) fast_plan_add(plan, n, name, expr, list_type, c0, c1, NULL);
        }
        const AstNode *row = NULL, *col = NULL, *mnode = NULL;
        const char *mtype = NULL;
        if (n->kind == AST_INDEX && n->child_count == 2 && n->children[0]->kind == AST_INDEX &&
            is_matrix_row_type(n->children[0]->type_name)) {
            mnode = n->children[0]->children[0]; mtype = mnode->type_name;
            row = n->children[0]->children[1]; col = n->children[1];
        } else if (n->kind == AST_MATRIX_SET && is_matrix_type(n->type_name) &&
                   ((n->child_count == 3 && n->text) || (n->child_count == 4 && !n->text))) {
            mnode = n->text ? NULL : n->children[3]; mtype = n->type_name;
            row = n->children[0]; col = n->children[1];
        }
        if (mtype) {
            const char *name = NULL, *expr = NULL;
            int ok = mnode ? fast_base_of(plan, mnode, k, outer_body, &name, &expr)
                           : (!name_changes_in(outer_body, n->text) && (name = expr = n->text) != NULL);
            char *r0, *r1, *k0, *k1;
            if (ok && affine_split(row, k, outer_body, &r0, &r1)) {
                if (affine_split(col, k, outer_body, &k0, &k1)) fast_plan_add_matrix(plan, n, name, expr, mtype, r0, r1, NULL, k0, k1, NULL);
                else { free(r0); free(r1); }
            }
        }
    } else {
        /* statements of the inner loop: indices in k and j */
        const AstNode *index = NULL, *base_node = NULL;
        const char *list_type = NULL;
        if (n->kind == AST_INDEX && n->child_count == 2 && is_list_type(n->children[0]->type_name)) {
            base_node = n->children[0]; list_type = base_node->type_name; index = n->children[1];
        } else if (n->kind == AST_INDEX_ASSIGN && is_list_type(n->type_name) &&
                   ((n->child_count == 2 && n->text) || (n->child_count == 3 && !n->text))) {
            list_type = n->type_name; index = n->children[0]; base_node = n->text ? NULL : n->children[2];
        }
        if (list_type) {
            const char *name = NULL, *expr = NULL;
            int ok = base_node ? fast_base_of(plan, base_node, k, outer_body, &name, &expr)
                               : (!name_changes_in(outer_body, n->text) && strcmp(n->text, j) != 0 &&
                                  (name = expr = n->text) != NULL);
            if (ok && base_node && base_node->kind == AST_IDENTIFIER && strcmp(base_node->text, j) == 0) ok = 0;
            char *c[3];
            if (ok && affine2_split(index, k, j, outer_body, c)) fast_plan_add(plan, n, name, expr, list_type, c[0], c[1], c[2]);
        }
        const AstNode *row = NULL, *col = NULL, *mnode = NULL;
        const char *mtype = NULL;
        if (n->kind == AST_INDEX && n->child_count == 2 && n->children[0]->kind == AST_INDEX &&
            is_matrix_row_type(n->children[0]->type_name)) {
            mnode = n->children[0]->children[0]; mtype = mnode->type_name;
            row = n->children[0]->children[1]; col = n->children[1];
        } else if (n->kind == AST_MATRIX_SET && is_matrix_type(n->type_name) &&
                   ((n->child_count == 3 && n->text) || (n->child_count == 4 && !n->text))) {
            mnode = n->text ? NULL : n->children[3]; mtype = n->type_name;
            row = n->children[0]; col = n->children[1];
        }
        if (mtype) {
            const char *name = NULL, *expr = NULL;
            int ok = mnode ? fast_base_of(plan, mnode, k, outer_body, &name, &expr)
                           : (!name_changes_in(outer_body, n->text) && strcmp(n->text, j) != 0 &&
                              (name = expr = n->text) != NULL);
            if (ok && mnode && mnode->kind == AST_IDENTIFIER && strcmp(mnode->text, j) == 0) ok = 0;
            char *r[3], *q[3];
            if (ok && affine2_split(row, k, j, outer_body, r)) {
                if (affine2_split(col, k, j, outer_body, q)) fast_plan_add_matrix(plan, n, name, expr, mtype, r[0], r[1], r[2], q[0], q[1], q[2]);
                else coef_free3(r);
            }
        }
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (n->children[i] == plan->inner_loop && inside) continue;
        nest_collect(plan, n->children[i], k, j, outer_body, inside);
    }
}

/*
 * for k in range(a, b) { ...; for j in range(c, d) { ... } }
 * One proof before the outer loop covers every access of both loops, so the
 * inner loop runs without any check at all (its range must not depend on k).
 */
static int fast_plan_nest(const AstNode *loop, FastPlan *plan) {
    plan->check_count = plan->access_count = plan->base_count = plan->owned_count = 0;
    plan->inner_loop = NULL;
    if (fast_suppress) return 0;
    if (!loop || loop->kind != AST_FOR || !loop->text) return 0;
    if (loop->child_count != 2 && loop->child_count != 3) return 0;
    const char *k = loop->text;
    const AstNode *body = loop->children[loop->child_count - 1];

    size_t loops = 0;
    const AstNode *inner = nest_find_inner(body, &loops);
    if (loops != 1 || !inner || inner->kind != AST_FOR || !inner->text) return 0;
    if (inner->child_count != 2 && inner->child_count != 3) return 0;      /* inner step 1 */
    const char *j = inner->text;
    const AstNode *inner_body = inner->children[inner->child_count - 1];
    if (has_nested_loop(inner_body)) return 0;
    if (strcmp(j, k) == 0) return 0;
    if (tree_may_grow_lists(body)) return 0;
    if (name_changes_in(body, k)) return 0;
    if (name_changes_in(inner_body, j)) return 0;

    size_t argc = inner->child_count - 1;
    const AstNode *start = argc == 1 ? NULL : inner->children[0];
    const AstNode *stop = inner->children[argc == 1 ? 0 : 1];
    if (!nest_bound_is_invariant(start, k, body) || !nest_bound_is_invariant(stop, k, body)) return 0;

    plan->inner_loop = inner;
    nest_collect(plan, body, k, j, body, 0);

    int inner_accesses = 0;
    for (size_t i = 0; i < plan->check_count; i++) if (plan->checks[i].c2) inner_accesses = 1;
    if (!inner_accesses) { fast_plan_free(plan); return 0; }
    return 1;
}

static int program_has_nest_loop(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_FOR) {
        FastPlan plan;
        int ok = fast_plan_nest(n, &plan);
        fast_plan_free(&plan);
        if (ok) return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (program_has_nest_loop(n->children[i])) return 1;
    }
    return 0;
}

static int program_has_fast_loop(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_FOR || n->kind == AST_WHILE) {
        FastPlan plan;
        const char *var;
        const AstNode *stop;
        int ok = n->kind == AST_FOR ? (fast_plan_nest(n, &plan) || fast_plan_for(n, &plan))
                                    : fast_plan_while(n, &plan, &var, &stop);
        fast_plan_free(&plan);
        if (ok) return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (program_has_fast_loop(n->children[i])) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Matrix product: the triple loop                                    */
/*     for i { for k { let aik = a[i][k]; for j {                     */
/*         c[i][j] = c[i][j] + aik * b[k][j] } } }                    */
/* is replaced by a blocked kernel doing the same additions in the    */
/* same order for every c[i][j]: the result is identical to the bit.  */
/* ------------------------------------------------------------------ */

/* "matrix[" + elem + "]": the kernels exist for float (double) and float32 (float) */
static int has_elem(const char *type, const char *prefix, const char *elem) {
    size_t lp = strlen(prefix), le = strlen(elem);
    return type && strncmp(type, prefix, lp) == 0 && strncmp(type + lp, elem, le) == 0 && strcmp(type + lp + le, "]") == 0;
}

/* float or float32: the element type of a kernel, or NULL */
static const char *kernel_elem(const char *type) {
    if (!type) return NULL;
    if (strcmp(type, "float") == 0) return "float";
    if (strcmp(type, "float32") == 0) return "float32";
    return NULL;
}

typedef struct {
    int list_form;                      /* 0: matrices m[i][j], 1: flat lists x[i * ld + j] */
    const char *elem;                   /* float or float32 */
    const char *i, *k, *j;
    const AstNode *m_stop, *k_stop, *n_stop;
    const char *a, *b, *c;
    const AstNode *lda, *ldb, *ldc;     /* list form: row lengths (identifier or literal) */
} MatmulPattern;

static int matmul_suppress = 0;         /* set while emitting the plain loop kept as fallback */

static int is_ident(const AstNode *e, const char *name) {
    return e && e->kind == AST_IDENTIFIER && e->text && (!name || strcmp(e->text, name) == 0);
}

static int is_binary(const AstNode *e, const char *op) {
    return e && e->kind == AST_BINARY && e->text && strcmp(e->text, op) == 0 && e->child_count == 2;
}

/* range(n) or range(0, n): the stop expression, or NULL */
static const AstNode *zero_based_stop(const AstNode *loop) {
    int64_t v;
    if (loop->child_count == 2) return loop->children[0];
    if (loop->child_count == 3 && const_int_value(loop->children[0], &v) && v == 0) return loop->children[1];
    return NULL;
}

/* row * ld + col, in any operand order; ld is an int variable or an int literal */
static const AstNode *flat_index_ld(const AstNode *e, const char *row, const char *col) {
    if (!is_binary(e, "+")) return NULL;
    for (int s = 0; s < 2; s++) {
        const AstNode *mul = e->children[s];
        if (!is_ident(e->children[1 - s], col) || !is_binary(mul, "*")) continue;
        for (int t = 0; t < 2; t++) {
            const AstNode *ld = mul->children[1 - t];
            int64_t v;
            if (is_ident(mul->children[t], row) &&
                ((is_ident(ld, NULL) && is_int_type(ld->type_name)) || const_int_value(ld, &v))) return ld;
        }
    }
    return NULL;
}

static int same_ld(const AstNode *x, const AstNode *y) {
    int64_t a, b;
    if (is_ident(x, NULL) && is_ident(y, NULL)) return strcmp(x->text, y->text) == 0;
    return const_int_value(x, &a) && const_int_value(y, &b) && a == b;
}

/* name of m when e is m[row][col] with m a matrix[float] (or matrix[float32]) variable */
static const char *matrix_read(const AstNode *e, const char *row, const char *col, const char *elem) {
    if (!e || e->kind != AST_INDEX || e->child_count != 2 || !is_ident(e->children[1], col)) return NULL;
    const AstNode *r = e->children[0];
    if (r->kind != AST_INDEX || r->child_count != 2 || !is_ident(r->children[1], row)) return NULL;
    const AstNode *m = r->children[0];
    if (!is_ident(m, NULL) || !has_elem(m->type_name, "matrix[", elem)) return NULL;
    return m->text;
}

/* name of x when e is x[row * ld + col] with x a list[float] variable */
static const char *list_read(const AstNode *e, const char *row, const char *col, const AstNode **ld, const char *elem) {
    if (!e || e->kind != AST_INDEX || e->child_count != 2) return NULL;
    const AstNode *x = e->children[0];
    if (!is_ident(x, NULL) || !has_elem(x->type_name, "list[", elem)) return NULL;
    *ld = flat_index_ld(e->children[1], row, col);
    return *ld ? x->text : NULL;
}

static const char *element_read(const MatmulPattern *p, const AstNode *e, const char *row, const char *col,
                                const AstNode **ld) {
    *ld = NULL;
    return p->list_form ? list_read(e, row, col, ld, p->elem) : matrix_read(e, row, col, p->elem);
}

/* A range bound that cannot change in the loops and cannot fail. */
static int matmul_bound_ok(const AstNode *e, const MatmulPattern *p, const char *aik) {
    int64_t v;
    if (const_int_value(e, &v)) return 1;
    if (is_ident(e, NULL)) {
        return is_int_type(e->type_name) && strcmp(e->text, p->i) != 0 && strcmp(e->text, p->k) != 0 &&
               strcmp(e->text, p->j) != 0 && (!aik || strcmp(e->text, aik) != 0);
    }
    if (e->kind == AST_CALL && e->text && e->child_count == 1 &&
        (strcmp(e->text, "len") == 0 || strcmp(e->text, "@rows") == 0 || strcmp(e->text, "@cols") == 0)) {
        return is_ident(e->children[0], NULL);
    }
    if (is_binary(e, "+") || is_binary(e, "-") || is_binary(e, "*")) {
        return matmul_bound_ok(e->children[0], p, aik) && matmul_bound_ok(e->children[1], p, aik);
    }
    return 0;
}

static int matmul_pattern(const AstNode *outer, MatmulPattern *p) {
    memset(p, 0, sizeof(*p));
    if (!outer || outer->kind != AST_FOR || !outer->text) return 0;
    p->i = outer->text;
    p->m_stop = zero_based_stop(outer);
    const AstNode *ob = outer->children[outer->child_count - 1];
    if (!p->m_stop || !ob || ob->kind != AST_BLOCK || ob->child_count != 1) return 0;

    const AstNode *kl = ob->children[0];
    if (kl->kind != AST_FOR || !kl->text) return 0;
    p->k = kl->text;
    p->k_stop = zero_based_stop(kl);
    const AstNode *kb = kl->children[kl->child_count - 1];
    if (!p->k_stop || !kb || kb->kind != AST_BLOCK || (kb->child_count != 1 && kb->child_count != 2)) return 0;

    const AstNode *jl = kb->children[kb->child_count - 1];
    if (jl->kind != AST_FOR || !jl->text) return 0;
    p->j = jl->text;
    p->n_stop = zero_based_stop(jl);
    const AstNode *jb = jl->children[jl->child_count - 1];
    if (!p->n_stop || !jb || jb->kind != AST_BLOCK || jb->child_count != 1) return 0;
    if (strcmp(p->i, p->k) == 0 || strcmp(p->i, p->j) == 0 || strcmp(p->k, p->j) == 0) return 0;

    /* c[i][j] = value  or  c[i * ldc + j] = value */
    const AstNode *upd = jb->children[0], *value = NULL;
    p->elem = has_elem(upd->type_name, "matrix[", "float32") || has_elem(upd->type_name, "list[", "float32")
            ? "float32" : "float";
    if (upd->kind == AST_MATRIX_SET && upd->text && upd->child_count == 3 && upd->type_name &&
        has_elem(upd->type_name, "matrix[", p->elem) &&
        is_ident(upd->children[0], p->i) && is_ident(upd->children[1], p->j)) {
        p->list_form = 0;
        value = upd->children[2];
    } else if (upd->kind == AST_INDEX_ASSIGN && upd->text && upd->child_count == 2 && upd->type_name &&
               has_elem(upd->type_name, "list[", p->elem)) {
        p->list_form = 1;
        p->ldc = flat_index_ld(upd->children[0], p->i, p->j);
        if (!p->ldc) return 0;
        value = upd->children[1];
    } else {
        return 0;
    }
    p->c = upd->text;

    /* optional  let aik: float = a[i][k] */
    const char *aik = NULL;
    if (kb->child_count == 2) {
        const AstNode *let = kb->children[0];
        if (let->kind != AST_LET || !let->text || let->child_count != 1 || !let->type_name ||
            strcmp(let->type_name, p->elem) != 0) return 0;
        aik = let->text;
        p->a = element_read(p, let->children[0], p->i, p->k, &p->lda);
        if (!p->a) return 0;
        if (strcmp(aik, p->i) == 0 || strcmp(aik, p->k) == 0 || strcmp(aik, p->j) == 0) return 0;
    }

    /* value: c[i][j] + aik * b[k][j], operands of + and * in any order */
    if (!is_binary(value, "+")) return 0;
    int found = 0;
    for (int s = 0; s < 2 && !found; s++) {
        const AstNode *ld = NULL;
        const char *cr = element_read(p, value->children[s], p->i, p->j, &ld);
        const AstNode *prod = value->children[1 - s];
        if (!cr || strcmp(cr, p->c) != 0 || (p->list_form && !same_ld(ld, p->ldc)) || !is_binary(prod, "*")) continue;
        for (int t = 0; t < 2 && !found; t++) {
            const AstNode *left = prod->children[t], *right = prod->children[1 - t];
            const AstNode *ldb = NULL, *lda = NULL;
            const char *b = element_read(p, right, p->k, p->j, &ldb);
            if (!b) continue;
            if (aik) {
                if (!is_ident(left, aik)) continue;
            } else {
                p->a = element_read(p, left, p->i, p->k, &lda);
                if (!p->a) continue;
                p->lda = lda;
            }
            p->b = b;
            p->ldb = ldb;
            found = 1;
        }
    }
    if (!found) return 0;
    if (!value->type_name || strcmp(value->type_name, p->elem) != 0) return 0;

    return matmul_bound_ok(p->m_stop, p, aik) && matmul_bound_ok(p->k_stop, p, aik) &&
           matmul_bound_ok(p->n_stop, p, aik) &&
           (!p->list_form || (matmul_bound_ok(p->lda, p, aik) && matmul_bound_ok(p->ldb, p, aik) &&
                              matmul_bound_ok(p->ldc, p, aik)));
}

static int program_has_matmul(const AstNode *n) {
    if (!n) return 0;
    MatmulPattern p;
    if (n->kind == AST_FOR && matmul_pattern(n, &p)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (program_has_matmul(n->children[i])) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Matrix x vector (a dense layer of a neural network):               */
/*     for i { let s = b[i]; for j { s = s + w[i][j] * x[j] }; ... }  */
/* computes every sum first (4 rows at once, on all cores for a big   */
/* layer), then runs the "..." row by row.  Each row keeps its own    */
/* additions in the same order, so the result is identical to the    */
/* bit whatever the number of cores.                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    FastPlan bases;                     /* owns the C expressions of field chains */
    const AstNode *body;
    const char *i, *j, *s;
    const AstNode *m_stop, *n_stop;
    const char *w_name, *w_expr;        /* clair_data_<w_name> and the C expression */
    int w_matrix;                       /* w[i][j], or a flat list w[i * ld + j] */
    const AstNode *ld;
    const char *x_name, *x_expr;
    const char *elem;                   /* float or float32 */
    const AstNode *w_node, *x_node, *b_node;
    const AstNode *init;                /* number, variable, or b[i] */
    const char *b_name, *b_expr;
    const char *targets[FAST_MAX];      /* lists written after the sum: checked distinct from w, x, b */
    size_t target_count;
} MatvecPattern;

static int matvec_suppress = 0;

/* What may follow the sum: anything that cannot change w, x, b or the rows still to compute. */
static int matvec_rest_ok(const AstNode *n, MatvecPattern *p) {
    if (!n) return 1;
    switch (n->kind) {
        case AST_FOR: case AST_WHILE: case AST_FOR_EACH: case AST_BREAK: case AST_CONTINUE:
        case AST_LIST_APPEND: case AST_METHOD_CALL:
            return 0;
        case AST_CALL:
            if (n->text && n->text[0] != '@' && strcmp(n->text, "len") != 0 &&
                strcmp(n->text, "input_int") != 0 && fn_writes_lists(n->text)) return 0;
            break;
        case AST_INDEX_ASSIGN:
        case AST_MATRIX_SET: {
            const char *name = NULL, *expr = NULL;
            if (p->target_count >= FAST_MAX) return 0;
            if (n->text) {
                if (name_changes_in(p->body, n->text) || strcmp(n->text, p->i) == 0) return 0;
                expr = n->text;
            } else {
                size_t plain = n->kind == AST_INDEX_ASSIGN ? 2 : 3;
                if (n->child_count != plain + 1 ||
                    !fast_base_of(&p->bases, n->children[plain], p->i, p->body, &name, &expr)) return 0;
            }
            p->targets[p->target_count++] = expr;
            break;
        }
        default:
            break;
    }
    for (size_t k = 0; k < n->child_count; k++) {
        if (!matvec_rest_ok(n->children[k], p)) return 0;
    }
    return 1;
}

static int matvec_pattern(const AstNode *loop, MatvecPattern *p) {
    memset(p, 0, sizeof(*p));
    if (!loop || loop->kind != AST_FOR || !loop->text) return 0;
    p->i = loop->text;
    p->m_stop = zero_based_stop(loop);
    p->body = loop->children[loop->child_count - 1];
    const AstNode *body = p->body;
    if (!p->m_stop || !body || body->kind != AST_BLOCK || body->child_count < 2) return 0;

    const AstNode *let = body->children[0], *jl = body->children[1];
    if (let->kind != AST_LET || !let->text || let->child_count != 1 || !kernel_elem(let->type_name)) return 0;
    p->elem = kernel_elem(let->type_name);
    p->s = let->text;
    p->init = let->children[0];
    if (jl->kind != AST_FOR || !jl->text) return 0;
    p->j = jl->text;
    p->n_stop = zero_based_stop(jl);
    const AstNode *jb = jl->children[jl->child_count - 1];
    if (!p->n_stop || !jb || jb->kind != AST_BLOCK || jb->child_count != 1) return 0;
    if (strcmp(p->i, p->j) == 0 || strcmp(p->i, p->s) == 0 || strcmp(p->j, p->s) == 0) return 0;

    /* s = s + w[i][j] * x[j], operands of + and * in any order */
    const AstNode *upd = jb->children[0];
    if (upd->kind != AST_ASSIGN || !upd->text || strcmp(upd->text, p->s) != 0 || upd->child_count != 1) return 0;
    const AstNode *sum = upd->children[0];
    if (!is_binary(sum, "+") || !sum->type_name || strcmp(sum->type_name, p->elem) != 0) return 0;
    const AstNode *prod = is_ident(sum->children[0], p->s) ? sum->children[1]
                        : is_ident(sum->children[1], p->s) ? sum->children[0] : NULL;
    if (!is_binary(prod, "*")) return 0;
    const AstNode *wnode = NULL, *xnode = NULL;
    for (int t = 0; t < 2 && !wnode; t++) {
        const AstNode *wr = prod->children[t], *xr = prod->children[1 - t];
        if (!xr || xr->kind != AST_INDEX || xr->child_count != 2 || !is_ident(xr->children[1], p->j) ||
            !has_elem(xr->children[0]->type_name, "list[", p->elem)) continue;
        if (!wr || wr->kind != AST_INDEX || wr->child_count != 2) continue;
        const AstNode *row = wr->children[0];
        if (row->kind == AST_INDEX && row->child_count == 2 && is_ident(row->children[1], p->i) &&
            is_ident(wr->children[1], p->j) && has_elem(row->children[0]->type_name, "matrix[", p->elem)) {
            p->w_matrix = 1;
            wnode = row->children[0];
        } else if (has_elem(wr->children[0]->type_name, "list[", p->elem) &&
                   (p->ld = flat_index_ld(wr->children[1], p->i, p->j)) != NULL) {
            p->w_matrix = 0;
            wnode = wr->children[0];
        }
        if (wnode) xnode = xr->children[0];
    }
    if (!wnode) return 0;
    p->w_node = wnode;
    p->x_node = xnode;
    if (!fast_base_of(&p->bases, wnode, p->i, body, &p->w_name, &p->w_expr)) return 0;
    if (!fast_base_of(&p->bases, xnode, p->i, body, &p->x_name, &p->x_expr)) return 0;
    if (p->ld && is_ident(p->ld, NULL) && (name_changes_in(body, p->ld->text) || strcmp(p->ld->text, p->i) == 0)) return 0;

    /* starting value of each sum: a number, a variable, or b[i] */
    int64_t k;
    const AstNode *init = p->init;
    if (init->kind == AST_FLOAT || const_int_value(init, &k)) {
        /* fine */
    } else if (is_ident(init, NULL)) {
        if (name_changes_in(body, init->text) || strcmp(init->text, p->i) == 0) return 0;
    } else if (init->kind == AST_INDEX && init->child_count == 2 && is_ident(init->children[1], p->i) &&
               has_elem(init->children[0]->type_name, "list[", p->elem)) {
        p->b_node = init->children[0];
        if (!fast_base_of(&p->bases, init->children[0], p->i, body, &p->b_name, &p->b_expr)) return 0;
    } else {
        return 0;
    }

    /* bounds that cannot change, nothing that appends, nothing that rewrites the inputs */
    if (!nest_bound_is_invariant(p->m_stop, p->i, body) || !nest_bound_is_invariant(p->n_stop, p->i, body)) return 0;
    if (tree_may_grow_lists(body)) return 0;
    for (size_t r = 2; r < body->child_count; r++) {
        const AstNode *st = body->children[r];
        if (name_changes_in(st, p->s) || name_changes_in(st, p->i) || name_changes_in(st, p->j)) return 0;
        if (!matvec_rest_ok(st, p)) return 0;
    }
    return 1;
}

/* Same variable or same chain of fields (self.w and self.w). */
static int same_access_path(const AstNode *a, const AstNode *b) {
    while (a && b && a->kind == AST_MEMBER && b->kind == AST_MEMBER && a->child_count == 1 && b->child_count == 1) {
        if (!a->text || !b->text || strcmp(a->text, b->text) != 0) return 0;
        a = a->children[0];
        b = b->children[0];
    }
    return is_ident(a, NULL) && is_ident(b, NULL) && strcmp(a->text, b->text) == 0;
}

/* rows(m) / cols(m) / len(m) of exactly this list or matrix: such a bound needs no check */
static int is_size_of(const AstNode *e, const char *call, const AstNode *base) {
    return e && e->kind == AST_CALL && e->text && strcmp(e->text, call) == 0 && e->child_count == 1 &&
           same_access_path(e->children[0], base);
}

static int program_has_matvec(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_FOR) {
        MatvecPattern p;
        int ok = matvec_pattern(n, &p);
        fast_plan_free(&p.bases);
        if (ok) return 1;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (program_has_matvec(n->children[i])) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* What follows each sum of a dense layer (y[i] = sigmoid(s)...) runs */
/* on every core too when it only computes with numbers and writes    */
/* y[i] for its own row: no output, no list read, nothing that can    */
/* stop the program (integer division, int(), an index out of range). */
/* The user functions called there must be of the same kind.          */
/* ------------------------------------------------------------------ */

#define PURE_MAX 64

static size_t function_param_count(const AstNode *fn);
static const AstNode *function_body(const AstNode *fn);

typedef struct {
    const char *names[PURE_MAX];            /* variables declared so far */
    size_t count;
    int function;                           /* body of a function: return allowed */
    MatvecPattern *layer;                   /* what follows a sum: its i, s and bases */
    const AstNode *captures[PURE_MAX];      /* variables of the enclosing function read there */
    size_t capture_count;
    const char *target_names[PURE_MAX];     /* lists written at [i]: clair_data_<name> */
    const char *target_exprs[PURE_MAX];     /* and their C expression */
    const char *target_types[PURE_MAX];     /* list type */
    size_t target_count;
    const AstNode *writes[PURE_MAX];        /* each y[i] = ... with its list */
    size_t write_target[PURE_MAX];
    size_t write_count;
} PureCtx;

static const char *pure_fn[SAFE_FN_MAX];
static size_t pure_fn_count = 0;

static int machine_form(const AstNode *n);
enum { MFORM_NONE, MFORM_PLAIN, MFORM_CAST, MFORM_CALL };

static int is_scalar_type(const char *t) {
    return t && (strcmp(t, "float") == 0 || strcmp(t, "float32") == 0 || strcmp(t, "int") == 0 || strcmp(t, "bool") == 0);
}

static int fn_is_pure(const char *name) {
    for (size_t i = 0; i < pure_fn_count; i++) {
        if (strcmp(pure_fn[i], name) == 0) return 1;
    }
    return 0;
}

static int pure_declared(const PureCtx *c, const char *name) {
    for (size_t k = c->count; k > 0; k--) {
        if (strcmp(c->names[k - 1], name) == 0) return 1;
    }
    return 0;
}

static int pure_expr(const AstNode *n, PureCtx *c) {
    if (!n || !is_scalar_type(n->type_name)) return 0;
    switch (n->kind) {
        case AST_INTEGER: case AST_FLOAT: case AST_BOOL:
            return 1;
        case AST_IDENTIFIER:
            if (!n->text) return 0;
            if (pure_declared(c, n->text)) return 1;
            if (!c->layer) return 0;
            if (strcmp(n->text, c->layer->s) == 0 || strcmp(n->text, c->layer->i) == 0) return 1;
            for (size_t k = 0; k < c->capture_count; k++) {
                if (strcmp(c->captures[k]->text, n->text) == 0) return 1;
            }
            if (c->capture_count >= PURE_MAX) return 0;
            c->captures[c->capture_count++] = n;
            return 1;
        case AST_UNARY:
            return n->child_count == 1 && pure_expr(n->children[0], c);
        case AST_BINARY:
            if (n->child_count != 2 || !n->text || !pure_expr(n->children[0], c) || !pure_expr(n->children[1], c)) return 0;
            if ((strcmp(n->text, "/") == 0 || strcmp(n->text, "%") == 0) && strcmp(n->type_name, "int") == 0) {
                return int_div_is_safe(n);
            }
            return machine_form(n) != MFORM_CALL;      /* x << n: n may be negative */
        case AST_CALL: {
            if (!n->text) return 0;
            if (n->text[0] == '@') {
                static const char *const math[] = {
                    "sqrt", "exp", "log", "log10", "log2", "sin", "cos", "tan", "asin", "acos", "atan",
                    "sinh", "cosh", "tanh", "floor", "ceil", "pow", "atan2", "hypot", "float", "float32", NULL
                };
                const char *name = n->text + 1;
                int known = 0;
                for (size_t k = 0; math[k]; k++) known |= strcmp(name, math[k]) == 0;
                /* abs, min and max of integers go through helpers: only their float form */
                if ((strcmp(name, "abs") == 0 || strcmp(name, "min") == 0 || strcmp(name, "max") == 0) &&
                    kernel_elem(n->type_name)) known = 1;
                if (!known) return 0;
            } else if (!fn_is_pure(n->text)) {
                return 0;
            }
            for (size_t k = 0; k < n->child_count; k++) {
                if (!pure_expr(n->children[k], c)) return 0;
            }
            return 1;
        }
        default:
            return 0;
    }
}

static int pure_stmt(const AstNode *n, PureCtx *c);

static int pure_block(const AstNode *block, PureCtx *c) {
    if (!block || block->kind != AST_BLOCK) return 0;
    size_t mark = c->count;
    for (size_t k = 0; k < block->child_count; k++) {
        if (!pure_stmt(block->children[k], c)) return 0;
    }
    c->count = mark;
    return 1;
}

static int pure_stmt(const AstNode *n, PureCtx *c) {
    switch (n->kind) {
        case AST_LET:
            if (!n->text || n->child_count != 1 || !is_scalar_type(n->type_name) ||
                !pure_expr(n->children[0], c) || c->count >= PURE_MAX) return 0;
            c->names[c->count++] = n->text;
            return 1;
        case AST_ASSIGN:            /* only variables declared there (no total = total + s) */
            return n->text && n->child_count == 1 && pure_declared(c, n->text) && pure_expr(n->children[0], c);
        case AST_IF:
            if ((n->child_count != 2 && n->child_count != 3) || !pure_expr(n->children[0], c)) return 0;
            for (size_t k = 1; k < n->child_count; k++) {
                if (!pure_block(n->children[k], c)) return 0;
            }
            return 1;
        case AST_RETURN:
            return c->function && n->child_count == 1 && pure_expr(n->children[0], c);
        case AST_INDEX_ASSIGN: {   /* y[i] = value, y a list of numbers */
            if (!c->layer || !n->type_name || n->child_count < 2 || c->write_count >= PURE_MAX) return 0;
            if (strcmp(n->type_name, "list[float]") != 0 && strcmp(n->type_name, "list[float32]") != 0 &&
                strcmp(n->type_name, "list[int]") != 0 && strcmp(n->type_name, "list[bool]") != 0) return 0;
            if (!is_ident(n->children[0], c->layer->i) || !pure_expr(n->children[1], c)) return 0;
            const char *name = NULL, *expr = NULL;
            if (n->text) {
                if (n->child_count != 2 || pure_declared(c, n->text)) return 0;
                name = expr = n->text;
            } else if (n->child_count != 3 ||
                       !fast_base_of(&c->layer->bases, n->children[2], c->layer->i, c->layer->body, &name, &expr)) {
                return 0;
            }
            size_t t = 0;
            while (t < c->target_count && strcmp(c->target_exprs[t], expr) != 0) t++;
            if (t == c->target_count) {
                if (t >= PURE_MAX) return 0;
                c->target_names[t] = name;
                c->target_exprs[t] = expr;
                c->target_types[t] = n->type_name;
                c->target_count++;
            }
            c->writes[c->write_count] = n;
            c->write_target[c->write_count++] = t;
            return 1;
        }
        default:
            return 0;
    }
}

/* Functions such as sigmoid or relu: numbers in, a number out, nothing else. */
static void compute_pure_functions(const AstNode *program) {
    pure_fn_count = 0;
    int changed = 1;
    while (changed) {                   /* a function calling itself is never added */
        changed = 0;
        for (size_t i = 0; i < program->child_count && pure_fn_count < SAFE_FN_MAX; i++) {
            const AstNode *fn = program->children[i];
            if (fn->kind != AST_FUNCTION || !fn->text || strcmp(fn->text, "main") == 0 ||
                fn_is_pure(fn->text) || !is_scalar_type(fn->type_name)) continue;
            PureCtx c;
            memset(&c, 0, sizeof(c));
            c.function = 1;
            size_t params = function_param_count(fn);
            int ok = params <= PURE_MAX;
            for (size_t k = 0; k < params && ok; k++) {
                const AstNode *param = fn->children[k];
                ok = param->text && is_scalar_type(param->type_name);
                if (ok) c.names[c.count++] = param->text;
            }
            if (ok && pure_block(function_body(fn), &c)) {
                pure_fn[pure_fn_count++] = fn->text;
                changed = 1;
            }
        }
    }
}

/* The statements after the sum can all run inside the job of each core. */
static int layer_fusable(MatvecPattern *mv, PureCtx *c) {
    memset(c, 0, sizeof(*c));
    c->layer = mv;
    if (mv->body->child_count <= 2) return 0;
    for (size_t st = 2; st < mv->body->child_count; st++) {
        if (!pure_stmt(mv->body->children[st], c)) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Expression precedence, so the C reads like hand-written code       */
/* (parentheses only where C needs them).                             */
/* ------------------------------------------------------------------ */

enum {
    PREC_OR = 4, PREC_AND = 5, PREC_BITOR = 6, PREC_BITXOR = 7, PREC_BITAND = 8,
    PREC_EQUALITY = 9, PREC_RELATIONAL = 10, PREC_SHIFT = 11,
    PREC_ADDITIVE = 12, PREC_MULTIPLICATIVE = 13, PREC_UNARY = 14, PREC_POSTFIX = 16
};

/* ------------------------------------------------------------------ */
/* Machine integers (u8 .. u64, i8 .. i32, and int for shifts).       */
/* Their arithmetic wraps around like in C: u8 and i16 values are     */
/* computed in C's int, then cut back with a cast, (uint8_t)(a + b).   */
/* Divisions and shifts by an unknown amount go through small checked */
/* functions (division by zero, negative shift).                      */
/* ------------------------------------------------------------------ */

/* value of a number written in the program, whatever its machine type */
static int literal_int(const AstNode *n, int64_t *out) {
    if (!n || n->kind != AST_INTEGER || !n->text || !machine_type_bits(n->type_name)) return 0;
    char *end = NULL;
    if (n->text[0] == '-') *out = (int64_t)strtoll(n->text, &end, 10);
    else *out = (int64_t)strtoull(n->text, &end, 10);
    return end && *end == '\0';
}

/* unsigned C type of the same width, for left shifts of signed numbers */
static const char *machine_unsigned_c(const char *t) {
    switch (machine_type_bits(t)) {
        case 8: return "uint8_t";
        case 16: return "uint16_t";
        case 32: return "uint32_t";
        default: return "uint64_t";
    }
}

static int machine_form(const AstNode *n) {
    if (!n || n->kind != AST_BINARY || !n->text || n->child_count != 2) return MFORM_NONE;
    const char *op = n->text, *t = n->type_name;
    int bits = machine_type_bits(t);
    if (!bits) return MFORM_NONE;
    int small = small_machine_bits(t) != 0, narrow = small && bits < 32;
    int64_t k;
    if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) {
        int known = const_int_value(n->children[1], &k) || literal_int(n->children[1], &k);
        if (!known || k < 0 || k >= bits) return MFORM_CALL;
        if (strcmp(op, ">>") == 0) return MFORM_PLAIN;
        /* u8 << 3 is cut back to 8 bits; a signed number is shifted as unsigned (no overflow in C) */
        return narrow || !machine_is_unsigned(t) ? MFORM_CAST : MFORM_PLAIN;
    }
    if (!small) return MFORM_NONE;                 /* int: the usual code */
    if (strcmp(op, "&") == 0 || strcmp(op, "|") == 0 || strcmp(op, "^") == 0) return MFORM_PLAIN;
    if (strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        /* by a number written other than 0 (and -1 for signed): cannot fail nor grow */
        if (!literal_int(n->children[1], &k) || k == 0 || (k == -1 && !machine_is_unsigned(t))) return MFORM_CALL;
        return MFORM_PLAIN;
    }
    if (strcmp(op, "+") != 0 && strcmp(op, "-") != 0 && strcmp(op, "*") != 0) return MFORM_NONE;
    return narrow ? MFORM_CAST : MFORM_PLAIN;
}

/* f32 + i32 is computed as a float (64 bits): the float32 side is converted first.  1 left, 2 right. */
static int float32_meets_wide_int(const AstNode *n) {
    if (!n || n->kind != AST_BINARY || n->child_count != 2) return 0;
    const char *lt = n->children[0]->type_name, *rt = n->children[1]->type_name;
    if (!lt || !rt) return 0;
    int lf = strcmp(lt, "float32") == 0, rf = strcmp(rt, "float32") == 0;
    if (lf && small_machine_bits(rt) >= 32) return 1;
    if (rf && small_machine_bits(lt) >= 32) return 2;
    return 0;
}

/*
 * u16 * (1 << 20) is an int (64 bits) in Clair, but both sides are narrower
 * than 64 bits in C (a number that fits in 32 bits is a C int): the machine
 * integer is converted first, (int64_t)x * 1048576.
 */
static int int_meet_needs_cast(const AstNode *n, size_t side) {
    int64_t k;
    if (!n || n->kind != AST_BINARY || n->child_count != 2) return 0;
    const AstNode *child = n->children[side], *other = n->children[1 - side];
    return small_machine_bits(child->type_name) && is_int_type(other->type_name) && const_int_value(other, &k);
}

static int emit_operand(FILE *f, const AstNode *child, int parent_prec, int is_right);

/* one side of a binary operator, after (int64_t) or (double) when C would compute in a narrower type */
static int emit_side(FILE *f, const AstNode *n, size_t side, int prec) {
    int widen = float32_meets_wide_int(n);
    if (int_meet_needs_cast(n, side)) fputs("(int64_t)", f);
    else if (widen == (int)side + 1) fputs("(double)", f);
    else return emit_operand(f, n->children[side], prec, side == 1);
    return emit_operand(f, n->children[side], PREC_UNARY, 0);
}

static const char *machine_c_type(const char *t) { return strcmp(t, "int") == 0 ? "int64_t" : c_type_name(t); }

static int machine_op_prec(const char *op) {
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return PREC_ADDITIVE;
    if (strcmp(op, "&") == 0) return PREC_BITAND;
    if (strcmp(op, "|") == 0) return PREC_BITOR;
    if (strcmp(op, "^") == 0) return PREC_BITXOR;
    if (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0) return PREC_SHIFT;
    return PREC_MULTIPLICATIVE;
}

/* ~x and -x on u8, i16...: cut back like the other operations */
static int machine_unary_cast(const AstNode *n) {
    if (!n || n->kind != AST_UNARY || !n->text) return 0;
    int bits = small_machine_bits(n->type_name);
    if (!bits || bits >= 32) return 0;
    return strcmp(n->text, "-") == 0 || (strcmp(n->text, "~") == 0 && machine_is_unsigned(n->type_name));
}

/*
 * Inside (uint8_t)(a * 31 + b), the cut of a * 31 to 8 bits changes nothing:
 * + - * and ~ keep the low bits (C wraps around with -fwrapv).  That inner
 * operation is written without its cast: (uint8_t)(a * 31 + b).
 */
static const AstNode *cast_skip = NULL;

static int is_wrap_op(const char *op) {
    return strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0;
}

static int cast_is_redundant(const AstNode *child, const char *t) {
    if (!child->type_name || strcmp(child->type_name, t) != 0) return 0;
    if (child->kind == AST_BINARY) return machine_form(child) == MFORM_CAST && is_wrap_op(child->text);
    return machine_unary_cast(child);
}

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
            if (n->text && n->text[0] == '@' && small_machine_bits(n->text + 1)) return PREC_UNARY;  /* (uint8_t)x */
            return PREC_POSTFIX;
        case AST_INTEGER:
            return n->text && n->text[0] == '-' ? PREC_UNARY : PREC_POSTFIX;   /* -5 next to an i8 */
        case AST_UNARY:
            if (const_int_value(n, &k) && k >= 0) return PREC_POSTFIX;
            if (big_bits(n->type_name)) return PREC_POSTFIX;               /* clair_i256_neg(x) */
            return PREC_UNARY;
        case AST_BINARY: {
            if (const_int_value(n, &k)) return k >= 0 ? PREC_POSTFIX : PREC_UNARY;
            const char *op = n->text;
            if (n == cast_skip) return machine_op_prec(op);
            switch (machine_form(n)) {
                case MFORM_CAST: return PREC_UNARY;                     /* (uint8_t)(a + b) */
                case MFORM_CALL: return PREC_POSTFIX;                   /* clair_rt_div_u8(a, b) */
                case MFORM_PLAIN: return machine_op_prec(op);
                default: break;
            }
            if (strcmp(op, "&") == 0) return PREC_BITAND;
            if (strcmp(op, "|") == 0) return PREC_BITOR;
            if (strcmp(op, "^") == 0) return PREC_BITXOR;
            const char *lt = n->children[0]->type_name, *rt = n->children[1]->type_name;
            if (big_bits(n->type_name)) return PREC_POSTFIX;               /* clair_i256_add(a, b) */
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
         * Safe read:
         *
         *     for v in values { ... }
         */
        if (parent &&
            parent->kind == AST_FOR_EACH &&
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

        case AST_FOR_EACH: {
            if (!n->text || n->child_count != 2) {
                return 0;
            }

            /* The list is evaluated before the loop variable enters scope. */
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
                strcmp(n->text, name) == 0 ? 0 : visible
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

/* ------------------------------------------------------------------ */
/* Ownership: freeing lists and matrices during the program.          */
/*                                                                    */
/* A variable declared with a new list or matrix ([...], matrix(r, c) */
/* or a call to a function that returns a new one) owns it when no    */
/* other reference can survive: it is only indexed, measured, appended */
/* to, iterated, or passed to parameters that do not keep it.  The    */
/* list is then freed when the variable goes away (end of its block,  */
/* break, continue, return), and `v = new list` frees the old one.    */
/* Everything else is kept until the end of the program, as before.   */
/* ------------------------------------------------------------------ */

static size_t function_param_count(const AstNode *fn);
static const AstNode *function_body(const AstNode *fn);

#define OWN_MAX 4096
static const AstNode *own_decls[OWN_MAX];
static size_t own_decl_count = 0;
static const AstNode *own_assigns[OWN_MAX];
static size_t own_assign_count = 0;
static const AstNode *own_pending[OWN_MAX];     /* assignments found while checking one binding */
static size_t own_pending_count = 0;

/* parameters (list or matrix) a function may keep: bit k set for parameter k */
static const char *escape_fn[SAFE_FN_MAX];
static uint64_t escape_bits[SAFE_FN_MAX];
static size_t escape_count = 0;
static const char *fresh_fn[SAFE_FN_MAX];
static size_t fresh_count = 0;

static int is_owned_type(const char *t) { return is_list_type(t) || is_matrix_type(t); }

static const AstNode *find_program_function(const char *name) {
    if (!codegen_program || !name) return NULL;
    for (size_t i = 0; i < codegen_program->child_count; i++) {
        const AstNode *fn = codegen_program->children[i];
        if (fn->kind == AST_FUNCTION && fn->text && strcmp(fn->text, name) == 0) return fn;
    }
    return NULL;
}

static int param_escapes(const char *fn_name, size_t k) {
    if (k >= 64) return 1;
    for (size_t i = 0; i < escape_count; i++) {
        if (strcmp(escape_fn[i], fn_name) == 0) return (int)((escape_bits[i] >> k) & 1);
    }
    return 1;   /* unknown function: assume it keeps everything */
}

static int is_fresh_fn(const char *name) {
    for (size_t i = 0; i < fresh_count; i++) if (strcmp(fresh_fn[i], name) == 0) return 1;
    return 0;
}

/* an expression that always makes a new list or matrix nobody else knows */
static int fresh_expr(const AstNode *e) {
    if (!e) return 0;
    if (e->kind == AST_LIST && is_list_type(e->type_name)) return 1;
    if (e->kind == AST_CALL && e->text && strcmp(e->text, "@matrix") == 0) return 1;
    if (e->kind == AST_CALL && e->text && strncmp(e->text, "@slice", 6) == 0 && is_list_type(e->type_name)) return 1;
    if (e->kind == AST_CALL && e->text && e->text[0] != '@' && is_fresh_fn(e->text)) return 1;
    return 0;
}

typedef struct { int allow_return; } OwnCtx;

static int own_block_ok(const AstNode *block, const AstNode *decl, const char *name, int visible, const OwnCtx *ctx);

static int own_expr_ok(const AstNode *n, const char *name, int visible, const AstNode *parent, size_t idx,
                       const OwnCtx *ctx) {
    if (!n || !visible) return 1;
    if (n->kind == AST_IDENTIFIER && n->text && strcmp(n->text, name) == 0) {
        if (!parent) return 0;
        if (parent->kind == AST_INDEX && idx == 0) return 1;                     /* v[i] */
        if (parent->kind == AST_FOR_EACH && idx == 0) return 1;                  /* for x in v */
        if (parent->kind == AST_RETURN && ctx->allow_return) return 1;           /* return v */
        if (parent->kind == AST_CALL && parent->text) {
            const char *callee = parent->text;
            if (strcmp(callee, "len") == 0 || strcmp(callee, "@rows") == 0 || strcmp(callee, "@cols") == 0) return 1;
            if (strncmp(callee, "@slice", 6) == 0 && idx == 0) return 1;          /* v[a:b] copies */
            if (callee[0] != '@' && find_program_function(callee) && !param_escapes(callee, idx)) return 1;
        }
        return 0;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (!own_expr_ok(n->children[i], name, visible, n, i, ctx)) return 0;
    }
    return 1;
}

static int own_stmt_ok(const AstNode *n, const AstNode *decl, const char *name, int visible, const OwnCtx *ctx) {
    if (!n) return 1;
    switch (n->kind) {
        case AST_IF:
            if (!own_expr_ok(n->children[0], name, visible, n, 0, ctx)) return 0;
            for (size_t i = 1; i < n->child_count; i++) {
                if (!own_block_ok(n->children[i], decl, name, visible, ctx)) return 0;
            }
            return 1;
        case AST_WHILE:
            if (n->child_count != 2) return 0;
            if (!own_expr_ok(n->children[0], name, visible, n, 0, ctx)) return 0;
            return own_block_ok(n->children[1], decl, name, visible, ctx);
        case AST_FOR: {
            if (!n->text || n->child_count < 2) return 0;
            for (size_t i = 0; i + 1 < n->child_count; i++) {
                if (!own_expr_ok(n->children[i], name, visible, n, i, ctx)) return 0;
            }
            return own_block_ok(n->children[n->child_count - 1], decl, name,
                                strcmp(n->text, name) == 0 ? 0 : visible, ctx);
        }
        case AST_FOR_EACH: {
            if (!n->text || n->child_count != 2) return 0;
            if (!own_expr_ok(n->children[0], name, visible, n, 0, ctx)) return 0;
            /* the loop keeps reading the list: it must not be replaced (and freed) meanwhile */
            if (visible && is_ident(n->children[0], name) && name_changes_in(n->children[1], name)) return 0;
            return own_block_ok(n->children[1], decl, name, strcmp(n->text, name) == 0 ? 0 : visible, ctx);
        }
        case AST_ASSIGN:
            if (visible && n->text && strcmp(n->text, name) == 0) {
                /* v = new list: the old one is freed first; anything else could share it */
                if (ctx->allow_return || !fresh_expr(n->children[0]) || own_pending_count >= OWN_MAX) return 0;
                if (!own_expr_ok(n->children[0], name, visible, n, 0, ctx)) return 0;
                own_pending[own_pending_count++] = n;
                return 1;
            }
            break;
        case AST_LET:
            return 0;    /* handled by the block walker */
        default:
            break;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        if (!own_expr_ok(n->children[i], name, visible, n, i, ctx)) return 0;
    }
    return 1;
}

static int own_block_ok(const AstNode *block, const AstNode *decl, const char *name, int visible, const OwnCtx *ctx) {
    if (!block || block->kind != AST_BLOCK) return 0;
    for (size_t i = 0; i < block->child_count; i++) {
        const AstNode *st = block->children[i];
        if (st->kind == AST_LET) {
            if (!st->text || st->child_count != 1) return 0;
            if (!own_expr_ok(st->children[0], name, visible, st, 0, ctx)) return 0;   /* also rejects aliases */
            if (st == decl) visible = 1;
            else if (strcmp(st->text, name) == 0) visible = 0;
            continue;
        }
        if (!own_stmt_ok(st, decl, name, visible, ctx)) return 0;
    }
    return 1;
}

static int own_binding_ok(const AstNode *decl, int allow_return) {
    const AstNode *fn = stack_list_find_function(decl);
    const AstNode *body = fn ? function_body(fn) : NULL;
    OwnCtx ctx = { allow_return };
    own_pending_count = 0;
    return body && own_block_ok(body, decl, decl->text, 0, &ctx);
}

static void collect_lets(const AstNode *n, const AstNode **out, size_t *count, size_t max) {
    if (!n) return;
    if (n->kind == AST_LET && *count < max) out[(*count)++] = n;
    for (size_t i = 0; i < n->child_count; i++) collect_lets(n->children[i], out, count, max);
}

static int count_declarations(const AstNode *n, const char *name) {
    if (!n) return 0;
    int c = ((n->kind == AST_LET || n->kind == AST_PARAM || n->kind == AST_FOR || n->kind == AST_FOR_EACH) &&
             n->text && strcmp(n->text, name) == 0) ? 1 : 0;
    for (size_t i = 0; i < n->child_count; i++) c += count_declarations(n->children[i], name);
    return c;
}

/* every `return` returns the same local, declared once with a new list nobody else keeps */
static int returns_fresh(const AstNode *fn) {
    if (!is_owned_type(fn->type_name)) return 0;
    const AstNode *body = function_body(fn);
    const AstNode *lets[256];
    size_t let_count = 0;
    collect_lets(body, lets, &let_count, 256);
    const AstNode *stack[512];
    size_t top = 0;
    const char *returned = NULL;
    if (body) stack[top++] = body;
    while (top > 0) {
        const AstNode *n = stack[--top];
        if (n->kind == AST_RETURN) {
            if (n->child_count != 1 || !is_ident(n->children[0], NULL)) return 0;
            if (returned && strcmp(returned, n->children[0]->text) != 0) return 0;
            returned = n->children[0]->text;
        }
        for (size_t i = 0; i < n->child_count; i++) {
            if (top >= 512) return 0;
            stack[top++] = n->children[i];
        }
    }
    if (!returned || count_declarations(fn, returned) != 1) return 0;
    for (size_t i = 0; i < let_count; i++) {
        const AstNode *let = lets[i];
        if (strcmp(let->text, returned) != 0) continue;
        if (!fresh_expr(let->children[0]) || stack_list_decl_is_safe(let)) return 0;
        return own_binding_ok(let, 1);
    }
    return 0;
}

static int own_param_escapes(const AstNode *fn, size_t k) {
    const AstNode *param = fn->children[k];
    if (!param->text || !is_owned_type(param->type_name)) return 0;
    OwnCtx ctx = { 0 };
    own_pending_count = 0;
    /* a parameter must not even be replaced: the caller still frees its own list */
    if (name_changes_in(function_body(fn), param->text)) return 1;
    return !own_block_ok(function_body(fn), NULL, param->text, 1, &ctx);
}

static int is_owned_decl(const AstNode *n) {
    for (size_t i = 0; i < own_decl_count; i++) if (own_decls[i] == n) return 1;
    return 0;
}

static int is_owned_assign(const AstNode *n) {
    for (size_t i = 0; i < own_assign_count; i++) if (own_assigns[i] == n) return 1;
    return 0;
}

static void compute_ownership(const AstNode *program) {
    own_decl_count = own_assign_count = escape_count = fresh_count = 0;

    /* 1. which list / matrix parameters may be kept by their function (least fixpoint) */
    for (size_t i = 0; i < program->child_count && escape_count < SAFE_FN_MAX; i++) {
        const AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION || !fn->text) continue;
        escape_fn[escape_count] = fn->text;
        escape_bits[escape_count++] = 0;
    }
    for (int changed = 1; changed;) {
        changed = 0;
        for (size_t i = 0; i < escape_count; i++) {
            const AstNode *fn = find_program_function(escape_fn[i]);
            size_t params = function_param_count(fn);
            for (size_t k = 0; k < params && k < 64; k++) {
                if ((escape_bits[i] >> k) & 1) continue;
                if (own_param_escapes(fn, k)) {
                    escape_bits[i] |= UINT64_C(1) << k;
                    changed = 1;
                }
            }
        }
    }

    /* 2. functions that return a new list or matrix (grows until stable) */
    for (int changed = 1; changed;) {
        changed = 0;
        for (size_t i = 0; i < program->child_count && fresh_count < SAFE_FN_MAX; i++) {
            const AstNode *fn = program->children[i];
            if (fn->kind != AST_FUNCTION || !fn->text || is_fresh_fn(fn->text)) continue;
            if (returns_fresh(fn)) {
                fresh_fn[fresh_count++] = fn->text;
                changed = 1;
            }
        }
    }

    /* 3. variables that own their list or matrix */
    const AstNode *lets[OWN_MAX];
    size_t let_count = 0;
    collect_lets(program, lets, &let_count, OWN_MAX);
    for (size_t i = 0; i < let_count; i++) {
        const AstNode *let = lets[i];
        if (!let->text || !is_owned_type(let->type_name) || let->child_count != 1) continue;
        if (!fresh_expr(let->children[0]) || stack_list_decl_is_safe(let)) continue;
        if (!own_binding_ok(let, 0)) continue;
        if (own_decl_count >= OWN_MAX || own_assign_count + own_pending_count > OWN_MAX) continue;
        own_decls[own_decl_count++] = let;
        for (size_t k = 0; k < own_pending_count; k++) own_assigns[own_assign_count++] = own_pending[k];
    }
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

/*
 * A big integer literal: clair_i256_from_i64(-5) when it fits in 64 bits,
 * otherwise its 64-bit words, least significant first.
 */
static int emit_big_literal(FILE *f, const AstNode *n) {
    int bits = big_bits(n->type_name), words = bits / 64;
    char tag = big_tag(n->type_name);
    uint64_t w[16];
    if (!clair_decimal_to_words(n->text, w, words, tag == 'u')) return 0;
    uint64_t ext = (w[0] >> 63) ? UINT64_MAX : 0;
    int small = tag == 'i' || ext == 0;
    for (int i = 1; i < words; i++) if (w[i] != ext) small = 0;
    if (small) {
        fprintf(f, "clair_%c%d_from_i64(", tag, bits);
        emit_int_literal(f, (int64_t)w[0]);
        fputc(')', f);
        return 1;
    }
    fprintf(f, "(%s){{", c_type_name(n->type_name));
    for (int i = 0; i < words; i++) fprintf(f, "%s0x%llxu", i ? ", " : "", (unsigned long long)w[i]);
    fprintf(f, "}} /* %s */", n->text);
    return 1;
}

/* @convert(x): int <-> big integers, big -> float; narrowing is checked at run time */
static int emit_convert(FILE *f, const AstNode *n) {
    if (n->child_count != 1) return 0;
    const AstNode *x = n->children[0];
    int to = big_bits(n->type_name), from = big_bits(x->type_name);
    char tt = big_tag(n->type_name), ft = big_tag(x->type_name);
    if (to && to == from && tt == ft) return emit_expr(f, x);     /* int256(x) with x already int256 */
    if (to && !from) fprintf(f, "clair_%c%d_from_i64(", tt, to);
    else if (to && from) fprintf(f, "clair_%c%d_from_%c%d(", tt, to, ft, from);
    else if (from && is_int_type(n->type_name)) fprintf(f, "clair_%c%d_to_i64(", ft, from);
    else if (from && n->type_name && strcmp(n->type_name, "float") == 0) fprintf(f, "clair_%c%d_to_double(", ft, from);
    else return 0;
    if (!emit_expr(f, x)) return 0;
    fputc(')', f);
    return 1;
}

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
               (is_right && parent_prec == PREC_ADDITIVE && is_negation(child)) ||  /* a - (-b) */
               /* a & (b | c), a << (b + c): written out, as C compilers ask */
               ((parent_prec == PREC_BITOR || parent_prec == PREC_BITXOR || parent_prec == PREC_BITAND ||
                 parent_prec == PREC_SHIFT) &&
                (prec == PREC_BITOR || prec == PREC_BITXOR || prec == PREC_BITAND || prec == PREC_SHIFT ||
                 prec == PREC_ADDITIVE) && prec != parent_prec);
    if (wrap) fputc('(', f);
    if (!emit_expr(f, child)) return 0;
    if (wrap) fputc(')', f);
    return 1;
}

static const char *list_accessor_name(const char *list_type) {
    static char name[64];
    int k = element_kind_of(list_type, "list[");
    if (k < 0) return NULL;
    snprintf(name, sizeof(name), "clair_rt_list_at_%s_checked", element_kinds[k][2]);
    return name;
}

/* name of the checked helper of a machine-integer operator: div, mod, shl, shr */
static const char *machine_helper(const char *op) {
    if (strcmp(op, "/") == 0) return "div";
    if (strcmp(op, "%") == 0) return "mod";
    if (strcmp(op, "<<") == 0) return "shl";
    return "shr";
}

/* a shift count: a u64 count is brought into int64_t (64 and more all give the same result) */
static int emit_shift_count(FILE *f, const AstNode *count) {
    int big = count->type_name && strcmp(count->type_name, "u64") == 0;
    if (big) fputs("clair_rt_count_u64(", f);
    if (!emit_expr(f, count)) return 0;
    if (big) fputc(')', f);
    return 1;
}

/*
 * u8 + u8 -> (uint8_t)(a + b), i32 << 3 -> (int32_t)((uint32_t)a << 3),
 * u16 / v -> clair_rt_div_u16(a, v), a & b -> a & b.
 */
static int emit_machine_binary(FILE *f, const AstNode *n, int form) {
    const char *op = n->text, *t = n->type_name;
    if (form == MFORM_CALL) {
        int shift = strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0;
        fprintf(f, "clair_rt_%s_%s(", machine_helper(op), t);
        if (!emit_expr(f, n->children[0])) return 0;
        fputs(", ", f);
        if (!(shift ? emit_shift_count(f, n->children[1]) : emit_expr(f, n->children[1]))) return 0;
        fputc(')', f);
        return 1;
    }
    int prec = machine_op_prec(op);
    int skipped = n == cast_skip;
    if (skipped) form = MFORM_PLAIN;
    if ((form == MFORM_CAST || skipped) && is_wrap_op(op)) {
        /* cut back by this cast or an enclosing one: the operands need not be cut */
        if (form == MFORM_CAST) fprintf(f, "(%s)(", machine_c_type(t));
        for (size_t side = 0; side < 2; side++) {
            const AstNode *child = n->children[side];
            const AstNode *saved = cast_skip;
            if (side) fprintf(f, " %s ", op);
            if (cast_is_redundant(child, t)) cast_skip = child;
            int ok = emit_operand(f, child, prec, (int)side);
            cast_skip = saved;
            if (!ok) return 0;
        }
        if (form == MFORM_CAST) fputc(')', f);
        return 1;
    }
    if (form == MFORM_CAST && strcmp(op, "<<") == 0 && !machine_is_unsigned(t)) {
        fprintf(f, "(%s)((%s)", machine_c_type(t), machine_unsigned_c(t));
        if (!emit_operand(f, n->children[0], PREC_UNARY, 0)) return 0;
        fputs(" << ", f);
        if (!emit_operand(f, n->children[1], PREC_SHIFT, 1)) return 0;
        fputc(')', f);
        return 1;
    }
    if (form == MFORM_CAST) fprintf(f, "(%s)(", machine_c_type(t));
    if (!emit_operand(f, n->children[0], prec, 0)) return 0;
    fprintf(f, " %s ", op);
    if (!emit_operand(f, n->children[1], prec, 1)) return 0;
    if (form == MFORM_CAST) fputc(')', f);
    return 1;
}

static int emit_expr(FILE *f, const AstNode *n) {
    switch (n->kind) {
        case AST_INTEGER:
            if (big_bits(n->type_name)) return emit_big_literal(f, n);
            if (small_machine_bits(n->type_name)) {
                /* 4000000000u: an unsigned C literal, so that u32 arithmetic stays in 32 bits */
                int64_t v;
                int unsigned_type = machine_is_unsigned(n->type_name);
                if (strcmp(n->text, "-2147483648") == 0) { fputs("INT32_MIN", f); return 1; }
                if (unsigned_type && (!literal_int(n, &v) || v < 0 || v > INT32_MAX)) { fprintf(f, "%su", n->text); return 1; }
            }
            fputs(n->text, f);
            return 1;

        case AST_FLOAT:
            if (n->type_name && strcmp(n->type_name, "float32") == 0) {
                /*
                 * 0.1f, unless reading the number straight into a float gives
                 * another float than going through a double (as Python and
                 * NumPy do): then (float)0.1.
                 */
                if (strtof(n->text, NULL) == (float)strtod(n->text, NULL)) fprintf(f, "%sf", n->text);
                else fprintf(f, "(float)%s", n->text);
                return 1;
            }
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
                    fputs(" * ", f);
                    emit_cols(f, fast_base);
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
            if (n->text && strcmp(n->text, "@convert") == 0) return emit_convert(f, n);
            if (n->text && strncmp(n->text, "@slice", 6) == 0) {
                /* xs[a:b:c]: clair_rt_list_slice(xs, a, b, c, missing bounds) like Python */
                const char *written = n->text + 6;
                size_t child = 1;
                fputs(strcmp(n->type_name, "string") == 0 ? "clair_rt_string_slice(" : "clair_rt_list_slice(", f);
                if (!emit_expr(f, n->children[0])) return 0;
                for (int k = 0; k < 3; k++) {
                    fputs(", ", f);
                    if (written[k] == '1') { if (!emit_expr(f, n->children[child++])) return 0; }
                    else fputs(k == 2 ? "1" : "0", f);
                }
                if (written[0] == '1' && written[1] == '1') fputs(", 0)", f);
                else if (written[0] == '1') fputs(", CLAIR_NO_STOP)", f);
                else if (written[1] == '1') fputs(", CLAIR_NO_START)", f);
                else fputs(", CLAIR_NO_START | CLAIR_NO_STOP)", f);
                return 1;
            }
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
                if (strcmp(name, "float") == 0 || strcmp(name, "float32") == 0) {
                    fputs(name[5] ? "(float)" : "(double)", f);
                    return emit_operand(f, n->children[0], PREC_UNARY, 0);
                }
                if (strcmp(name, "int") == 0) {
                    if (t0 && (strcmp(t0, "float") == 0 || strcmp(t0, "float32") == 0)) {
                        fputs("clair_rt_float_to_int(", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputc(')', f);
                        return 1;
                    }
                    if (t0 && strcmp(t0, "u64") == 0) {          /* checked: 2^63 and more do not fit */
                        fputs("clair_rt_u64_to_int(", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputc(')', f);
                        return 1;
                    }
                    fputs("(int64_t)", f);
                    return emit_operand(f, n->children[0], PREC_UNARY, 0);
                }
                if (small_machine_bits(name)) {
                    /* u8(x): keeps the low bits, like C; a float is first cut toward zero, like int() */
                    fprintf(f, "(%s)", c_type_name(name));
                    if (t0 && (strcmp(t0, "float") == 0 || strcmp(t0, "float32") == 0)) {
                        fputs("clair_rt_float_to_int(", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputc(')', f);
                        return 1;
                    }
                    return emit_operand(f, n->children[0], PREC_UNARY, 0);
                }
                if (small_machine_bits(n->type_name) && (strcmp(name, "abs") == 0 || strcmp(name, "min") == 0 ||
                                                          strcmp(name, "max") == 0)) {
                    /* abs of an unsigned number is the number itself: (a + b), not a + b */
                    if (strcmp(name, "abs") == 0 && machine_is_unsigned(n->type_name))
                        return emit_operand(f, n->children[0], PREC_POSTFIX, 0);
                    fprintf(f, "clair_rt_%s_%s(", name, n->type_name);
                    for (size_t i = 0; i < n->child_count; i++) {
                        if (i != 0) fputs(", ", f);
                        if (!emit_expr(f, n->children[i])) return 0;
                    }
                    fputc(')', f);
                    return 1;
                }
                if (big_bits(n->type_name) && (strcmp(name, "abs") == 0 || strcmp(name, "min") == 0 ||
                                               strcmp(name, "max") == 0 || strcmp(name, "pow") == 0)) {
                    /* exact, in the big type; abs of a uint is the value itself */
                    if (strcmp(name, "abs") == 0 && big_tag(n->type_name) == 'u') return emit_expr(f, n->children[0]);
                    fprintf(f, "clair_%c%d_%s(", big_tag(n->type_name), big_bits(n->type_name), name);
                    for (size_t i = 0; i < n->child_count; i++) {
                        if (i != 0) fputs(", ", f);
                        if (!emit_expr(f, n->children[i])) return 0;
                    }
                    fputc(')', f);
                    return 1;
                }
                const char *c_name = name;
                if (strcmp(name, "abs") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_abs_i64" : "fabs";
                if (strcmp(name, "min") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_min_i64" : "fmin";
                if (strcmp(name, "max") == 0) c_name = is_int_type(n->type_name) ? "clair_rt_max_i64" : "fmax";
                char f32_name[32];
                if (n->type_name && strcmp(n->type_name, "float32") == 0) {
                    snprintf(f32_name, sizeof(f32_name), "%sf", c_name);   /* sqrtf, expf, fabsf: computed in float32 */
                    c_name = f32_name;
                }
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

            int bits = big_bits(n->children[0]->type_name);
            char tag = big_tag(n->children[0]->type_name);
            if (bits) {
                /* big integers: checked helpers, comparisons through clair_iN_cmp */
                static const char *const ops[][2] = {
                    { "+", "add" }, { "-", "sub" }, { "*", "mul" }, { "/", "div" }, { "%", "mod" },
                };
                const char *fn = NULL;
                for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
                    if (strcmp(op, ops[i][0]) == 0) fn = ops[i][1];
                }
                fprintf(f, "clair_%c%d_%s(", tag, bits, fn ? fn : "cmp");
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputc(')', f);
                if (!fn) fprintf(f, " %s 0", op);
                return 1;
            }

            int64_t folded;
            if (const_int_value(n, &folded)) {
                emit_int_literal(f, folded);
                return 1;
            }

            int form = machine_form(n);
            if (form != MFORM_NONE) return emit_machine_binary(f, n, form);

            if (strcmp(op, "%") == 0) {
                if (!is_int_type(n->type_name)) {
                    fputs(strcmp(n->type_name, "float32") == 0 ? "fmodf(" : "fmod(", f);
                } else if (int_div_is_safe(n)) {
                    if (!emit_side(f, n, 0, PREC_MULTIPLICATIVE)) return 0;
                    fputs(" % ", f);
                    return emit_side(f, n, 1, PREC_MULTIPLICATIVE);
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
                machine_type_bits(n->children[0]->type_name) &&
                machine_type_bits(n->children[1]->type_name)) {
                if (int_div_is_safe(n)) {
                    /* constant divisor other than 0 and -1: cannot fail */
                    if (!emit_side(f, n, 0, PREC_MULTIPLICATIVE)) return 0;
                    fputs(" / ", f);
                    return emit_side(f, n, 1, PREC_MULTIPLICATIVE);
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

            if (!emit_side(f, n, 0, prec)) return 0;
            fprintf(f, " %s ", op);
            return emit_side(f, n, 1, prec);
        }

        case AST_UNARY: {
            if (n->child_count != 1 || !n->text) return 0;
            int64_t folded;
            if (const_int_value(n, &folded)) {
                emit_int_literal(f, folded);
                return 1;
            }
            if (big_bits(n->type_name)) {
                fprintf(f, "clair_i%d_neg(", big_bits(n->type_name));
                if (!emit_expr(f, n->children[0])) return 0;
                fputc(')', f);
                return 1;
            }
            const char *op = strcmp(n->text, "not") == 0 ? "!" : n->text;
            int cut = machine_unary_cast(n);
            if (cut && n != cast_skip) fprintf(f, "(%s)", machine_c_type(n->type_name));   /* (uint8_t)~x */
            fputs(op, f);
            const AstNode *operand = n->children[0];
            const AstNode *saved = cast_skip;
            if (cut && cast_is_redundant(operand, n->type_name)) cast_skip = operand;   /* (uint8_t)-(a + b) */
            /* `- -x` would read as a decrement; keep `-(-x)` */
            int wrap = expr_prec(operand) < PREC_UNARY || operand->kind == AST_UNARY ||
                       (operand->kind == AST_INTEGER && operand->text && operand->text[0] == '-');
            if (wrap) fputc('(', f);
            int ok = emit_expr(f, operand);
            cast_skip = saved;
            if (!ok) return 0;
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

/* Is the variable `name` read or assigned somewhere in `n`? (for v in xs { } with v unused) */
static int name_mentioned_in(const AstNode *n, const char *name) {
    if (!n) return 0;
    if ((n->kind == AST_IDENTIFIER || n->kind == AST_ASSIGN) && n->text && strcmp(n->text, name) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (name_mentioned_in(n->children[i], name)) return 1;
    }
    return 0;
}

/* A variable or a chain of fields (xs, self.items, net.layer.w): reading it twice gives the same list. */
static int is_access_path(const AstNode *e) {
    while (e && e->kind == AST_MEMBER && e->child_count == 1 && e->text) e = e->children[0];
    return e && e->kind == AST_IDENTIFIER && e->text;
}

/* Can evaluating `n` stop the program or run user code (calls, bounds, division)? */
static int node_may_fail(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_CALL && !(n->text && strcmp(n->text, "len") == 0) && !builtin_cannot_fail(n)) return 1;
    if (n->kind == AST_INDEX) return 1;
    if ((n->kind == AST_BINARY || n->kind == AST_UNARY) && big_bits(n->type_name)) return 1;  /* overflow */
    if (n->kind == AST_BINARY && n->text && (strcmp(n->text, "/") == 0 || strcmp(n->text, "%") == 0) &&
        is_int_type(n->type_name) && !int_div_is_safe(n)) return 1;
    if (n->kind == AST_BINARY && machine_form(n) == MFORM_CALL) return 1;   /* u8 / 0, x << -1 */
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_may_fail(n->children[i])) return 1;
    }
    return 0;
}

/* Lists and matrices owned by the variables of the blocks being written (see compute_ownership). */
typedef struct { const char *name; int matrix; } OwnedVar;   /* matrix: 0 list, 1 matrix, 2 sums of a layer (3: float32) */
static OwnedVar owned_live[OWN_MAX];
static size_t owned_live_count = 0;
static size_t loop_marks[1024];          /* owned_live_count when each enclosing loop started */
static size_t loop_mark_count = 0;
static const char *current_return_type = NULL;

static void emit_free_since(FILE *f, size_t mark, int depth) {
    for (size_t i = owned_live_count; i > mark; i--) {
        indent(f, depth);
        int kind = owned_live[i - 1].matrix;
        if (kind >= 2) fprintf(f, "clair_rt_sums_free%s(%s, %s_small);\n", kind == 3 ? "_f32" : "",
                               owned_live[i - 1].name, owned_live[i - 1].name);
        else fprintf(f, "%s(%s);\n", kind ? "clair_rt_matrix_free" : "clair_rt_list_free", owned_live[i - 1].name);
    }
}

/* break / continue / return leaving owned variables: free them first */
static int emit_jump(FILE *f, const char *jump, size_t mark, int depth) {
    if (owned_live_count <= mark) {
        fprintf(f, "%s;\n", jump);
        return 1;
    }
    fputs("{\n", f);
    emit_free_since(f, mark, depth + 1);
    indent(f, depth + 1);
    fprintf(f, "%s;\n", jump);
    indent(f, depth);
    fputs("}\n", f);
    return 1;
}

static int emit_block(FILE *f, const AstNode *block, int depth) {
    if (!block || block->kind != AST_BLOCK) return 0;
    size_t mark = owned_live_count;
    for (size_t i = 0; i < block->child_count; i++) {
        if (!emit_stmt(f, block->children[i], depth)) return 0;
    }
    const AstNode *last = block->child_count ? block->children[block->child_count - 1] : NULL;
    int jumps = last && (last->kind == AST_RETURN || last->kind == AST_BREAK || last->kind == AST_CONTINUE);
    if (!jumps) emit_free_since(f, mark, depth);   /* after a jump the frees were already written */
    owned_live_count = mark;
    return 1;
}

static int emit_stmt_inner(FILE *f, const AstNode *n, int depth);

/* loops remember which owned variables are outside them, for break and continue */
static int emit_stmt(FILE *f, const AstNode *n, int depth) {
    int loop = n->kind == AST_FOR || n->kind == AST_WHILE || n->kind == AST_FOR_EACH;
    if (loop) {
        if (loop_mark_count >= sizeof(loop_marks) / sizeof(loop_marks[0])) return 0;
        loop_marks[loop_mark_count++] = owned_live_count;
    }
    int ok = emit_stmt_inner(f, n, depth);
    if (loop) loop_mark_count--;
    return ok;
}

/* C names of the float32 kernels: the float ones keep their names, the float32 ones end in F32 / _f32 */
static int is_f32(const char *elem) { return elem && strcmp(elem, "float32") == 0; }
static const char *kernel_ctype(const char *elem) { return is_f32(elem) ? "float" : "double"; }

/* Functions written for the dense layers of the function being written (see PureCtx). */
static FILE *layer_helpers = NULL;
static const char *layer_fn_name = NULL;
static int layer_count = 0;

/* one statement of what follows a sum, inside clair_layer_<id> */
static int emit_pure_stmt(FILE *f, const AstNode *n, int depth, const PureCtx *c) {
    indent(f, depth);
    switch (n->kind) {
        case AST_LET:
            fprintf(f, "%s %s = ", c_type_name(n->type_name), n->text);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;
        case AST_ASSIGN:
            fprintf(f, "%s = ", n->text);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;
        case AST_IF:
            fputs("if (", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(") {\n", f);
            for (size_t k = 1; k < n->child_count; k++) {
                if (k == 2) fputs(" else {\n", f);
                const AstNode *block = n->children[k];
                for (size_t st = 0; st < block->child_count; st++) {
                    if (!emit_pure_stmt(f, block->children[st], depth + 1, c)) return 0;
                }
                indent(f, depth);
                fputc('}', f);
            }
            fputc('\n', f);
            return 1;
        case AST_INDEX_ASSIGN:
            for (size_t w = 0; w < c->write_count; w++) {
                if (c->writes[w] != n) continue;
                fprintf(f, "clair_data_%s[%s] = ", c->target_names[c->write_target[w]], c->layer->i);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs(";\n", f);
                return 1;
            }
            return 0;
        default:
            return 0;
    }
}

/*
 * The job of a dense layer for one core: the sums of rows [first, last), then
 * what follows each sum, row by row.  The lists written at [i] and the
 * variables read there travel in the job.
 */
static int emit_layer_helper(FILE *f, const char *id, const MatvecPattern *mv, const PureCtx *c) {
    fprintf(f, "/* dense layer of %s: the sums of rows [first, last), then the rest of each row */\n", layer_fn_name);
    fprintf(f, "typedef struct {\n    ClairMatvecJob%s sums;\n", is_f32(mv->elem) ? "F32" : "");
    for (size_t t = 0; t < c->target_count; t++) {
        fprintf(f, "    %s *clair_data_%s;\n", list_element_c_type(c->target_types[t]), c->target_names[t]);
    }
    for (size_t k = 0; k < c->capture_count; k++) {
        fprintf(f, "    %s %s;\n", c_type_name(c->captures[k]->type_name), c->captures[k]->text);
    }
    fprintf(f, "} ClairLayer_%s;\n\n", id);
    fprintf(f, "static void clair_layer_%s(const void *clair_job, int64_t clair_first, int64_t clair_last) {\n", id);
    fprintf(f, "    const ClairLayer_%s *clair_layer = clair_job;\n", id);
    fprintf(f, "    clair_rt_matvec_rows%s(&clair_layer->sums, clair_first, clair_last);\n", is_f32(mv->elem) ? "_f32" : "");
    for (size_t t = 0; t < c->target_count; t++) {
        fprintf(f, "    %s *clair_data_%s = clair_layer->clair_data_%s;\n", list_element_c_type(c->target_types[t]),
                c->target_names[t], c->target_names[t]);
    }
    for (size_t k = 0; k < c->capture_count; k++) {
        fprintf(f, "    %s %s = clair_layer->%s;\n", c_type_name(c->captures[k]->type_name), c->captures[k]->text,
                c->captures[k]->text);
    }
    fprintf(f, "    for (int64_t %s = clair_first; %s < clair_last; %s++) {\n", mv->i, mv->i, mv->i);
    int uses_s = 0;
    for (size_t st = 2; st < mv->body->child_count; st++) uses_s |= name_mentioned_in(mv->body->children[st], mv->s);
    if (uses_s) fprintf(f, "        %s %s = clair_layer->sums.s[%s];\n", kernel_ctype(mv->elem), mv->s, mv->i);
    for (size_t st = 2; st < mv->body->child_count; st++) {
        if (!emit_pure_stmt(f, mv->body->children[st], 2, c)) return 0;
    }
    fputs("    }\n}\n\n", f);
    return 1;
}

static int emit_stmt_inner(FILE *f, const AstNode *n, int depth) {
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
            if (is_owned_decl(n)) {
                if (owned_live_count >= OWN_MAX) return 0;
                owned_live[owned_live_count++] = (OwnedVar){ n->text, is_matrix_type(n->type_name) };
            }
            return 1;
        }

        case AST_ASSIGN:
            if (is_owned_assign(n)) {
                /* v = new list: the old one is freed (nothing else can still use it) */
                const char *type = c_type_name(n->children[0]->type_name);
                if (!type) return 0;
                fprintf(f, "{\n");
                indent(f, depth + 1);
                fprintf(f, "%s clair_new = ", type);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(";\n", f);
                indent(f, depth + 1);
                fprintf(f, "%s(%s);\n", is_matrix_type(n->children[0]->type_name) ? "clair_rt_matrix_free"
                                                                                     : "clair_rt_list_free", n->text);
                indent(f, depth + 1);
                fprintf(f, "%s = clair_new;\n", n->text);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }
            {
                /* x = x + v is written x += v, like C written by hand (same machine code) */
                const AstNode *v = n->children[0];
                const char *op = v->kind == AST_BINARY && v->child_count == 2 && v->text ? v->text : "";
                int plain = op[0] && !op[1] && strchr("+-*/%", op[0]) && is_ident(v->children[0], n->text) &&
                            v->type_name && (strcmp(v->type_name, "int") == 0 || strcmp(v->type_name, "float") == 0 ||
                                             strcmp(v->type_name, "float32") == 0) &&
                            !(op[0] == '%' && strcmp(v->type_name, "int") != 0) &&
                            !((op[0] == '/' || op[0] == '%') && strcmp(v->type_name, "int") == 0 && !int_div_is_safe(v)) &&
                            !float32_meets_wide_int(v) && !int_meet_needs_cast(v, 1);
                /* u8 and i32: C's x += v cuts the result back into x by itself */
                int form = machine_form(v);
                if (op[0] && is_ident(v->children[0], n->text) && small_machine_bits(v->type_name) &&
                    (form == MFORM_PLAIN || form == MFORM_CAST) && !(op[0] == '<' && !machine_is_unsigned(v->type_name)))
                    plain = 1;
                if (op[0] && is_ident(v->children[0], n->text) && strcmp(op, ">>") == 0 && form == MFORM_PLAIN) plain = 1;
                int64_t folded;
                if (plain && !const_int_value(v, &folded)) {
                    fprintf(f, "%s %s= ", n->text, op);
                    if (!emit_expr(f, v->children[1])) return 0;
                    fputs(";\n", f);
                    return 1;
                }
            }
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
                fputs(" * ", f);
                emit_cols(f, fast_base);
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
            if (owned_live_count > 0) {
                /* the value first (it may read an owned list), then the frees */
                const char *type = c_type_name(current_return_type);
                if (!type) return 0;
                fputs("{\n", f);
                indent(f, depth + 1);
                fprintf(f, "%s clair_result = ", type);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(";\n", f);
                emit_free_since(f, 0, depth + 1);
                indent(f, depth + 1);
                fputs("return clair_result;\n", f);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }
            fputs("return ", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_BREAK:
            return emit_jump(f, "break", loop_mark_count ? loop_marks[loop_mark_count - 1] : owned_live_count, depth);

        case AST_CONTINUE:
            return emit_jump(f, "continue", loop_mark_count ? loop_marks[loop_mark_count - 1] : owned_live_count, depth);

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
            } else if (strcmp(value->type_name, "float32") == 0) {
                fputs("clair_rt_print_f32(", f);
                if (!emit_expr(f, value)) return 0;
                fputs(");\n", f);
            } else if (strcmp(value->type_name, "bool") == 0) {
                fputs("printf(\"%d\\n\", (int)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (strcmp(value->type_name, "int") == 0 ||
                       (small_machine_bits(value->type_name) && !machine_is_unsigned(value->type_name))) {
                fputs("printf(\"%\" PRId64 \"\\n\", (int64_t)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (small_machine_bits(value->type_name)) {
                fputs("printf(\"%\" PRIu64 \"\\n\", (uint64_t)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (big_bits(value->type_name)) {
                fprintf(f, "clair_%c%d_print(", big_tag(value->type_name), big_bits(value->type_name));
                if (!emit_expr(f, value)) return 0;
                fputs(");\n", f);
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
            {
                FastPlan plan;
                const char *var = NULL;
                const AstNode *stop = NULL;
                if (fast_plan_while(n, &plan, &var, &stop)) {
                    /*
                     * Same idea as the for loop: prove the bounds once, at entry.
                     * The bound is copied to a local so stores into the list can
                     * never look like they change it (Clang can then vectorize).
                     */
                    int64_t kstop;
                    int stop_const = const_int_value(stop, &kstop) || stop->kind == AST_IDENTIFIER;
                    if (!stop_const) {
                        fputs("{\n", f);
                        indent(f, depth + 1);
                        fprintf(f, "int64_t clair_stop_%s = ", var);
                        if (!emit_expr(f, stop)) return 0;
                        fputs(";\n", f);
                        depth++;
                        indent(f, depth);
                    }
                    if (unchecked_mode) fputs("{\n", f);
                    else fputs("if (", f);
                    for (size_t i = 0; i < plan.check_count && !unchecked_mode; i++) {
                        BoundsCheck *b = &plan.checks[i];
                        if (i != 0) {
                            fputs(" &&\n", f);
                            indent(f, depth + 1);
                        }
                        const char *bexpr = fast_plan_expr(&plan, b->base);
                        if (b->dim == 0) fprintf(f, "clair_rt_affine_ok(%s, %s, %s, %s, ", bexpr, b->c0, b->c1, var);
                        else fprintf(f, "clair_rt_affine_in(%s, %s, %s, ", b->c0, b->c1, var);
                        if (stop_const) { if (!emit_expr(f, stop)) return 0; }
                        else fprintf(f, "clair_stop_%s", var);
                        if (b->dim == 1) fprintf(f, ", %s->rows", bexpr);
                        if (b->dim == 2) fprintf(f, ", %s->cols", bexpr);
                        fputc(')', f);
                    }
                    if (!unchecked_mode) fputs(") {\n", f);
                    for (size_t i = 0; i < plan.base_count; i++) {
                        int matrix = is_matrix_type(plan.base_types[i]);
                        const char *elem = matrix ? matrix_element_c_type(plan.base_types[i])
                                                  : list_element_c_type(plan.base_types[i]);
                        indent(f, depth + 1);
                        fprintf(f, "%s *clair_data_%s = (%s *)%s->data;\n",
                                elem, plan.bases[i], elem, plan.base_exprs[i]);
                        if (matrix) {
                            indent(f, depth + 1);
                            fprintf(f, "int64_t clair_cols_%s = %s->cols;\n", plan.bases[i], plan.base_exprs[i]);
                        }
                    }
                    for (size_t i = 0; i < plan.access_count; i++) fast_access[i] = plan.accesses[i];
                    fast_access_count = plan.access_count;
                    indent(f, depth + 1);
                    if (stop_const) {
                        fputs("while (", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputs(") {\n", f);
                    } else {
                        fprintf(f, "while (%s < clair_stop_%s) {\n", var, var);
                    }
                    fast_access_active = 1;
                    int ok = emit_block(f, n->children[1], depth + 2);
                    fast_access_active = 0;
                    fast_access_count = 0;
                    if (!ok) return 0;
                    indent(f, depth + 1);
                    fputs("}\n", f);
                    if (unchecked_mode) {
                        indent(f, depth);
                        fputs("}\n", f);
                    } else {
                        indent(f, depth);
                        fputs("} else {\n", f);
                        indent(f, depth + 1);
                        fputs("while (", f);
                        if (!emit_expr(f, n->children[0])) return 0;
                        fputs(") {\n", f);
                        if (!emit_block(f, n->children[1], depth + 2)) return 0;
                        indent(f, depth + 1);
                        fputs("}\n", f);
                        indent(f, depth);
                        fputs("}\n", f);
                    }
                    if (!stop_const) {
                        depth--;
                        indent(f, depth);
                        fputs("}\n", f);
                    }
                    fast_plan_free(&plan);
                    return 1;
                }
            }
            fputs("while (", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputs(") {\n", f);
            if (!emit_block(f, n->children[1], depth + 1)) return 0;
            indent(f, depth);
            fputs("}\n", f);
            return 1;

        case AST_FOR: {
            if (!n->text || n->child_count < 2 || n->child_count > 4) return 0;

            MatvecPattern mv;
            memset(&mv, 0, sizeof(mv));
            int is_matvec = !matvec_suppress && !matmul_suppress && !fast_suppress && matvec_pattern(n, &mv);
            if (is_matvec) {
                /* Matrix x vector: the sums by the kernel when the shapes fit and nothing written aliases w, x, b. */
                const AstNode *body = mv.body;
                PureCtx lc;
                int fused = layer_helpers && layer_fusable(&mv, &lc);   /* the rest of each row inside the job */
                int first = 1;
                #define MV_AND() do { if (!first) { fputs(" &&\n", f); indent(f, depth + 1); } first = 0; } while (0)
                fputs("if (", f);
                if (mv.w_matrix) {
                    if (!is_size_of(mv.m_stop, "@rows", mv.w_node)) {
                        MV_AND();
                        if (!emit_expr(f, mv.m_stop)) return 0;
                        fprintf(f, " <= %s->rows", mv.w_expr);
                    }
                    if (!is_size_of(mv.n_stop, "@cols", mv.w_node)) {
                        MV_AND();
                        if (!emit_expr(f, mv.n_stop)) return 0;
                        fprintf(f, " <= %s->cols", mv.w_expr);
                    }
                } else {
                    MV_AND();
                    fprintf(f, "clair_rt_matvec_list_fits(%s, ", mv.w_expr);
                    if (!emit_expr(f, mv.ld)) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, mv.m_stop)) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, mv.n_stop)) return 0;
                    fputc(')', f);
                }
                if (!is_size_of(mv.n_stop, "len", mv.x_node)) {
                    MV_AND();
                    if (!emit_expr(f, mv.n_stop)) return 0;
                    fprintf(f, " <= (int64_t)%s->len", mv.x_expr);
                }
                if (mv.b_expr && !is_size_of(mv.m_stop, "len", mv.b_node)) {
                    MV_AND();
                    if (!emit_expr(f, mv.m_stop)) return 0;
                    fprintf(f, " <= (int64_t)%s->len", mv.b_expr);
                }
                for (size_t t = 0; t < mv.target_count; t++) {
                    MV_AND();
                    fprintf(f, "%s->data != %s->data && %s->data != %s->data", mv.targets[t], mv.w_expr,
                            mv.targets[t], mv.x_expr);
                    if (mv.b_expr) fprintf(f, " && %s->data != %s->data", mv.targets[t], mv.b_expr);
                }
                for (size_t t = 0; fused && t < lc.target_count; t++) {
                    MV_AND();
                    if (!emit_expr(f, mv.m_stop)) return 0;
                    fprintf(f, " <= (int64_t)%s->len", lc.target_exprs[t]);
                }
                if (first) fputs("true", f);
                #undef MV_AND
                fputs(") {\n", f);

                const char *ct = kernel_ctype(mv.elem), *sfx = is_f32(mv.elem) ? "_f32" : "";
                indent(f, depth + 1);
                fprintf(f, "%s *clair_data_%s = (%s *)%s->data;\n", ct, mv.w_name, ct, mv.w_expr);
                if (mv.w_matrix) {
                    indent(f, depth + 1);
                    fprintf(f, "int64_t clair_cols_%s = %s->cols;\n", mv.w_name, mv.w_expr);
                }
                if (strcmp(mv.x_name, mv.w_name) != 0) {
                    indent(f, depth + 1);
                    fprintf(f, "%s *clair_data_%s = (%s *)%s->data;\n", ct, mv.x_name, ct, mv.x_expr);
                }
                if (mv.b_expr && strcmp(mv.b_name, mv.w_name) != 0 && strcmp(mv.b_name, mv.x_name) != 0) {
                    indent(f, depth + 1);
                    fprintf(f, "%s *clair_data_%s = (%s *)%s->data;\n", ct, mv.b_name, ct, mv.b_expr);
                }
                /*
                 * 1. every sum, on all cores (nothing else happens meanwhile:
                 *    the sums only read w, x and their starting values);
                 * 2. what followed each sum, row by row, in the original order.
                 */
                indent(f, depth + 1);
                fprintf(f, "%s clair_sums_small[CLAIR_SUMS_SMALL];\n", ct);
                indent(f, depth + 1);
                fprintf(f, "%s *clair_sums = clair_rt_sums_make%s(", ct, sfx);
                if (!emit_expr(f, mv.m_stop)) return 0;
                fputs(", clair_sums_small);\n", f);
                if (owned_live_count >= OWN_MAX) return 0;
                owned_live[owned_live_count++] = (OwnedVar){ "clair_sums", is_f32(mv.elem) ? 3 : 2 };
                indent(f, depth + 1);
                fprintf(f, "for (int64_t %s = 0; %s < ", mv.i, mv.i);
                if (!emit_operand(f, mv.m_stop, PREC_RELATIONAL, 1)) return 0;
                fprintf(f, "; %s++) clair_sums[%s] = ", mv.i, mv.i);
                if (mv.b_expr) fprintf(f, "clair_data_%s[%s]", mv.b_name, mv.i);
                else if (!emit_expr(f, mv.init)) return 0;
                fputs(";\n", f);
                int ok = 1;
                if (fused) {
                    /* each core: the sums of its rows, then what follows them (clair_layer_<id>) */
                    char id[200];
                    snprintf(id, sizeof(id), "%s_%d", layer_fn_name, ++layer_count);
                    if (!emit_layer_helper(layer_helpers, id, &mv, &lc)) return 0;
                    indent(f, depth + 1);
                    fprintf(f, "ClairLayer_%s clair_layer = { { ", id);
                    if (!emit_expr(f, mv.n_stop)) return 0;
                    fputs(", ", f);
                    if (mv.w_matrix) fprintf(f, "clair_cols_%s", mv.w_name);
                    else if (!emit_expr(f, mv.ld)) return 0;
                    fprintf(f, ", clair_data_%s, clair_data_%s, clair_sums }", mv.w_name, mv.x_name);
                    for (size_t t = 0; t < lc.target_count; t++) {
                        fprintf(f, ", (%s *)%s->data", list_element_c_type(lc.target_types[t]), lc.target_exprs[t]);
                    }
                    for (size_t k = 0; k < lc.capture_count; k++) fprintf(f, ", %s", lc.captures[k]->text);
                    fputs(" };\n", f);
                    indent(f, depth + 1);
                    fprintf(f, "clair_rt_layer_%s(", is_f32(mv.elem) ? "f32" : "f64");
                    if (!emit_expr(f, mv.m_stop)) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, mv.n_stop)) return 0;
                    fprintf(f, ", clair_layer_%s, &clair_layer);\n", id);
                } else {
                    indent(f, depth + 1);
                    fprintf(f, "clair_rt_matvec_%s(", is_f32(mv.elem) ? "f32" : "f64");
                    if (!emit_expr(f, mv.m_stop)) return 0;
                    fputs(", ", f);
                    if (!emit_expr(f, mv.n_stop)) return 0;
                    fprintf(f, ", clair_data_%s, ", mv.w_name);
                    if (mv.w_matrix) fprintf(f, "clair_cols_%s", mv.w_name);
                    else if (!emit_expr(f, mv.ld)) return 0;
                    fprintf(f, ", clair_data_%s, clair_sums);\n", mv.x_name);
                }
                if (!fused && body->child_count > 2) {
                    int uses_s = 0;
                    for (size_t st = 2; st < body->child_count; st++) uses_s |= name_mentioned_in(body->children[st], mv.s);
                    indent(f, depth + 1);
                    fprintf(f, "for (int64_t %s = 0; %s < ", mv.i, mv.i);
                    if (!emit_operand(f, mv.m_stop, PREC_RELATIONAL, 1)) return 0;
                    fprintf(f, "; %s++) {\n", mv.i);
                    if (uses_s) {
                        indent(f, depth + 2);
                        fprintf(f, "%s %s = clair_sums[%s];\n", ct, mv.s, mv.i);
                    }
                    size_t own_mark = owned_live_count;
                    for (size_t st = 2; st < body->child_count && ok; st++) ok = emit_stmt(f, body->children[st], depth + 2);
                    if (!ok) return 0;
                    if (body->children[body->child_count - 1]->kind != AST_RETURN) emit_free_since(f, own_mark, depth + 2);
                    owned_live_count = own_mark;
                    indent(f, depth + 1);
                    fputs("}\n", f);
                }
                owned_live_count--;                     /* clair_sums */
                indent(f, depth + 1);
                fprintf(f, "clair_rt_sums_free%s(clair_sums, clair_sums_small);\n", sfx);
                matvec_suppress = 1;
                if (ok) {
                    indent(f, depth);
                    fputs("} else {\n", f);
                    ok = emit_stmt(f, n, depth + 1);
                }
                matvec_suppress = 0;
                fast_plan_free(&mv.bases);
                if (!ok) return 0;
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }
            fast_plan_free(&mv.bases);

            MatmulPattern mm;
            if (!matmul_suppress && !fast_suppress && matmul_pattern(n, &mm)) {
                /*
                 * Matrix product: when the shapes fit and c is neither a nor b,
                 * the blocked kernel does exactly the additions of the loops
                 * below, in the same order for each c[i][j].
                 */
                const char *a = mm.a, *b = mm.b, *c = mm.c;
                if (mm.list_form) {
                    fprintf(f, "if (clair_rt_matmul_list_ok(%s, ", a);
                    if (!emit_expr(f, mm.lda)) return 0;
                    fprintf(f, ", %s, ", b);
                    if (!emit_expr(f, mm.ldb)) return 0;
                    fprintf(f, ", %s, ", c);
                    if (!emit_expr(f, mm.ldc)) return 0;
                } else {
                    fprintf(f, "if (clair_rt_matmul_ok(%s, %s, %s", a, b, c);
                }
                const AstNode *dims[3] = { mm.m_stop, mm.n_stop, mm.k_stop };
                for (int d = 0; d < 3; d++) {
                    fputs(", ", f);
                    if (!emit_expr(f, dims[d])) return 0;
                }
                fputs(")) {\n", f);
                indent(f, depth + 1);
                fprintf(f, "clair_rt_matmul_%s(", is_f32(mm.elem) ? "f32" : "f64");
                for (int d = 0; d < 3; d++) {
                    if (!emit_expr(f, dims[d])) return 0;
                    fputs(d < 2 ? ", " : ",\n", f);
                }
                const char *names[3] = { a, b, c };
                const AstNode *lds[3] = { mm.lda, mm.ldb, mm.ldc };
                for (int d = 0; d < 3; d++) {
                    indent(f, depth + 2);
                    fprintf(f, "(%s%s *)%s->data, ", d < 2 ? "const " : "", kernel_ctype(mm.elem), names[d]);
                    if (mm.list_form) { if (!emit_expr(f, lds[d])) return 0; }
                    else fprintf(f, "%s->cols", names[d]);
                    fputs(d < 2 ? ",\n" : ");\n", f);
                }
                indent(f, depth);
                fputs("} else {\n", f);
                matmul_suppress = 1;
                int ok = emit_stmt(f, n, depth + 1);
                matmul_suppress = 0;
                if (!ok) return 0;
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }

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
                int fast = step == 1 && (fast_plan_nest(n, &plan) || fast_plan_for(n, &plan));
                const AstNode *nest_inner = fast ? plan.inner_loop : NULL;
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
                    if (unchecked_mode) fputs("{\n", f);
                    else fputs("if (", f);
                    for (size_t i = 0; i < plan.check_count && !unchecked_mode; i++) {
                        BoundsCheck *b = &plan.checks[i];
                        if (i != 0) {
                            fputs(" &&\n", f);
                            indent(f, inner + 1);
                        }
                        const char *bexpr = fast_plan_expr(&plan, b->base);
                        const char *two = b->c2 ? "2" : "";
                        if (b->dim == 0) fprintf(f, "clair_rt_affine%s_ok(%s, %s, %s, ", two, bexpr, b->c0, b->c1);
                        else fprintf(f, "clair_rt_affine%s_in(%s, %s, ", two, b->c0, b->c1);
                        if (start_text) fputs(start_text, f);
                        else if (!start_node) fputs("0", f);
                        else if (!emit_expr(f, start_node)) return 0;
                        fputs(", ", f);
                        if (stop_text) fputs(stop_text, f);
                        else if (!emit_expr(f, stop_node)) return 0;
                        if (b->c2) {
                            /* inner loop: its coefficient and its range */
                            size_t iargc = nest_inner->child_count - 1;
                            fprintf(f, ", %s, ", b->c2);
                            if (iargc == 1) fputs("0", f);
                            else if (!emit_expr(f, nest_inner->children[0])) return 0;
                            fputs(", ", f);
                            if (!emit_expr(f, nest_inner->children[iargc == 1 ? 0 : 1])) return 0;
                        }
                        if (b->dim == 1) fprintf(f, ", %s->rows", bexpr);
                        if (b->dim == 2) fprintf(f, ", %s->cols", bexpr);
                        fputc(')', f);
                    }
                    if (!unchecked_mode) fputs(") {\n", f);

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

                    for (size_t i = 0; i < plan.access_count; i++) fast_access[i] = plan.accesses[i];
                    fast_access_count = plan.access_count;

                    /* fixed-width matrices: one more copy with the width as a constant */
                    width_override_count = 0;
                    for (size_t i = 0; i < plan.base_count; i++) {
                        if (!is_matrix_type(plan.base_types[i])) continue;
                        int64_t w = fast_plan_const_width(&plan, plan.bases[i]);
                        if (w > 0) width_override[width_override_count++] = (WidthOverride){ plan.bases[i], w };
                    }
                    int ok = 1;
                    int fast_depth = inner + 1;
                    if (width_override_count > 0) {
                        indent(f, inner + 1);
                        fputs("if (", f);
                        for (size_t i = 0; i < width_override_count; i++) {
                            if (i != 0) fputs(" && ", f);
                            fprintf(f, "clair_cols_%s == %lld", width_override[i].base,
                                    (long long)width_override[i].width);
                        }
                        fputs(") {\n", f);
                        indent(f, inner + 2);
                        fputs("/* row width known: Clang can vectorize like hand-written C */\n", f);
                        EMIT_FOR_HEADER(inner + 2);
                        fast_access_active = 1;
                        fast_suppress = nest_inner != NULL;
                        ok = emit_block(f, body, inner + 3);
                        fast_suppress = 0;
                        fast_access_active = 0;
                        width_override_count = 0;
                        if (!ok) return 0;
                        indent(f, inner + 2);
                        fputs("}\n", f);
                        indent(f, inner + 1);
                        fputs("} else {\n", f);
                        fast_depth = inner + 2;
                    }

                    EMIT_FOR_HEADER(fast_depth);
                    fast_access_active = 1;
                    fast_suppress = nest_inner != NULL;
                    ok = emit_block(f, body, fast_depth + 1);
                    fast_suppress = 0;
                    fast_access_active = 0;
                    if (!ok) return 0;
                    indent(f, fast_depth);
                    fputs("}\n", f);
                    if (fast_depth != inner + 1) {
                        indent(f, inner + 1);
                        fputs("}\n", f);
                    }
                    fast_access_count = 0;

                    if (unchecked_mode) {
                        indent(f, inner);
                        fputs("}\n", f);
                    } else {
                        indent(f, inner);
                        fputs("} else {\n", f);
                        EMIT_FOR_HEADER(inner + 1);
                        if (!emit_block(f, body, inner + 2)) return 0;
                        indent(f, inner + 1);
                        fputs("}\n", f);
                        indent(f, inner);
                        fputs("}\n", f);
                    }
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

        case AST_FOR_EACH: {
            /*
             * for v in xs { ... }  —  Python semantics: xs is evaluated once,
             * v takes xs[0], xs[1], ... while the index is below len(xs).
             */
            if (!n->text || n->child_count != 2 || !n->type_name) return 0;
            const char *elem = list_element_c_type(n->type_name);
            if (!elem) return 0;
            const AstNode *list = n->children[0], *body = n->children[1];
            const char *v = n->text;
            const char *gap = elem[strlen(elem) - 1] == '*' ? "" : " ";   /* "double v" / "const char *v" */

            if (!tree_may_grow_lists(body)) {
                /*
                 * Nothing in the body can append to a list: the length and the
                 * data pointer cannot change, so they are read once and every
                 * element is in range by construction (no check at all).
                 */
                fputs("{\n", f);
                if (!is_access_path(list)) {
                    indent(f, depth + 1);
                    fprintf(f, "ClairList clair_list_%s = ", v);
                    if (!emit_expr(f, list)) return 0;
                    fputs(";\n", f);
                }
                int used = name_mentioned_in(body, v);
                if (used) {
                    indent(f, depth + 1);
                    fprintf(f, "%s%s*clair_items_%s = (%s%s*)", elem, gap, v, elem, gap);
                    if (is_access_path(list)) { if (!emit_operand(f, list, PREC_POSTFIX, 0)) return 0; }
                    else fprintf(f, "clair_list_%s", v);
                    fputs("->data;\n", f);
                }
                indent(f, depth + 1);
                fprintf(f, "int64_t clair_count_%s = (int64_t)", v);
                if (is_access_path(list)) { if (!emit_operand(f, list, PREC_POSTFIX, 0)) return 0; }
                else fprintf(f, "clair_list_%s", v);
                fputs("->len;\n", f);
                indent(f, depth + 1);
                fprintf(f, "for (int64_t clair_k_%s = 0; clair_k_%s < clair_count_%s; clair_k_%s++) {\n", v, v, v, v);
                if (used) {
                    indent(f, depth + 2);
                    fprintf(f, "%s%s%s = clair_items_%s[clair_k_%s];\n", elem, gap, v, v, v);
                }
                if (!emit_block(f, body, depth + 2)) return 0;
                indent(f, depth + 1);
                fputs("}\n", f);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }

            /*
             * The body may append (possibly to xs itself): like Python, the
             * length and the data are read again at every step.
             */
            int direct = list->kind == AST_IDENTIFIER && list->text && strcmp(list->text, v) != 0 &&
                         !name_changes_in(body, list->text);
            int d = depth;
            if (!direct) {
                fputs("{\n", f);
                indent(f, depth + 1);
                fprintf(f, "ClairList clair_list_%s = ", v);
                if (!emit_expr(f, list)) return 0;
                fputs(";\n", f);
                d = depth + 1;
                indent(f, d);
            }
            char *name = direct ? str_dup(list->text) : str_fmt("clair_list_%s", v);
            if (!name) return 0;
            fprintf(f, "for (int64_t clair_k_%s = 0; clair_k_%s < (int64_t)%s->len; clair_k_%s++) {\n", v, v, name, v);
            if (name_mentioned_in(body, v)) {
                indent(f, d + 1);
                fprintf(f, "%s%s%s = ((%s%s*)%s->data)[clair_k_%s];\n", elem, gap, v, elem, gap, name, v);
            }
            free(name);
            if (!emit_block(f, body, d + 1)) return 0;
            indent(f, d);
            fputs("}\n", f);
            if (!direct) {
                indent(f, depth);
                fputs("}\n", f);
            }
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
        "}\n\n",
        f
    );
    fputs(
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

static int node_prints_type(const AstNode *n, const char *type) {
    if (!n) return 0;
    if (n->kind == AST_PRINT && n->child_count == 1 && n->children[0]->type_name &&
        strcmp(n->children[0]->type_name, type) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_prints_type(n->children[i], type)) return 1;
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
        strncmp(n->text, "@slice", 6) != 0 && !small_machine_bits(n->text + 1) &&
        !(machine_type_bits(n->type_name) &&
          (strcmp(n->text, "@abs") == 0 || strcmp(n->text, "@min") == 0 || strcmp(n->text, "@max") == 0))) return 1;
    if (n->kind == AST_BINARY && n->text && strcmp(n->text, "%") == 0 && !machine_type_bits(n->type_name)) return 1;
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


/* ------------------------------------------------------------------ */
/* Big integers int128 .. int1024 (runtime emitted only when used)    */
/* ------------------------------------------------------------------ */

/* slices of lists (strings = 0) or of strings (strings = 1) */
static int node_uses_slice(const AstNode *n, int strings) {
    if (!n) return 0;
    if (n->kind == AST_CALL && n->text && strncmp(n->text, "@slice", 6) == 0 && n->type_name &&
        (strcmp(n->type_name, "string") == 0) == strings) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_slice(n->children[i], strings)) return 1;
    }
    return 0;
}

/* Python's slice.indices(): the first index of xs[start:stop:step] and how many elements */
static void emit_slice_count(FILE *f) {
    fputs(
        "#define CLAIR_NO_START 1   /* xs[:b] */\n"
        "#define CLAIR_NO_STOP 2    /* xs[a:] */\n\n"
        "static int64_t clair_rt_slice_count(int64_t len, int64_t *start, int64_t stop, int64_t step, int missing) {\n"
        "    if (step == 0) { fputs(\"Clair runtime error: slice step cannot be zero\\n\", stderr); exit(EXIT_FAILURE); }\n"
        "    int64_t lower = step < 0 ? -1 : 0, upper = step < 0 ? len - 1 : len;\n"
        "    int64_t a = *start, b = stop;\n"
        "    if (missing & CLAIR_NO_START) a = step < 0 ? upper : lower;\n"
        "    else if (a < 0) { a += len; if (a < lower) a = lower; }   /* -1 is the last element */\n"
        "    else if (a > upper) a = upper;\n"
        "    if (missing & CLAIR_NO_STOP) b = step < 0 ? lower : upper;\n"
        "    else if (b < 0) { b += len; if (b < lower) b = lower; }\n"
        "    else if (b > upper) b = upper;\n"
        "    *start = a;\n"
        "    if (step < 0) return b < a ? (a - b - 1) / -step + 1 : 0;\n"
        "    return a < b ? (b - a - 1) / step + 1 : 0;\n"
        "}\n\n",
        f);
}

static int node_uses_type(const AstNode *n, const char *type) {
    if (!n) return 0;
    if (n->type_name && strcmp(n->type_name, type) == 0) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (node_uses_type(n->children[i], type)) return 1;
    }
    return 0;
}

static const char *const big_types[] = {
    "int128", "int256", "int512", "int1024", "uint128", "uint256", "uint512", "uint1024"
};

static int program_uses_big(const AstNode *program) {
    for (size_t i = 0; i < 8; i++) if (node_uses_type(program, big_types[i])) return 1;
    return 0;
}

static void emit_big_runtime(FILE *f, const AstNode *program) {
    int used[8];
    for (int i = 0; i < 8; i++) used[i] = node_uses_type(program, big_types[i]);
    fputs(
        "/*\n"
        " * Big integers int128 .. int1024 and uint128 .. uint1024: n words of 64\n"
        " * bits, least significant first; the signed ones in two's complement.\n"
        " * Every operation checks overflow (like division by zero): a result that\n"
        " * does not fit stops the program.\n"
        " */\n"
        "static _Noreturn void clair_rt_big_fail(const char *message) {\n"
        "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
        "    exit(EXIT_FAILURE);\n"
        "}\n"
        "\n"
        "static inline bool clair_rt_big_is_neg(const uint64_t *a, int n) { return (a[n - 1] >> 63) != 0; }\n"
        "\n"
        "/* a = -a, wrapping around (used on magnitudes) */\n"
        "static inline void clair_rt_big_negate(uint64_t *a, int n) {\n"
        "    uint64_t carry = 1;\n"
        "    for (int i = 0; i < n; i++) {\n"
        "        a[i] = ~a[i] + carry;\n"
        "        carry = carry && a[i] == 0;\n"
        "    }\n"
        "}\n"
        "\n"
        "/* magnitude of a as an unsigned number (the most negative value fits too) */\n"
        "static inline void clair_rt_big_abs(uint64_t *m, const uint64_t *a, int n) {\n"
        "    memcpy(m, a, (size_t)n * sizeof(uint64_t));\n"
        "    if (clair_rt_big_is_neg(a, n)) clair_rt_big_negate(m, n);\n"
        "}\n"
        "\n"
        "/* ---- numbers 0 and above (uint, and the magnitudes of int) ---- */\n"
        "\n"
        "/* r = a + b; true when the result needs more than n words */\n"
        "static inline bool clair_rt_ubig_add(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    uint64_t carry = 0;\n"
        "    for (int i = 0; i < n; i++) {\n"
        "        uint64_t s = a[i] + b[i];\n"
        "        uint64_t c = s < a[i];\n"
        "        r[i] = s + carry;\n"
        "        carry = c | (r[i] < s);\n"
        "    }\n"
        "    return carry != 0;\n"
        "}\n"
        "\n"
        "/* r = a - b; true when the result is negative */\n"
        "static inline bool clair_rt_ubig_sub(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    uint64_t borrow = 0;\n"
        "    for (int i = 0; i < n; i++) {\n"
        "        uint64_t d = a[i] - b[i];\n"
        "        uint64_t c = a[i] < b[i];\n"
        "        r[i] = d - borrow;\n"
        "        borrow = c | (d < borrow);\n"
        "    }\n"
        "    return borrow != 0;\n"
        "}\n"
        "\n"
        "/* r = a * b (schoolbook); true when the result needs more than n words */\n"
        "static bool clair_rt_ubig_mul(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    uint64_t p[32] = { 0 };\n"
        "    for (int i = 0; i < n; i++) {\n"
        "        if (!a[i]) continue;\n"
        "        uint64_t carry = 0;\n"
        "        for (int j = 0; j < n; j++) {\n"
        "            unsigned __int128 t = (unsigned __int128)a[i] * b[j] + p[i + j] + carry;\n"
        "            p[i + j] = (uint64_t)t;\n"
        "            carry = (uint64_t)(t >> 64);\n"
        "        }\n"
        "        p[i + n] = carry;\n"
        "    }\n"
        "    memcpy(r, p, (size_t)n * sizeof(uint64_t));\n"
        "    for (int i = n; i < 2 * n; i++) if (p[i]) return true;\n"
        "    return false;\n"
        "}\n",
        f
    );
    fputs(
        "/* q = a / b and m = a % b; true for a division by zero */\n"
        "static bool clair_rt_ubig_divmod(uint64_t *q, uint64_t *m, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    bool zero = true;\n"
        "    for (int i = 0; i < n; i++) if (b[i]) zero = false;\n"
        "    if (zero) return true;\n"
        "    uint64_t quo[16] = { 0 }, rem[16] = { 0 };\n"
        "    int top = 64 * n - 1;\n"
        "    while (top >= 0 && !((a[top / 64] >> (top % 64)) & 1)) top--;\n"
        "    for (int bit = top; bit >= 0; bit--) {            /* long division, one bit at a time */\n"
        "        bool carry = rem[n - 1] >> 63;                 /* rem may use the top bit (uint) */\n"
        "        for (int i = n - 1; i > 0; i--) rem[i] = (rem[i] << 1) | (rem[i - 1] >> 63);\n"
        "        rem[0] = (rem[0] << 1) | ((a[bit / 64] >> (bit % 64)) & 1);\n"
        "        int i = n - 1;\n"
        "        while (i > 0 && rem[i] == b[i]) i--;\n"
        "        if (carry || rem[i] >= b[i]) {\n"
        "            uint64_t borrow = 0;\n"
        "            for (int k = 0; k < n; k++) {\n"
        "                uint64_t d = rem[k] - b[k];\n"
        "                uint64_t c = rem[k] < b[k];\n"
        "                rem[k] = d - borrow;\n"
        "                borrow = c | (d < borrow);\n"
        "            }\n"
        "            quo[bit / 64] |= UINT64_C(1) << (bit % 64);\n"
        "        }\n"
        "    }\n"
        "    memcpy(q, quo, (size_t)n * sizeof(uint64_t));\n"
        "    memcpy(m, rem, (size_t)n * sizeof(uint64_t));\n"
        "    return false;\n"
        "}\n"
        "\n"
        "static inline int clair_rt_ubig_cmp(const uint64_t *a, const uint64_t *b, int n) {\n"
        "    for (int i = n - 1; i >= 0; i--) {\n"
        "        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "/* nearest double: the top 128 bits plus a sticky bit for the rest round correctly */\n"
        "static double clair_rt_ubig_to_double(const uint64_t *x, int n) {\n"
        "    int t = n - 1;\n"
        "    while (t > 0 && !x[t]) t--;\n"
        "    if (t == 0) return (double)x[0];\n"
        "    unsigned __int128 top = ((unsigned __int128)x[t] << 64) | x[t - 1];\n"
        "    for (int i = 0; i < t - 1; i++) if (x[i]) { top |= 1; break; }\n"
        "    return ldexp((double)top, 64 * (t - 1));\n"
        "}\n"
        "\n"
        "/* prints the digits of x (destroyed), after a '-' when negative */\n"
        "static void clair_rt_ubig_print(uint64_t *x, int n, bool negative) {\n"
        "    uint64_t chunks[20];\n"
        "    int count = 0;\n"
        "    for (;;) {                                         /* groups of 19 digits */\n"
        "        unsigned __int128 rem = 0;\n"
        "        bool more = false;\n"
        "        for (int i = n - 1; i >= 0; i--) {\n"
        "            unsigned __int128 cur = (rem << 64) | x[i];\n"
        "            x[i] = (uint64_t)(cur / UINT64_C(10000000000000000000));\n"
        "            rem = cur % UINT64_C(10000000000000000000);\n"
        "            if (x[i]) more = true;\n"
        "        }\n"
        "        chunks[count++] = (uint64_t)rem;\n"
        "        if (!more) break;\n"
        "    }\n"
        "    if (negative) putchar('-');\n"
        "    printf(\"%\" PRIu64, chunks[count - 1]);\n"
        "    for (int i = count - 2; i >= 0; i--) printf(\"%019\" PRIu64, chunks[i]);\n"
        "    putchar('\\n');\n"
        "}\n"
        "\n"
        "/* ---- signed numbers (int) ---- */\n"
        "\n"
        "/* r = a + b; true on overflow */\n"
        "static inline bool clair_rt_big_add(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    bool sa = clair_rt_big_is_neg(a, n), sb = clair_rt_big_is_neg(b, n);\n"
        "    clair_rt_ubig_add(r, a, b, n);\n"
        "    return sa == sb && clair_rt_big_is_neg(r, n) != sa;\n"
        "}\n"
        "\n"
        "/* r = a - b; true on overflow */\n"
        "static inline bool clair_rt_big_sub(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    bool sa = clair_rt_big_is_neg(a, n), sb = clair_rt_big_is_neg(b, n);\n"
        "    clair_rt_ubig_sub(r, a, b, n);\n"
        "    return sa != sb && clair_rt_big_is_neg(r, n) != sa;\n"
        "}\n",
        f
    );
    fputs(
        "/* r = a * b; true on overflow (product of the magnitudes) */\n"
        "static bool clair_rt_big_mul(uint64_t *r, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    uint64_t x[16], y[16];\n"
        "    bool negative = clair_rt_big_is_neg(a, n) != clair_rt_big_is_neg(b, n);\n"
        "    clair_rt_big_abs(x, a, n);\n"
        "    clair_rt_big_abs(y, b, n);\n"
        "    if (clair_rt_ubig_mul(r, x, y, n)) return true;\n"
        "    if (r[n - 1] >> 63) {                      /* only the most negative value is allowed */\n"
        "        if (!negative || r[n - 1] != (UINT64_C(1) << 63)) return true;\n"
        "        for (int i = 0; i < n - 1; i++) if (r[i]) return true;\n"
        "    }\n"
        "    if (negative) clair_rt_big_negate(r, n);\n"
        "    return false;\n"
        "}\n"
        "\n"
        "/*\n"
        " * q = a / b and m = a % b, truncated toward zero like Clair int.\n"
        " * Returns 0, 1 for a division by zero, 2 for the most negative value / -1.\n"
        " */\n"
        "static int clair_rt_big_divmod(uint64_t *q, uint64_t *m, const uint64_t *a, const uint64_t *b, int n) {\n"
        "    bool minus_one = true, most_negative = a[n - 1] == (UINT64_C(1) << 63);\n"
        "    for (int i = 0; i < n; i++) {\n"
        "        if (b[i] != UINT64_MAX) minus_one = false;\n"
        "        if (i < n - 1 && a[i]) most_negative = false;\n"
        "    }\n"
        "    if (most_negative && minus_one) return 2;\n"
        "    uint64_t x[16], y[16];\n"
        "    clair_rt_big_abs(x, a, n);\n"
        "    clair_rt_big_abs(y, b, n);\n"
        "    if (clair_rt_ubig_divmod(q, m, x, y, n)) return 1;\n"
        "    if (clair_rt_big_is_neg(a, n) != clair_rt_big_is_neg(b, n)) clair_rt_big_negate(q, n);\n"
        "    if (clair_rt_big_is_neg(a, n)) clair_rt_big_negate(m, n);\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "static inline int clair_rt_big_cmp(const uint64_t *a, const uint64_t *b, int n) {\n"
        "    bool na = clair_rt_big_is_neg(a, n), nb = clair_rt_big_is_neg(b, n);\n"
        "    if (na != nb) return na ? -1 : 1;\n"
        "    return clair_rt_ubig_cmp(a, b, n);\n"
        "}\n"
        "\n"
        "static inline void clair_rt_big_from_i64(uint64_t *r, int64_t v, int n) {\n"
        "    r[0] = (uint64_t)v;\n"
        "    for (int i = 1; i < n; i++) r[i] = v < 0 ? UINT64_MAX : 0;\n"
        "}\n"
        "\n"
        "/*\n"
        " * dst (nd words) = src (ns words), each signed or not.  Returns 0, 1 when a\n"
        " * negative value would go into a uint, 2 when the value does not fit.\n"
        " */\n"
        "static inline int clair_rt_big_convert(uint64_t *dst, int nd, bool dst_signed, const uint64_t *src, int ns, bool src_signed) {\n"
        "    bool negative = src_signed && clair_rt_big_is_neg(src, ns);\n"
        "    if (negative && !dst_signed) return 1;\n"
        "    uint64_t ext = negative ? UINT64_MAX : 0;\n"
        "    for (int i = 0; i < nd; i++) dst[i] = i < ns ? src[i] : ext;\n"
        "    for (int i = nd; i < ns; i++) if (src[i] != ext) return 2;\n"
        "    if (dst_signed && clair_rt_big_is_neg(dst, nd) != negative) return 2;\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "static double clair_rt_big_to_double(const uint64_t *a, int n) {\n"
        "    uint64_t x[16];\n"
        "    clair_rt_big_abs(x, a, n);\n"
        "    double d = clair_rt_ubig_to_double(x, n);\n"
        "    return clair_rt_big_is_neg(a, n) ? -d : d;\n"
        "}\n"
        "\n"
        "static void clair_rt_big_print(const uint64_t *a, int n) {\n"
        "    uint64_t x[16];\n"
        "    clair_rt_big_abs(x, a, n);\n"
        "    clair_rt_ubig_print(x, n, clair_rt_big_is_neg(a, n));\n"
        "}\n",
        f
    );
    fputc('\n', f);
    for (int t = 0; t < 8; t++) {
        if (!used[t]) continue;
        int b = big_bits(big_types[t]), n = b / 64;
        const char *type = c_type_name(big_types[t]), *name = big_types[t];
        char tag = big_tag(name);
        fprintf(f, "typedef struct { uint64_t w[%d]; } %s;\n\n", n, type);
        if (tag == 'i') {
            fprintf(f,
                "static inline %s clair_i%d_from_i64(int64_t v) {\n"
                "    %s r;\n"
                "    clair_rt_big_from_i64(r.w, v, %d);\n"
                "    return r;\n"
                "}\n\n",
                type, b, type, n);
        } else {
            fprintf(f,
                "static inline %s clair_u%d_from_i64(int64_t v) {\n"
                "    if (v < 0) clair_rt_big_fail(\"%s cannot hold a negative value\");\n"
                "    %s r = { { (uint64_t)v } };\n"
                "    return r;\n"
                "}\n\n",
                type, b, name, type);
        }
        static const char *const ops[] = { "add", "sub", "mul" };
        for (int k = 0; k < 3; k++) {
            const char *message = tag == 'u' && k == 1 ? " overflow: negative result" : " overflow";
            fprintf(f,
                "static inline %s clair_%c%d_%s(%s a, %s b) {\n"
                "    %s r;\n"
                "    if (clair_rt_%sbig_%s(r.w, a.w, b.w, %d)) clair_rt_big_fail(\"%s%s\");\n"
                "    return r;\n"
                "}\n\n",
                type, tag, b, ops[k], type, type, type, tag == 'u' ? "u" : "", ops[k], n, name, message);
        }
        if (tag == 'i') {
            fprintf(f,
                "static inline %s clair_i%d_div(%s a, %s b) {\n"
                "    %s q, m;\n"
                "    int status = clair_rt_big_divmod(q.w, m.w, a.w, b.w, %d);\n"
                "    if (status == 1) clair_rt_big_fail(\"division by zero\");\n"
                "    if (status == 2) clair_rt_big_fail(\"integer division overflow\");\n"
                "    return q;\n"
                "}\n\n"
                "static inline %s clair_i%d_mod(%s a, %s b) {\n"
                "    %s q, m;\n"
                "    int status = clair_rt_big_divmod(q.w, m.w, a.w, b.w, %d);\n"
                "    if (status == 1) clair_rt_big_fail(\"modulo by zero\");\n"
                "    if (status == 2) return clair_i%d_from_i64(0);   /* most negative %% -1 */\n"
                "    return m;\n"
                "}\n\n"
                "static inline %s clair_i%d_neg(%s a) {\n"
                "    %s zero = { { 0 } }, r;\n"
                "    if (clair_rt_big_sub(r.w, zero.w, a.w, %d)) clair_rt_big_fail(\"%s overflow\");\n"
                "    return r;\n"
                "}\n\n"
                "static inline %s clair_i%d_abs(%s a) { return clair_rt_big_is_neg(a.w, %d) ? clair_i%d_neg(a) : a; }\n\n"
                "static inline int clair_i%d_cmp(%s a, %s b) { return clair_rt_big_cmp(a.w, b.w, %d); }\n\n"
                "static inline int64_t clair_i%d_to_i64(%s a) {\n"
                "    uint64_t r[1];\n"
                "    if (clair_rt_big_convert(r, 1, true, a.w, %d, true)) clair_rt_big_fail(\"int() argument out of range\");\n"
                "    return (int64_t)r[0];\n"
                "}\n\n"
                "static inline double clair_i%d_to_double(%s a) { return clair_rt_big_to_double(a.w, %d); }\n\n"
                "static inline void clair_i%d_print(%s a) { clair_rt_big_print(a.w, %d); }\n\n",
                type, b, type, type, type, n,
                type, b, type, type, type, n, b,
                type, b, type, type, n, name,
                type, b, type, n, b,
                b, type, type, n,
                b, type, n,
                b, type, n,
                b, type, n);
        } else {
            fprintf(f,
                "static inline %s clair_u%d_div(%s a, %s b) {\n"
                "    %s q, m;\n"
                "    if (clair_rt_ubig_divmod(q.w, m.w, a.w, b.w, %d)) clair_rt_big_fail(\"division by zero\");\n"
                "    return q;\n"
                "}\n\n"
                "static inline %s clair_u%d_mod(%s a, %s b) {\n"
                "    %s q, m;\n"
                "    if (clair_rt_ubig_divmod(q.w, m.w, a.w, b.w, %d)) clair_rt_big_fail(\"modulo by zero\");\n"
                "    return m;\n"
                "}\n\n"
                "static inline int clair_u%d_cmp(%s a, %s b) { return clair_rt_ubig_cmp(a.w, b.w, %d); }\n\n"
                "static inline int64_t clair_u%d_to_i64(%s a) {\n"
                "    uint64_t r[1];\n"
                "    if (clair_rt_big_convert(r, 1, true, a.w, %d, false)) clair_rt_big_fail(\"int() argument out of range\");\n"
                "    return (int64_t)r[0];\n"
                "}\n\n"
                "static inline void clair_u%d_print(%s a) { clair_rt_ubig_print(a.w, %d, false); }\n\n",
                type, b, type, type, type, n,
                type, b, type, type, type, n,
                b, type, type, n,
                b, type, n,
                b, type, n);
            if (b == 1024) {
                fputs("/* only a uint1024 can be beyond the largest double (about 1.8e308) */\n"
                      "static inline double clair_u1024_to_double(ClairUInt1024 a) {\n"
                      "    double d = clair_rt_ubig_to_double(a.w, 16);\n"
                      "    if (d > DBL_MAX) clair_rt_big_fail(\"float() argument out of range\");\n"
                      "    return d;\n"
                      "}\n\n", f);
            } else {
                fprintf(f, "static inline double clair_u%d_to_double(%s a) { return clair_rt_ubig_to_double(a.w, %d); }\n\n",
                        b, type, n);
            }
        }
        fprintf(f,
            "static inline %s clair_%c%d_min(%s a, %s b) { return clair_%c%d_cmp(a, b) <= 0 ? a : b; }\n\n"
            "static inline %s clair_%c%d_max(%s a, %s b) { return clair_%c%d_cmp(a, b) >= 0 ? a : b; }\n\n"
            "/* a^e by squaring: a^13 = a * a^4 * a^8; overflow stops the program */\n"
            "static inline %s clair_%c%d_pow(%s a, int64_t e) {\n"
            "    if (e < 0) clair_rt_big_fail(\"pow() exponent must not be negative\");\n"
            "    %s r = clair_%c%d_from_i64(1);\n"
            "    while (e > 0) {\n"
            "        if (e & 1) r = clair_%c%d_mul(r, a);\n"
            "        e >>= 1;\n"
            "        if (e > 0) a = clair_%c%d_mul(a, a);\n"
            "    }\n"
            "    return r;\n"
            "}\n\n",
            type, tag, b, type, type, tag, b,
            type, tag, b, type, type, tag, b,
            type, tag, b, type,
            type, tag, b,
            tag, b,
            tag, b);
    }
    /* conversions between the types in use: widening never fails */
    for (int t = 0; t < 8; t++) {
        for (int u = 0; u < 8; u++) {
            if (t == u || !used[t] || !used[u]) continue;
            const char *to = big_types[t], *from = big_types[u];
            int bt = big_bits(to), bu = big_bits(from);
            char tt = big_tag(to), tu = big_tag(from);
            fprintf(f, "static inline %s clair_%c%d_from_%c%d(%s a) {\n    %s r;\n", c_type_name(to), tt, bt, tu, bu,
                    c_type_name(from), c_type_name(to));
            if (tt == 'i' && tu == 'i') {
                fprintf(f, "    if (clair_rt_big_convert(r.w, %d, true, a.w, %d, true)) clair_rt_big_fail(\"%s() argument out of range\");\n",
                        bt / 64, bu / 64, to);
            } else {
                fprintf(f, "    int status = clair_rt_big_convert(r.w, %d, %s, a.w, %d, %s);\n", bt / 64,
                        tt == 'i' ? "true" : "false", bu / 64, tu == 'i' ? "true" : "false");
                if (tt == 'u' && tu == 'i') fprintf(f, "    if (status == 1) clair_rt_big_fail(\"%s cannot hold a negative value\");\n", to);
                fprintf(f, "    if (status == 2) clair_rt_big_fail(\"%s() argument out of range\");\n", to);
            }
            fputs("    return r;\n}\n\n", f);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Kernels of matrix products and dense layers: written for float     */
/* (double); the float32 copy is the same text with float, and names  */
/* ending in F32 / _f32.  Its matrix product keeps a 4 x 16 block of  */
/* c in registers (4 float32 per NEON register instead of 2 double).  */
/* ------------------------------------------------------------------ */

static void kernel_kinds_in(const AstNode *n, int matmul, int *kinds) {
    if (!n) return;
    if (n->kind == AST_FOR) {
        if (matmul) {
            MatmulPattern p;
            if (matmul_pattern(n, &p)) *kinds |= is_f32(p.elem) ? 2 : 1;
        } else {
            MatvecPattern p;
            if (matvec_pattern(n, &p)) *kinds |= is_f32(p.elem) ? 2 : 1;
            fast_plan_free(&p.bases);
        }
    }
    for (size_t i = 0; i < n->child_count; i++) kernel_kinds_in(n->children[i], matmul, kinds);
}

/* 1: float kernels used, 2: float32 kernels used */
static int program_kernel_kinds(const AstNode *program, int matmul) {
    int kinds = 0;
    kernel_kinds_in(program, matmul, &kinds);
    return kinds;
}

static void emit_kernel_text(FILE *f, int f32, int with_f64, const char *text) {
    if (!f32) {
        fputs(text, f);
        return;
    }
    static const char *const swaps[][2] = {
        { "#define CLAIR_SUMS_SMALL 64\n", NULL },        /* already there with the float kernels */
        { "ClairMatvecJob", "ClairMatvecJobF32" }, { "clair_rt_matvec_rows", "clair_rt_matvec_rows_f32" },
        { "clair_rt_matvec_f64", "clair_rt_matvec_f32" }, { "clair_rt_layer_f64", "clair_rt_layer_f32" },
        { "clair_rt_sums_make", "clair_rt_sums_make_f32" }, { "clair_rt_sums_free", "clair_rt_sums_free_f32" },
        { "ClairMatmulJob", "ClairMatmulJobF32" }, { "clair_rt_matmul_rows", "clair_rt_matmul_rows_f32" },
        { "clair_rt_matmul_f64", "clair_rt_matmul_f32" },
        { "4 x 8 block", "4 x 16 block" }, { "acc[4][8]", "acc[4][16]" }, { "j + 8 <= n; j += 8", "j + 16 <= n; j += 16" },
        { "q < 8", "q < 16" }, { "double", "float" },
    };
    for (const char *p = text; *p;) {
        size_t k = 0, count = sizeof(swaps) / sizeof(swaps[0]);
        for (; k < count; k++) {
            size_t len = strlen(swaps[k][0]);
            if (strncmp(p, swaps[k][0], len) != 0) continue;
            if (swaps[k][1]) fputs(swaps[k][1], f);
            else if (!with_f64) fputs(swaps[k][0], f);
            p += len;
            break;
        }
        if (k == count) fputc(*p++, f);
    }
}

/* ------------------------------------------------------------------ */
/* Runtime of the machine integers: only the helpers the program uses. */
/* ------------------------------------------------------------------ */

static const char *const machine_names[] = { "int", "i8", "i16", "i32", "u8", "u16", "u32", "u64" };
static const char *const machine_ops[] = { "div", "mod", "shl", "shr", "abs", "min", "max" };
#define MACHINE_TYPES (sizeof(machine_names) / sizeof(machine_names[0]))
#define MACHINE_OPS (sizeof(machine_ops) / sizeof(machine_ops[0]))

typedef struct {
    unsigned char op[MACHINE_TYPES][MACHINE_OPS];
    int count_u64, u64_to_int, float_to_int;
} MachineNeeds;

static void machine_need(MachineNeeds *m, const char *type, const char *op) {
    for (size_t t = 0; t < MACHINE_TYPES; t++) {
        if (!type || strcmp(type, machine_names[t]) != 0) continue;
        for (size_t k = 0; k < MACHINE_OPS; k++) {
            if (strcmp(op, machine_ops[k]) == 0) m->op[t][k] = 1;
        }
    }
}

static void scan_machine_needs(const AstNode *n, MachineNeeds *m) {
    if (!n) return;
    if (n->kind == AST_BINARY && machine_form(n) == MFORM_CALL) {
        machine_need(m, n->type_name, machine_helper(n->text));
        if (n->children[1]->type_name && strcmp(n->children[1]->type_name, "u64") == 0 &&
            (strcmp(n->text, "<<") == 0 || strcmp(n->text, ">>") == 0)) m->count_u64 = 1;
    }
    if (n->kind == AST_CALL && n->text && n->text[0] == '@') {
        const char *name = n->text + 1;
        const char *t0 = n->child_count == 1 ? n->children[0]->type_name : NULL;
        if (small_machine_bits(n->type_name) &&
            ((strcmp(name, "abs") == 0 && !machine_is_unsigned(n->type_name)) ||
             strcmp(name, "min") == 0 || strcmp(name, "max") == 0)) machine_need(m, n->type_name, name);
        if ((strcmp(name, "int") == 0 || small_machine_bits(name)) && t0 &&
            (strcmp(t0, "float") == 0 || strcmp(t0, "float32") == 0)) m->float_to_int = 1;
        if (strcmp(name, "int") == 0 && t0 && strcmp(t0, "u64") == 0) m->u64_to_int = 1;
    }
    for (size_t i = 0; i < n->child_count; i++) scan_machine_needs(n->children[i], m);
}

static void emit_machine_runtime(FILE *f, const MachineNeeds *m) {
    int fails = m->u64_to_int;
    for (size_t t = 0; t < MACHINE_TYPES; t++) {
        for (size_t k = 0; k < 4; k++) fails |= m->op[t][k];
    }
    if (fails) {
        fputs("/* machine integers (u8 .. u64, i8 .. i32): checked divisions and shifts */\n"
              "static _Noreturn void clair_rt_machine_fail(const char *message) {\n"
              "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
              "    exit(EXIT_FAILURE);\n"
              "}\n\n", f);
    }
    if (m->count_u64) {
        fputs("/* a u64 shift count: 64 and more all give the same result */\n"
              "static inline int64_t clair_rt_count_u64(uint64_t n) { return n > 64 ? 64 : (int64_t)n; }\n\n", f);
    }
    if (m->u64_to_int) {
        fputs("static int64_t clair_rt_u64_to_int(uint64_t x) {\n"
              "    if (x > INT64_MAX) clair_rt_machine_fail(\"int() argument out of range\");\n"
              "    return (int64_t)x;\n"
              "}\n\n", f);
    }
    for (size_t t = 0; t < MACHINE_TYPES; t++) {
        const char *name = machine_names[t];
        const char *ct = machine_c_type(name), *ut = machine_unsigned_c(name);
        int bits = machine_type_bits(name), is_unsigned = machine_is_unsigned(name);
        char min_name[16];
        snprintf(min_name, sizeof(min_name), "INT%d_MIN", bits);
        if (m->op[t][0]) {
            fprintf(f, "static %s clair_rt_div_%s(%s a, %s b) {\n", ct, name, ct, ct);
            fputs("    if (b == 0) clair_rt_machine_fail(\"division by zero\");\n", f);
            if (!is_unsigned)
                fprintf(f, "    if (a == %s && b == -1) clair_rt_machine_fail(\"integer division overflow\");\n", min_name);
            fputs("    return a / b;\n}\n\n", f);
        }
        if (m->op[t][1]) {
            fprintf(f, "static %s clair_rt_mod_%s(%s a, %s b) {\n", ct, name, ct, ct);
            fputs("    if (b == 0) clair_rt_machine_fail(\"modulo by zero\");\n", f);
            fputs(is_unsigned ? "    return a % b;\n}\n\n" : "    return b == -1 ? 0 : a % b;\n}\n\n", f);
        }
        if (m->op[t][2]) {
            /* shifted as unsigned: no overflow in C; 'bits' and more push every bit out */
            fprintf(f, "static %s clair_rt_shl_%s(%s a, int64_t n) {\n", ct, name, ct);
            fputs("    if (n < 0) clair_rt_machine_fail(\"negative shift count\");\n", f);
            if (is_unsigned && bits >= 32) fprintf(f, "    return n >= %d ? 0 : a << n;\n}\n\n", bits);
            else if (is_unsigned) fprintf(f, "    return n >= %d ? 0 : (%s)(a << n);\n}\n\n", bits, ct);
            else fprintf(f, "    return n >= %d ? 0 : (%s)((%s)a << n);\n}\n\n", bits, ct, ut);
        }
        if (m->op[t][3]) {
            fprintf(f, "static %s clair_rt_shr_%s(%s a, int64_t n) {\n", ct, name, ct);
            fputs("    if (n < 0) clair_rt_machine_fail(\"negative shift count\");\n", f);
            if (is_unsigned) fprintf(f, "    return n >= %d ? 0 : a >> n;\n}\n\n", bits);
            else fprintf(f, "    return n >= %d ? (a < 0 ? -1 : 0) : a >> n;\n}\n\n", bits);
        }
        if (m->op[t][4])
            fprintf(f, "static inline %s clair_rt_abs_%s(%s x) { return x < 0 ? -x : x; }\n\n", ct, name, ct);
        if (m->op[t][5])
            fprintf(f, "static inline %s clair_rt_min_%s(%s a, %s b) { return a < b ? a : b; }\n\n", ct, name, ct, ct);
        if (m->op[t][6])
            fprintf(f, "static inline %s clair_rt_max_%s(%s a, %s b) { return a > b ? a : b; }\n\n", ct, name, ct, ct);
    }
}

int codegen_c_file(const AstNode *program, const char *path) {
    codegen_program = program;
    stack_list_counter = 0;
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    int uses_input_int = node_uses_call_named(program, "input_int");
    compute_safe_functions(program);
    compute_pure_functions(program);
    compute_ownership(program);

    fputs(unchecked_mode ? "/* Generated by the Clair compiler (--sans-verif: no bounds checks). */\n"
                         : "/* Generated by the Clair compiler. */\n", f);
    int uses_cores = program_has_matmul(program) || program_has_matvec(program);
    if (uses_cores) fputs("#define _GNU_SOURCE   /* sched_getaffinity: how many cores the program may use */\n", f);
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
    if (uses_cores) fputs("#include <pthread.h>\n#include <sched.h>\n#include <stdatomic.h>\n#include <time.h>\n", f);
    if (node_uses_math(program) || program_uses_big(program)) fputs("#include <math.h>\n", f);
    if (node_uses_type(program, "uint1024")) fputs("#include <float.h>\n", f);
    fputc('\n', f);
    if (program_uses_big(program)) emit_big_runtime(f, program);
    MachineNeeds machine_needs;
    memset(&machine_needs, 0, sizeof(machine_needs));
    scan_machine_needs(program, &machine_needs);
    emit_machine_runtime(f, &machine_needs);

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
    if (node_prints_type(program, "float32")) {
        fputs(
            "/* a float32 with the fewest digits that read back the same number (like NumPy) */\n"
            "static void clair_rt_print_f32(float x) {\n"
            "    char text[32];\n"
            "    for (int digits = 1; digits <= 9; digits++) {\n"
            "        snprintf(text, sizeof(text), \"%.*g\", digits, (double)x);\n"
            "        if (strtof(text, NULL) == x) break;\n"
            "    }\n"
            "    puts(text);\n"
            "}\n\n",
            f
        );
    }
    if (node_uses_call_named(program, "@int") || machine_needs.float_to_int) {
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

        int uses_list_slice = node_uses_slice(program, 0);
        int uses_list_literal = node_uses_kind(program, AST_LIST) || uses_list_slice;
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
            if (uses_list_slice) {
                emit_slice_count(f);
                fputs(
                    "/* xs[start:stop:step]: a new list, like Python */\n"
                    "static ClairList clair_rt_list_slice(ClairList xs, int64_t start, int64_t stop, int64_t step, int missing) {\n"
                    "    int64_t count = clair_rt_slice_count((int64_t)xs->len, &start, stop, step, missing);\n"
                    "    size_t size = xs->elem_size;\n"
                    "    if (count == 0) return clair_rt_list_make(NULL, 0, size);\n"
                    "    if (step == 1) return clair_rt_list_make((const char *)xs->data + (size_t)start * size, (size_t)count, size);\n"
                    "    char *items = malloc((size_t)count * size);\n"
                    "    if (!items) { fputs(\"Clair runtime error: out of memory while creating list\\n\", stderr); exit(EXIT_FAILURE); }\n"
                    "    for (int64_t k = 0; k < count; k++) memcpy(items + (size_t)k * size, (const char *)xs->data + (size_t)(start + k * step) * size, size);\n"
                    "    ClairList result = clair_rt_list_make(items, (size_t)count, size);\n"
                    "    free(items);\n"
                    "    return result;\n"
                    "}\n\n",
                    f
                );
            }
            if (own_decl_count > 0) {
                fputs(
                    "/* A list nobody uses any more: freed now instead of at the end. */\n"
                    "static void clair_rt_list_free(ClairList list) {\n"
                    "    for (size_t i = clair_rt_list_count; i > 0; i--) {     /* usually the most recent */\n"
                    "        if (clair_rt_lists[i - 1] == list) {\n"
                    "            clair_rt_lists[i - 1] = NULL;\n"
                    "            while (clair_rt_list_count > 0 && !clair_rt_lists[clair_rt_list_count - 1]) clair_rt_list_count--;\n"
                    "            break;\n"
                    "        }\n"
                    "    }\n"
                    "    free(list->data);\n"
                    "    free(list);\n"
                    "}\n\n",
                    f
                );
            }
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
            for (size_t e = 0; e < ELEMENT_KIND_COUNT; e++) {
                char list_type[32];
                snprintf(list_type, sizeof(list_type), "list[%s]", element_kinds[e][0]);
                if (!node_indexes_list_type(program, list_type)) continue;
                const char *ctype = element_kinds[e][1], *suffix = element_kinds[e][2];
                if (unchecked_mode) {
                    fprintf(f,
                        "static inline %s *clair_rt_list_at_%s_checked(ClairList list, int64_t index) {\n"
                        "    return (%s *)list->data + index;  /* --sans-verif */\n"
                        "}\n\n",
                        ctype, suffix, ctype);
                    continue;
                }
                fprintf(f,
                    "static inline %s *clair_rt_list_at_%s_checked(ClairList list, int64_t index) {\n"
                    "    if (!list || index < 0 || (uint64_t)index >= list->len) clair_rt_index_error(list, index, list ? list->len : 0);\n"
                    "    return (%s *)list->data + index;\n"
                    "}\n\n",
                    ctype, suffix, ctype);
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
    int uses_string_slice = node_uses_slice(program, 1);

    if (uses_objects || uses_concat || uses_matrix || uses_string_slice) {
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
            "    for (ClairMatrix m = clair_rt_matrices, next; m; m = next) {\n"
            "        next = m->next;\n"
            "        free(m->data);\n"
            "        free(m);\n"
            "    }\n"
            "    clair_rt_matrices = NULL;\n"
            "}\n\n"
            "/* A matrix nobody uses any more: freed now instead of at the end. */\n"
            "static void clair_rt_matrix_free(ClairMatrix m) {\n"
            "    for (ClairMatrix *p = &clair_rt_matrices; *p; p = &(*p)->next) {   /* usually the first */\n"
            "        if (*p == m) {\n"
            "            *p = m->next;\n"
            "            break;\n"
            "        }\n"
            "    }\n"
            "    free(m->data);\n"
            "    free(m);\n"
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
            "    ClairMatrix m = malloc(sizeof(*m));\n"
            "    if (!m) {\n"
            "        fputs(\"Clair runtime error: out of memory while creating matrix\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
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
        for (size_t e = 0; e < ELEMENT_KIND_COUNT; e++) {
            char bracketed[32];
            snprintf(bracketed, sizeof(bracketed), "[%s]", element_kinds[e][0]);
            const char *mkinds[1][3] = { { bracketed, element_kinds[e][2], element_kinds[e][1] } };
            const size_t k = 0;
            if (!node_uses_matrix_kind(program, mkinds[k][0])) continue;
            fprintf(f,
                "static inline %s *clair_rt_matrix_at_%s_checked(ClairMatrix m, int64_t i, int64_t j) {\n"
                "%s"
                "    return (%s *)m->data + (i * m->cols + j);\n"
                "}\n\n",
                mkinds[k][2], mkinds[k][1],
                unchecked_mode ? "    /* --sans-verif: no bounds check */\n"
                               : "    if ((uint64_t)i >= (uint64_t)m->rows || (uint64_t)j >= (uint64_t)m->cols) clair_rt_matrix_index_error(m, i, j);\n",
                mkinds[k][2]);
        }
    }

    if (program_has_fast_loop(program) && !unchecked_mode) {
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
        if (program_has_nest_loop(program)) {
            fputs(
                "/* Smallest and largest c * v for v in [first, last], or false on overflow. */\n"
                "static inline bool clair_rt_term_range(int64_t c, int64_t first, int64_t last, int64_t *lo, int64_t *hi) {\n"
                "    int64_t a, b;\n"
                "    if (__builtin_mul_overflow(c, first, &a) || __builtin_mul_overflow(c, last, &b)) return false;\n"
                "    *lo = a < b ? a : b;\n"
                "    *hi = a < b ? b : a;\n"
                "    return true;\n"
                "}\n\n"
                "/* True when c0 + ck * k + cj * j stays in [0, limit) for every k in [k0, k1) and j in [j0, j1). */\n"
                "static inline bool clair_rt_affine2_in(int64_t c0, int64_t ck, int64_t k0, int64_t k1,\n"
                "                                       int64_t cj, int64_t j0, int64_t j1, int64_t limit) {\n"
                "    if (k0 >= k1 || j0 >= j1) return true;\n"
                "    int64_t klo, khi, jlo, jhi, lo, hi;\n"
                "    if (!clair_rt_term_range(ck, k0, k1 - 1, &klo, &khi) || !clair_rt_term_range(cj, j0, j1 - 1, &jlo, &jhi)) return false;\n"
                "    if (__builtin_add_overflow(klo, jlo, &lo) || __builtin_add_overflow(lo, c0, &lo)) return false;\n"
                "    if (__builtin_add_overflow(khi, jhi, &hi) || __builtin_add_overflow(hi, c0, &hi)) return false;\n"
                "    return lo >= 0 && hi < limit;\n"
                "}\n\n",
                f
            );
            if (node_uses_list(program)) {
                fputs(
                    "static inline bool clair_rt_affine2_ok(ClairList list, int64_t c0, int64_t ck, int64_t k0, int64_t k1,\n"
                    "                                       int64_t cj, int64_t j0, int64_t j1) {\n"
                    "    return list && list->len <= (size_t)INT64_MAX &&\n"
                    "           clair_rt_affine2_in(c0, ck, k0, k1, cj, j0, j1, (int64_t)list->len);\n"
                    "}\n\n",
                    f
                );
            }
        }
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

    if (program_has_matvec(program) && node_uses_list(program)) {
        fputs(
            "/* w[i * ld + j] stays inside the list for every i < rows and j < cols */\n"
            "static inline bool clair_rt_matvec_list_fits(ClairList w, int64_t ld, int64_t rows, int64_t cols) {\n"
            "    int64_t last;\n"
            "    if (rows <= 0 || cols <= 0) return true;\n"
            "    return ld >= 0 && w->len <= (size_t)INT64_MAX &&\n"
            "           !__builtin_mul_overflow(rows - 1, ld, &last) && !__builtin_add_overflow(last, cols - 1, &last) &&\n"
            "           last < (int64_t)w->len;\n"
            "}\n\n",
            f
        );
    }

    if (program_has_matmul(program) || program_has_matvec(program)) {
        fputs(
            "/*\n"
            " * Several cores: a pool of threads started the first time it is needed.  A\n"
            " * job is cut into chunks of rows; every thread (the caller too) takes the next\n"
            " * free chunk until none is left, so fast and slow cores share the work.  Each\n"
            " * row is computed by one thread exactly as in a single loop, so the result\n"
            " * does not depend on the number of threads.  CLAIR_THREADS=1 uses one core.\n"
            " *\n"
            " * The caller waits only for the chunks, never for a thread that arrives late:\n"
            " * a late thread finds the job over (its number has changed) and goes back to\n"
            " * waiting.  Between two jobs the threads wait actively for a short time, then\n"
            " * sleep; the caller makes a system call only when one of them sleeps.\n"
            " */\n"
            "typedef void (*ClairTask)(const void *job, int64_t first, int64_t last);\n"
            "\n"
            "typedef struct {\n"
            "    _Atomic(ClairTask) task;\n"
            "    _Atomic(const void *) job;\n"
            "    _Atomic int64_t count, chunk;\n"
            "} ClairPoolJob;\n"
            "\n"
            "static struct {\n"
            "    pthread_mutex_t lock;\n"
            "    pthread_cond_t wake;\n"
            "    pthread_once_t once;\n"
            "    int workers;                         /* threads besides the caller */\n"
            "    ClairPoolJob jobs[2];                /* the current job and the previous one */\n"
            "    _Atomic unsigned number;             /* number of the current job */\n"
            "    _Atomic uint64_t next;               /* (job number << 32) | first row of the next free chunk */\n"
            "    _Atomic int64_t done;                /* rows finished in the current job */\n"
            "    _Atomic int sleeping;                /* threads waiting on the condition */\n"
            "} clair_rt_pool = {\n"
            "    .lock = PTHREAD_MUTEX_INITIALIZER,\n"
            "    .wake = PTHREAD_COND_INITIALIZER,\n"
            "    .once = PTHREAD_ONCE_INIT,\n"
            "};\n"
            "\n"
            "static inline void clair_rt_pause(void) {\n"
            "#if defined(__aarch64__)\n"
            "    __asm__ __volatile__(\"yield\");\n"
            "#elif defined(__x86_64__)\n"
            "    __asm__ __volatile__(\"pause\");\n"
            "#endif\n"
            "}\n"
            "\n"
            "static inline int64_t clair_rt_now_us(void) {\n"
            "    struct timespec t;\n"
            "    clock_gettime(CLOCK_MONOTONIC, &t);\n"
            "    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;\n"
            "}\n"
            "\n",
            f
        );
        fputs(
            "/* takes chunks of job `number` until none is left or the job is over */\n"
            "static void clair_rt_pool_work(unsigned number) {\n"
            "    ClairPoolJob *job = &clair_rt_pool.jobs[number & 1];\n"
            "    ClairTask task = atomic_load_explicit(&job->task, memory_order_acquire);\n"
            "    const void *data = atomic_load_explicit(&job->job, memory_order_acquire);\n"
            "    int64_t count = atomic_load_explicit(&job->count, memory_order_acquire);\n"
            "    int64_t chunk = atomic_load_explicit(&job->chunk, memory_order_acquire);\n"
            "    uint64_t next = atomic_load_explicit(&clair_rt_pool.next, memory_order_acquire);\n"
            "    for (;;) {\n"
            "        if ((unsigned)(next >> 32) != number) return;\n"
            "        int64_t first = (int64_t)(next & 0xffffffffu);\n"
            "        if (first >= count) return;\n"
            "        int64_t last = count - first < chunk ? count : first + chunk;\n"
            "        if (!atomic_compare_exchange_weak_explicit(&clair_rt_pool.next, &next, ((uint64_t)number << 32) | (uint64_t)last,\n"
            "                                                   memory_order_acquire, memory_order_acquire)) continue;\n"
            "        task(data, first, last);\n"
            "        atomic_fetch_add_explicit(&clair_rt_pool.done, last - first, memory_order_release);\n"
            "        next = atomic_load_explicit(&clair_rt_pool.next, memory_order_acquire);\n"
            "    }\n"
            "}\n"
            "\n"
            "static void *clair_rt_pool_worker(void *unused) {\n"
            "    (void)unused;\n"
            "    unsigned seen = 0;\n"
            "    for (;;) {\n"
            "        unsigned number = atomic_load_explicit(&clair_rt_pool.number, memory_order_acquire);\n"
            "        if (number == seen) {\n"
            "            /*\n"
            "             * jobs often follow each other: wait actively about 0.2 ms, then\n"
            "             * sleep; after a few turns, leave the core to any other thread\n"
            "             */\n"
            "            int64_t until = 0;\n"
            "            for (int spins = 1; number == seen; spins++) {\n"
            "                if (spins <= 64) clair_rt_pause();\n"
            "                else {\n"
            "                    sched_yield();\n"
            "                    int64_t now = clair_rt_now_us();\n"
            "                    if (until == 0) until = now + 200;\n"
            "                    else if (now > until) break;\n"
            "                }\n"
            "                number = atomic_load_explicit(&clair_rt_pool.number, memory_order_acquire);\n"
            "            }\n"
            "            if (number == seen) {\n"
            "                pthread_mutex_lock(&clair_rt_pool.lock);\n"
            "                atomic_fetch_add(&clair_rt_pool.sleeping, 1);\n"
            "                while ((number = atomic_load(&clair_rt_pool.number)) == seen) {\n"
            "                    pthread_cond_wait(&clair_rt_pool.wake, &clair_rt_pool.lock);\n"
            "                }\n"
            "                atomic_fetch_sub(&clair_rt_pool.sleeping, 1);\n"
            "                pthread_mutex_unlock(&clair_rt_pool.lock);\n"
            "            }\n"
            "        }\n"
            "        seen = number;\n"
            "        clair_rt_pool_work(number);\n"
            "    }\n"
            "    return NULL;\n"
            "}\n",
            f
        );
        fputs(
            "static void clair_rt_pool_start(void) {\n"
            "    int cores = 1;\n"
            "    cpu_set_t allowed;\n"
            "    if (sched_getaffinity(0, sizeof(allowed), &allowed) == 0) cores = CPU_COUNT(&allowed);\n"
            "    const char *wanted = getenv(\"CLAIR_THREADS\");\n"
            "    if (wanted && atoi(wanted) > 0) cores = atoi(wanted);\n"
            "    if (cores > 16) cores = 16;\n"
            "    for (int i = 0; i < cores - 1; i++) {\n"
            "        pthread_t thread;\n"
            "        if (pthread_create(&thread, NULL, clair_rt_pool_worker, NULL) != 0) break;\n"
            "        pthread_detach(thread);\n"
            "        clair_rt_pool.workers++;\n"
            "    }\n"
            "}\n"
            "\n",
            f
        );
        fputs(
            "/* task(job, first, last) for every chunk of [0, count): on all cores when the pool has workers */\n"
            "static void clair_rt_parallel_for(ClairTask task, const void *data, int64_t count, int64_t chunk) {\n"
            "    pthread_once(&clair_rt_pool.once, clair_rt_pool_start);\n"
            "    if (clair_rt_pool.workers == 0 || count <= chunk || count > 0x7fffffff) {\n"
            "        task(data, 0, count);\n"
            "        return;\n"
            "    }\n"
            "    unsigned number = atomic_load_explicit(&clair_rt_pool.number, memory_order_relaxed) + 1;\n"
            "    ClairPoolJob *job = &clair_rt_pool.jobs[number & 1];\n"
            "    atomic_store_explicit(&job->task, task, memory_order_release);\n"
            "    atomic_store_explicit(&job->job, data, memory_order_release);\n"
            "    atomic_store_explicit(&job->count, count, memory_order_release);\n"
            "    atomic_store_explicit(&job->chunk, chunk, memory_order_release);\n"
            "    atomic_store_explicit(&clair_rt_pool.done, 0, memory_order_relaxed);\n"
            "    atomic_store_explicit(&clair_rt_pool.next, (uint64_t)number << 32, memory_order_release);\n"
            "    atomic_store(&clair_rt_pool.number, number);\n"
            "    if (atomic_load(&clair_rt_pool.sleeping) > 0) {\n"
            "        pthread_mutex_lock(&clair_rt_pool.lock);\n"
            "        pthread_cond_broadcast(&clair_rt_pool.wake);\n"
            "        pthread_mutex_unlock(&clair_rt_pool.lock);\n"
            "    }\n"
            "    clair_rt_pool_work(number);\n"
            "    /* the last chunks are being finished by other cores */\n"
            "    for (int spins = 1; atomic_load_explicit(&clair_rt_pool.done, memory_order_acquire) < count; spins++) {\n"
            "        if (spins > 64) sched_yield();\n"
            "        else clair_rt_pause();\n"
            "    }\n"
            "}\n",
            f
        );
    }

    int matvec_kinds = program_kernel_kinds(program, 0);
    for (int k = 0; k < 2; k++) {
        if (!(matvec_kinds & (1 << k))) continue;
        emit_kernel_text(f, k, (matvec_kinds & 1) != 0,
            "typedef struct {\n"
            "    int64_t n, ldw;\n"
            "    const double *w, *x;\n"
            "    double *s;\n"
            "} ClairMatvecJob;\n"
            "\n"
            "/*\n"
            " * Rows [first, last) of s[i] = s[i] + w[i][0] * x[0] + w[i][1] * x[1] + ...\n"
            " * 4 rows at once: each keeps its own additions in the same order as the\n"
            " * plain loop (identical result), and the 4 sums no longer wait for each other.\n"
            " */\n"
            "static void clair_rt_matvec_rows(const void *job, int64_t first, int64_t last) {\n"
            "    const ClairMatvecJob *p = job;\n"
            "    const int64_t n = p->n, ldw = p->ldw;\n"
            "    const double *w = p->w, *x = p->x;\n"
            "    double *s = p->s;\n"
            "    int64_t i = first;\n"
            "    for (; i + 4 <= last; i += 4) {\n"
            "        double s0 = s[i], s1 = s[i + 1], s2 = s[i + 2], s3 = s[i + 3];\n"
            "        const double *w0 = w + i * ldw, *w1 = w0 + ldw, *w2 = w1 + ldw, *w3 = w2 + ldw;\n"
            "        for (int64_t j = 0; j < n; j++) {\n"
            "            double xj = x[j];\n"
            "            s0 = s0 + w0[j] * xj;\n"
            "            s1 = s1 + w1[j] * xj;\n"
            "            s2 = s2 + w2[j] * xj;\n"
            "            s3 = s3 + w3[j] * xj;\n"
            "        }\n"
            "        s[i] = s0;\n"
            "        s[i + 1] = s1;\n"
            "        s[i + 2] = s2;\n"
            "        s[i + 3] = s3;\n"
            "    }\n"
            "    for (; i < last; i++) {\n"
            "        double si = s[i];\n"
            "        const double *wi = w + i * ldw;\n"
            "        for (int64_t j = 0; j < n; j++) si = si + wi[j] * x[j];\n"
            "        s[i] = si;\n"
            "    }\n"
            "}\n"
            "\n"
            "/* the sums of a dense layer, on all cores once the layer is big enough */\n"
            "static inline void clair_rt_matvec_f64(int64_t m, int64_t n, const double *w, int64_t ldw, const double *x,\n"
            "                                       double *s) {\n"
            "    ClairMatvecJob job = { n, ldw, w, x, s };\n"
            "    if ((double)m * (double)n < 32768.0) clair_rt_matvec_rows(&job, 0, m);\n"
            "    else clair_rt_parallel_for(clair_rt_matvec_rows, &job, m, 16);\n"
            "}\n"
            "\n"
            "/* a whole dense layer (sums and the rest of each row), on all cores once it is big enough */\n"
            "static inline void clair_rt_layer_f64(int64_t m, int64_t n, ClairTask layer, const void *job) {\n"
            "    if ((double)m * (double)n < 32768.0) layer(job, 0, m);\n"
            "    else clair_rt_parallel_for(layer, job, m, 16);\n"
            "}\n"
            "\n"
            "/* room for the sums: on the stack for a small layer (no malloc at each call) */\n"
            "#define CLAIR_SUMS_SMALL 64\n"
            "static inline double *clair_rt_sums_make(int64_t m, double *small) {\n"
            "    if (m <= CLAIR_SUMS_SMALL) return small;\n"
            "    double *s = malloc((size_t)m * sizeof(double));\n"
            "    if (!s) {\n"
            "        fputs(\"Clair runtime error: out of memory\\n\", stderr);\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    return s;\n"
            "}\n"
            "\n"
            "static inline void clair_rt_sums_free(double *s, const double *small) {\n"
            "    if (s != small) free(s);\n"
            "}\n"
        );
    }

    int matmul_kinds = program_kernel_kinds(program, 1);
    if (matmul_kinds) {
        for (int k = 0; k < 2; k++) {
            if (!(matmul_kinds & (1 << k))) continue;
            emit_kernel_text(f, k, (matmul_kinds & 1) != 0,
            "typedef struct {\n"
            "    int64_t n, kk, lda, ldb, ldc;\n"
            "    const double *a, *b;\n"
            "    double *c;\n"
            "} ClairMatmulJob;\n"
            "\n"
            "/*\n"
            " * Rows [first, last) of c[i][j] = c[i][j] + a[i][k] * b[k][j].  Every c[i][j]\n"
            " * receives the same additions in the same order (k increasing) as with the\n"
            " * three plain loops, so the result is identical to the bit; but a 4 x 8 block\n"
            " * of c stays in registers while k runs over 128 values.\n"
            " */\n"
            "static void clair_rt_matmul_rows(const void *job, int64_t first, int64_t last) {\n"
            "    const ClairMatmulJob *p = job;\n"
            "    const int64_t n = p->n, kk = p->kk, lda = p->lda, ldb = p->ldb, ldc = p->ldc;\n"
            "    const double *a = p->a, *b = p->b;\n"
            "    double *c = p->c;\n"
            "    for (int64_t k0 = 0; k0 < kk; k0 += 128) {\n"
            "        int64_t k1 = kk - k0 < 128 ? kk : k0 + 128;\n"
            "        int64_t i = first;\n"
            "        for (; i + 4 <= last; i += 4) {\n"
            "            int64_t j = 0;\n"
            "            for (; j + 8 <= n; j += 8) {\n"
            "                double acc[4][8];\n"
            "                for (int r = 0; r < 4; r++)\n"
            "                    for (int q = 0; q < 8; q++) acc[r][q] = c[(i + r) * ldc + j + q];\n"
            "                for (int64_t k = k0; k < k1; k++) {\n"
            "                    const double *bk = b + k * ldb + j;\n"
            "                    for (int r = 0; r < 4; r++) {\n"
            "                        double air = a[(i + r) * lda + k];\n"
            "                        for (int q = 0; q < 8; q++) acc[r][q] = acc[r][q] + air * bk[q];\n"
            "                    }\n"
            "                }\n"
            "                for (int r = 0; r < 4; r++)\n"
            "                    for (int q = 0; q < 8; q++) c[(i + r) * ldc + j + q] = acc[r][q];\n"
            "            }\n"
            "            for (; j < n; j++) {\n"
            "                for (int r = 0; r < 4; r++) {\n"
            "                    double s = c[(i + r) * ldc + j];\n"
            "                    for (int64_t k = k0; k < k1; k++) s = s + a[(i + r) * lda + k] * b[k * ldb + j];\n"
            "                    c[(i + r) * ldc + j] = s;\n"
            "                }\n"
            "            }\n"
            "        }\n"
            "        for (; i < last; i++) {\n"
            "            for (int64_t k = k0; k < k1; k++) {\n"
            "                double aik = a[i * lda + k];\n"
            "                for (int64_t j = 0; j < n; j++) c[i * ldc + j] = c[i * ldc + j] + aik * b[k * ldb + j];\n"
            "            }\n"
            "        }\n"
            "    }\n"
            "}\n"
            "\n"
            "/* the matrix product, on all cores once it is big enough to be worth it */\n"
            "static __attribute__((noinline)) void clair_rt_matmul_f64(int64_t m, int64_t n, int64_t kk,\n"
            "                                                        const double *a, int64_t lda, const double *b, int64_t ldb,\n"
            "                                                        double *c, int64_t ldc) {\n"
            "    ClairMatmulJob job = { n, kk, lda, ldb, ldc, a, b, c };\n"
            "    if ((double)m * (double)n * (double)kk < 1048576.0) clair_rt_matmul_rows(&job, 0, m);\n"
            "    else clair_rt_parallel_for(clair_rt_matmul_rows, &job, m, 8);\n"
            "}\n"
            );
        }
        if (node_uses_matrix_kind(program, "[float]") || node_uses_matrix_kind(program, "[float32]")) {
            fputs(
                "/* The kernel may replace the loops: shapes large enough, c distinct from a and b. */\n"
                "static inline bool clair_rt_matmul_ok(ClairMatrix a, ClairMatrix b, ClairMatrix c,\n"
                "                                      int64_t m, int64_t n, int64_t k) {\n"
                "    return m > 0 && n > 0 && k > 0 &&\n"
                "           m <= a->rows && k <= a->cols && k <= b->rows && n <= b->cols &&\n"
                "           m <= c->rows && n <= c->cols && c->data != a->data && c->data != b->data;\n"
                "}\n\n",
                f
            );
        }
        if (node_uses_list(program)) {
            fputs(
                "/* Same for flat lists x[i * ld + j]: rows of c must not overlap. */\n"
                "static inline bool clair_rt_matmul_list_fits(ClairList x, int64_t ld, int64_t rows, int64_t cols) {\n"
                "    int64_t last;\n"
                "    return ld >= cols && x->len <= (size_t)INT64_MAX &&\n"
                "           !__builtin_mul_overflow(rows - 1, ld, &last) && !__builtin_add_overflow(last, cols - 1, &last) &&\n"
                "           last < (int64_t)x->len;\n"
                "}\n\n"
                "static inline bool clair_rt_matmul_list_ok(ClairList a, int64_t lda, ClairList b, int64_t ldb,\n"
                "                                           ClairList c, int64_t ldc, int64_t m, int64_t n, int64_t k) {\n"
                "    return m > 0 && n > 0 && k > 0 &&\n"
                "           clair_rt_matmul_list_fits(a, lda, m, k) && clair_rt_matmul_list_fits(b, ldb, k, n) &&\n"
                "           clair_rt_matmul_list_fits(c, ldc, m, n) && c->data != a->data && c->data != b->data;\n"
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

    if (uses_string_slice) {
        if (!node_uses_slice(program, 0)) emit_slice_count(f);
        fputs(
            "/* s[start:stop:step]: a new string, like Python (counted in bytes, like len) */\n"
            "static const char *clair_rt_string_slice(const char *s, int64_t start, int64_t stop, int64_t step, int missing) {\n"
            "    int64_t count = clair_rt_slice_count((int64_t)strlen(s), &start, stop, step, missing);\n"
            "    char *result = clair_rt_arena_alloc((size_t)count + 1);\n"
            "    for (int64_t k = 0; k < count; k++) result[k] = s[start + k * step];\n"
            "    result[count] = '\\0';\n"
            "    return result;\n"
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
        /* the function is written in memory: the jobs of its dense layers go before it */
        char *text = NULL, *helpers = NULL;
        size_t text_len = 0, helpers_len = 0;
        FILE *fn_out = open_memstream(&text, &text_len);
        layer_helpers = open_memstream(&helpers, &helpers_len);
        layer_fn_name = fn->text;
        layer_count = 0;
        int ok = fn_out && layer_helpers && emit_function_signature(fn_out, fn, 0);
        if (ok) {
            fputs(" {\n", fn_out);
            current_return_type = fn->type_name;
            owned_live_count = 0;
            loop_mark_count = 0;
            ok = emit_block(fn_out, function_body(fn), 1);
            fputs("}\n\n", fn_out);
        }
        if (fn_out) fclose(fn_out);
        if (layer_helpers) fclose(layer_helpers);
        layer_helpers = NULL;
        if (ok) {
            fwrite(helpers, 1, helpers_len, f);
            fwrite(text, 1, text_len, f);
        }
        free(text);
        free(helpers);
        if (!ok) {
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    return 1;
}
