#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    const char *type;
    size_t depth;
} VarSymbol;

typedef struct {
    AstNode *program;
    VarSymbol *vars;
    size_t var_count;
    size_t var_capacity;
    size_t depth;
    const char *return_type;
    int had_error;
} Semantic;

static int set_type(AstNode *node, const char *type) {
    return ast_set_type(node, type, strlen(type));
}

/*
 * Machine integers: int (64 bits, signed), i8 i16 i32, u8 u16 u32 u64.  Their
 * bits, 0 for any other type.  Their arithmetic wraps around like in C.
 */
static int machine_bits(const char *t) {
    if (!t) return 0;
    if (strcmp(t, "int") == 0 || strcmp(t, "u64") == 0) return 64;
    if (strcmp(t, "i8") == 0 || strcmp(t, "u8") == 0) return 8;
    if (strcmp(t, "i16") == 0 || strcmp(t, "u16") == 0) return 16;
    if (strcmp(t, "i32") == 0 || strcmp(t, "u32") == 0) return 32;
    return 0;
}

static int machine_unsigned(const char *t) { return machine_bits(t) && t[0] == 'u'; }

/* i8 .. u64 but not int */
static int is_small_machine(const char *t) { return machine_bits(t) && strcmp(t, "int") != 0; }

/* src converts to dst without losing anything (u8 -> u16, u8 -> i16, i32 -> int) */
static int machine_widens(const char *src, const char *dst) {
    int bs = machine_bits(src), bd = machine_bits(dst);
    if (!bs || !bd) return 0;
    if (strcmp(src, dst) == 0) return 1;
    if (machine_unsigned(src) == machine_unsigned(dst)) return bd > bs;
    return !machine_unsigned(dst) && bd > bs;
}

/* where two machine integers meet: the one the other widens into, or NULL (u32 and i32) */
static const char *machine_meet(const char *left, const char *right) {
    if (machine_widens(left, right)) return right;
    if (machine_widens(right, left)) return left;
    return NULL;
}

/* an index, a size or a range bound: int or a machine integer that fits in it */
static int is_index_type(const char *t) { return machine_widens(t, "int"); }

static int is_numeric(const char *type) {
    return type && (machine_bits(type) || strcmp(type, "float") == 0 || strcmp(type, "float32") == 0);
}

static int is_float_type(const char *type) {
    return type && (strcmp(type, "float") == 0 || strcmp(type, "float32") == 0);
}

/*
 * Type of a calculation between two numbers: float (64 bits) wins, then
 * float32, then int.  float32 with an int stays float32 (like NumPy).
 */
static const char *numeric_result(const char *left, const char *right) {
    if (strcmp(left, "float") == 0 || strcmp(right, "float") == 0) return "float";
    if (strcmp(left, "float32") == 0 || strcmp(right, "float32") == 0) {
        /* i8, u8, i16, u16 and int fit with a float32; i32, u32, u64 go to float (like NumPy) */
        const char *other = strcmp(left, "float32") == 0 ? right : left;
        if (strcmp(other, "float32") == 0 || strcmp(other, "int") == 0 || machine_bits(other) <= 16) return "float32";
        return "float";
    }
    return machine_meet(left, right);   /* NULL: u32 with i32, u64 with int... */
}

static void mix_error(Semantic *s, const char *what, const char *left, const char *right) {
    fprintf(stderr, "semantic error: %s cannot mix %s and %s: convert one of them first, e.g. %s(x)\n",
            what, left, right, machine_bits(left) >= machine_bits(right) && !machine_unsigned(left) ? left : "int");
    s->had_error = 1;
}

/*
 * Big integers: int128 .. int1024 (signed) and uint128 .. uint1024 (0 and
 * above): their bits, 0 for any other type.
 */
static int big_bits(const char *type) {
    if (!type) return 0;
    if (type[0] == 'u') type++;
    if (strcmp(type, "int128") == 0) return 128;
    if (strcmp(type, "int256") == 0) return 256;
    if (strcmp(type, "int512") == 0) return 512;
    if (strcmp(type, "int1024") == 0) return 1024;
    return 0;
}

static int is_big(const char *type) { return big_bits(type) != 0; }

static int is_ubig(const char *type) { return is_big(type) && type[0] == 'u'; }

/* int counts as a 64-bit integer when big integers are mixed */
static int int_bits(const char *type) {
    if (type && strcmp(type, "int") == 0) return 64;
    return big_bits(type);
}

/* what a list can hold, and the matching list types */
static const char *const list_elements[][2] = {
    { "int", "list[int]" }, { "float", "list[float]" }, { "float32", "list[float32]" },
    { "bool", "list[bool]" }, { "string", "list[string]" },
    { "i8", "list[i8]" }, { "i16", "list[i16]" }, { "i32", "list[i32]" },
    { "u8", "list[u8]" }, { "u16", "list[u16]" }, { "u32", "list[u32]" }, { "u64", "list[u64]" },
};

static const char *list_element_type(const char *type) {
    if (!type) return NULL;
    for (size_t i = 0; i < sizeof(list_elements) / sizeof(list_elements[0]); i++) {
        if (strcmp(type, list_elements[i][1]) == 0) return list_elements[i][0];
    }
    return NULL;
}

static int is_list_type(const char *type) { return list_element_type(type) != NULL; }

static const char *list_type_for_element(const char *type) {
    if (!type) return NULL;
    for (size_t i = 0; i < sizeof(list_elements) / sizeof(list_elements[0]); i++) {
        if (strcmp(type, list_elements[i][0]) == 0) return list_elements[i][1];
    }
    return NULL;
}

/* what a matrix can hold: numbers */
static const char *matrix_element_of(const char *type, const char *prefix) {
    static const char *const elements[] = { "float", "float32", "int", "i8", "i16", "i32", "u8", "u16", "u32", "u64", NULL };
    size_t len = strlen(prefix);
    if (!type || strncmp(type, prefix, len) != 0) return NULL;
    for (size_t i = 0; elements[i]; i++) {
        size_t n = strlen(elements[i]);
        if (strncmp(type + len, elements[i], n) == 0 && strcmp(type + len + n, "]") == 0) return elements[i];
    }
    return NULL;
}

static int is_matrix_type(const char *type) { return matrix_element_of(type, "matrix[") != NULL; }

/* m[i] alone is only an intermediate step of m[i][j] */
static int is_matrix_row_type(const char *type) {
    return type && strncmp(type, "matrix_row[", 11) == 0;
}

static const char *matrix_element_type(const char *type) {
    const char *e = matrix_element_of(type, "matrix[");
    return e ? e : matrix_element_of(type, "matrix_row[");
}

static int is_dict_type(const char *type) {
    return type && (
        strcmp(type, "dict[string,int]") == 0 ||
        strcmp(type, "dict[string,float]") == 0 ||
        strcmp(type, "dict[string,bool]") == 0 ||
        strcmp(type, "dict[string,string]") == 0
    );
}

static const char *dict_value_type(const char *type) {
    if (!type) return NULL;
    if (strcmp(type, "dict[string,int]") == 0) return "int";
    if (strcmp(type, "dict[string,float]") == 0) return "float";
    if (strcmp(type, "dict[string,bool]") == 0) return "bool";
    if (strcmp(type, "dict[string,string]") == 0) return "string";
    return NULL;
}

static const char *dict_type_for_value(const char *type) {
    if (!type) return NULL;
    if (strcmp(type, "int") == 0) return "dict[string,int]";
    if (strcmp(type, "float") == 0) return "dict[string,float]";
    if (strcmp(type, "bool") == 0) return "dict[string,bool]";
    if (strcmp(type, "string") == 0) return "dict[string,string]";
    return NULL;
}


/* int -> float, int -> float32, float32 -> float, and float -> float32 (rounded, like NumPy) */
static int assignable(const char *dst, const char *src) {
    if (!dst || !src) return 0;
    if (strcmp(dst, src) == 0) return 1;
    if (machine_widens(src, dst)) return 1;
    return is_float_type(dst) && is_numeric(src);
}

static void error_type(Semantic *s, const char *message) {
    fprintf(stderr, "semantic error: %s\n", message);
    s->had_error = 1;
}

static const AstNode *find_function(const Semantic *s, const char *name) {
    for (size_t i = 0; i < s->program->child_count; i++) {
        const AstNode *fn = s->program->children[i];
        if (fn->kind == AST_FUNCTION && fn->text && strcmp(fn->text, name) == 0) return fn;
    }
    return NULL;
}

static const AstNode *find_class(const Semantic *s, const char *name) {
    for (size_t i = 0; i < s->program->child_count; i++) {
        const AstNode *cls = s->program->children[i];
        if (cls->kind == AST_CLASS && cls->text && strcmp(cls->text, name) == 0) return cls;
    }
    return NULL;
}

static const AstNode *find_field(const AstNode *cls, const char *name) {
    if (!cls || cls->kind != AST_CLASS) return NULL;
    for (size_t i = 0; i < cls->child_count; i++) {
        const AstNode *field = cls->children[i];
        if (field->kind == AST_FIELD && field->text && strcmp(field->text, name) == 0) return field;
    }
    return NULL;
}

static int is_known_type(const Semantic *s, const char *type) {
    if (!type) return 0;
    if (strcmp(type, "bigint") == 0 || strcmp(type, "bigfloat") == 0) return 1;
    if (is_big(type) || machine_bits(type)) return 1;
    if (strcmp(type, "int") == 0 || strcmp(type, "float") == 0 || strcmp(type, "float32") == 0 ||
        strcmp(type, "bool") == 0 || strcmp(type, "string") == 0 ||
        (is_list_type(type) || is_dict_type(type) || is_matrix_type(type))) {
        return 1;
    }
    return find_class(s, type) != NULL;
}

static const char *find_var(const Semantic *s, const char *name) {
    for (size_t i = s->var_count; i > 0; i--) {
        if (strcmp(s->vars[i - 1].name, name) == 0) return s->vars[i - 1].type;
    }
    return NULL;
}

static int add_var(Semantic *s, const char *name, const char *type) {
    for (size_t i = s->var_count; i > 0; i--) {
        const VarSymbol *v = &s->vars[i - 1];
        if (v->depth != s->depth) break;
        if (strcmp(v->name, name) == 0) {
            fprintf(stderr, "semantic error: variable '%s' is already declared in this scope\n", name);
            s->had_error = 1;
            return 0;
        }
    }

    if (s->var_count == s->var_capacity) {
        size_t cap = s->var_capacity ? s->var_capacity * 2 : 16;
        VarSymbol *vars = realloc(s->vars, cap * sizeof(*vars));
        if (!vars) return 0;
        s->vars = vars;
        s->var_capacity = cap;
    }

    char *copy = malloc(strlen(name) + 1);
    if (!copy) return 0;
    strcpy(copy, name);
    s->vars[s->var_count++] = (VarSymbol){ copy, type, s->depth };
    return 1;
}

static void enter_scope(Semantic *s) { s->depth++; }

static void leave_scope(Semantic *s) {
    while (s->var_count > 0 && s->vars[s->var_count - 1].depth == s->depth) {
        free(s->vars[s->var_count - 1].name);
        s->var_count--;
    }
    if (s->depth > 0) s->depth--;
}

static const char *expr_type(Semantic *s, AstNode *n);
static const char *expr_type_expected(Semantic *s, AstNode *n, const char *expected);

static const char *check_list_literal_as(Semantic *s, AstNode *n, const char *list_type) {
    const char *element_type = list_element_type(list_type);
    if (!element_type) return NULL;

    for (size_t i = 0; i < n->child_count; i++) {
        const char *item = expr_type_expected(s, n->children[i], element_type);
        if (!item || !assignable(element_type, item)) {
            fprintf(stderr, "semantic error: %s literal expects %s elements, got %s\n",
                    list_type, element_type, item ? item : "invalid");
            s->had_error = 1;
            return NULL;
        }
    }

    set_type(n, list_type);
    return n->type_name;
}

static const char *infer_list_literal(Semantic *s, AstNode *n) {
    if (n->child_count == 0) {
        error_type(s, "empty list literal requires an expected list type");
        return NULL;
    }

    const char *element = expr_type(s, n->children[0]);
    if (!element || is_list_type(element) || !list_type_for_element(element)) {
        fprintf(stderr, "semantic error: unsupported list element type %s\n",
                element ? element : "invalid");
        s->had_error = 1;
        return NULL;
    }

    for (size_t i = 1; i < n->child_count; i++) {
        const char *item = expr_type(s, n->children[i]);
        if (!item) return NULL;

        if (is_numeric(element) && is_numeric(item)) {
            /* [1, 2.5] is a list[float], [b, 300] with b a u8 a list[int] */
            const char *meet = numeric_result(element, item);
            if (!meet) {
                mix_error(s, "a list literal", element, item);
                return NULL;
            }
            element = meet;
            continue;
        }

        if (strcmp(item, element) != 0) {
            fprintf(stderr, "semantic error: list literal mixes incompatible element types %s and %s\n",
                    element, item);
            s->had_error = 1;
            return NULL;
        }
    }

    const char *list_type = list_type_for_element(element);
    if (!list_type) return NULL;
    set_type(n, list_type);
    return n->type_name;
}

static const char *check_dict_literal_as(Semantic *s, AstNode *n, const char *dict_type) {
    const char *value_type = dict_value_type(dict_type);
    if (!value_type || (n->child_count % 2) != 0) return NULL;

    for (size_t i = 0; i < n->child_count; i += 2) {
        const char *key = expr_type(s, n->children[i]);
        if (!key || strcmp(key, "string") != 0) {
            fprintf(stderr, "semantic error: dictionary literal keys must be string, got %s\n",
                    key ? key : "invalid");
            s->had_error = 1;
            return NULL;
        }

        const char *value = expr_type_expected(s, n->children[i + 1], value_type);
        if (!value || !assignable(value_type, value)) {
            fprintf(stderr, "semantic error: %s literal expects %s values, got %s\n",
                    dict_type, value_type, value ? value : "invalid");
            s->had_error = 1;
            return NULL;
        }
    }

    set_type(n, dict_type);
    return n->type_name;
}

static const char *infer_dict_literal(Semantic *s, AstNode *n) {
    if (n->child_count == 0) {
        error_type(s, "empty dictionary literal requires an expected dictionary type");
        return NULL;
    }
    if ((n->child_count % 2) != 0) {
        error_type(s, "invalid dictionary literal");
        return NULL;
    }

    const char *first_key = expr_type(s, n->children[0]);
    if (!first_key || strcmp(first_key, "string") != 0) {
        fprintf(stderr, "semantic error: dictionary literal keys must be string, got %s\n",
                first_key ? first_key : "invalid");
        s->had_error = 1;
        return NULL;
    }

    const char *value_type = expr_type(s, n->children[1]);
    if (!value_type || is_list_type(value_type) || is_dict_type(value_type) ||
        !dict_type_for_value(value_type)) {
        fprintf(stderr, "semantic error: unsupported dictionary value type %s\n",
                value_type ? value_type : "invalid");
        s->had_error = 1;
        return NULL;
    }

    int numeric_float = strcmp(value_type, "float") == 0;
    for (size_t i = 2; i < n->child_count; i += 2) {
        const char *key = expr_type(s, n->children[i]);
        if (!key || strcmp(key, "string") != 0) {
            fprintf(stderr, "semantic error: dictionary literal keys must be string, got %s\n",
                    key ? key : "invalid");
            s->had_error = 1;
            return NULL;
        }

        const char *value = expr_type(s, n->children[i + 1]);
        if (!value) return NULL;

        if (is_numeric(value_type) && is_numeric(value)) {
            if (strcmp(value, "float") == 0) numeric_float = 1;
            continue;
        }

        if (strcmp(value, value_type) != 0) {
            fprintf(stderr,
                    "semantic error: dictionary literal mixes incompatible value types %s and %s\n",
                    value_type, value);
            s->had_error = 1;
            return NULL;
        }
    }

    if (is_numeric(value_type) && numeric_float) value_type = "float";
    const char *dict_type = dict_type_for_value(value_type);
    if (!dict_type) return NULL;
    set_type(n, dict_type);
    return n->type_name;
}


/*
 * Built-in math functions and conversions.  A user function with the same
 * name wins (checked by the caller first), as in Python where a def shadows
 * a builtin.  Accepted calls are renamed "@name" so code generation can tell
 * them apart from user functions.
 *
 * Returns 1 when `n` is a valid builtin call, -1 when it is a builtin used
 * wrongly (error already reported), 0 when the name is not a builtin.
 */
static int check_matrix_builtin(Semantic *s, AstNode *n) {
    const char *name = n->text;
    if (strcmp(name, "matrix") == 0) {
        error_type(s, "matrix() needs its type from context, e.g. let m: matrix[float] = matrix(2, 3)");
        return -1;
    }
    if (strcmp(name, "rows") != 0 && strcmp(name, "cols") != 0) return 0;
    if (n->child_count != 1) {
        fprintf(stderr, "semantic error: %s() expects 1 argument, got %zu\n", name, n->child_count);
        s->had_error = 1;
        return -1;
    }
    const char *t = expr_type(s, n->children[0]);
    if (!t) return -1;
    if (!is_matrix_type(t)) {
        fprintf(stderr, "semantic error: %s() expects a matrix, got %s\n", name, t);
        s->had_error = 1;
        return -1;
    }
    char *renamed = malloc(strlen(name) + 2);
    if (!renamed) return -1;
    renamed[0] = '@';
    strcpy(renamed + 1, name);
    free(n->text);
    n->text = renamed;
    set_type(n, "int");
    return 1;
}

static int is_int_literal(const AstNode *n);
static const char *type_int_literal(Semantic *s, AstNode *n, const char *type);
static const char *machine_literal(Semantic *s, AstNode *n, const char *type);

/* 0.5 or -0.5 written directly in the program */
static int is_float_literal(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_FLOAT) return 1;
    return n->kind == AST_UNARY && n->text && strcmp(n->text, "-") == 0 && n->child_count == 1 &&
           n->children[0]->kind == AST_FLOAT;
}

/* next to a float32, a number written in the program is a float32 (like a Python number with NumPy) */
static int set_float32_literal(AstNode *n) {
    if (n->kind == AST_UNARY && !set_type(n->children[0], "float32")) return 0;
    return set_type(n, "float32");
}
static const char *big_common_type(Semantic *s, AstNode *n, const char *what, const char *left, const char *right);

/*
 * Random numbers, like Python's random module (same draws for the same seed):
 *   seed(n), random(), randint(a, b), uniform(a, b), gauss(mu, sigma),
 *   shuffle(xs), choice(xs).
 * 1 when n is one of them (renamed @random...), 0 otherwise, -1 on error.
 */
static int check_random_call(Semantic *s, AstNode *n) {
    static const struct { const char *name; size_t args; const char *kind; } calls[] = {
        { "seed", 1, "int" }, { "random", 0, "" }, { "randint", 2, "int" }, { "uniform", 2, "number" },
        { "gauss", 2, "number" }, { "shuffle", 1, "list" }, { "choice", 1, "list" },
    };
    size_t c = 0, count = sizeof(calls) / sizeof(calls[0]);
    while (c < count && strcmp(n->text, calls[c].name) != 0) c++;
    if (c == count) return 0;
    if (n->child_count != calls[c].args) {
        fprintf(stderr, "semantic error: %s() expects %zu argument(s), got %zu\n", calls[c].name, calls[c].args,
                n->child_count);
        s->had_error = 1;
        return -1;
    }
    const char *result = "float", *element = NULL;
    for (size_t i = 0; i < n->child_count; i++) {
        const char *t = expr_type(s, n->children[i]);
        if (!t) return -1;
        int ok = strcmp(calls[c].kind, "int") == 0 ? is_index_type(t)
               : strcmp(calls[c].kind, "number") == 0 ? is_numeric(t) && !is_big(t)
               : is_list_type(t);
        if (!ok) {
            fprintf(stderr, "semantic error: argument %zu of %s() expects %s, got %s\n", i + 1, calls[c].name,
                    strcmp(calls[c].kind, "list") == 0 ? "a list" : strcmp(calls[c].kind, "int") == 0 ? "an int"
                                                                                                    : "a number", t);
            s->had_error = 1;
            return -1;
        }
        if (strcmp(calls[c].kind, "list") == 0) element = list_element_type(t);
    }
    if (strcmp(calls[c].name, "seed") == 0 || strcmp(calls[c].name, "randint") == 0 ||
        strcmp(calls[c].name, "shuffle") == 0) result = "int";
    if (strcmp(calls[c].name, "choice") == 0) result = element;
    char *renamed = malloc(strlen(calls[c].name) + 2);
    if (!renamed) return -1;
    renamed[0] = '@';
    strcpy(renamed + 1, calls[c].name);
    free(n->text);
    n->text = renamed;
    return set_type(n, result) ? 1 : -1;
}

/*
 * Files and text:
 *   load_csv(path) -> matrix[float], load_csv(path, "float32" / "int") -> matrix[...]
 *   save_csv(path, m), read_lines(path) -> list[string],
 *   split(s) / split(s, sep) -> list[string], strip(s) -> string.
 * 1 when n is one of them (renamed @load_csv...), 0 otherwise, -1 on error.
 */
static int check_text_call(Semantic *s, AstNode *n) {
    static const char *const names[] = { "load_csv", "save_csv", "read_lines", "split", "strip", NULL };
    size_t c = 0;
    while (names[c] && strcmp(n->text, names[c]) != 0) c++;
    if (!names[c]) return 0;
    const char *name = names[c], *result = NULL;
    size_t min = 1, max = 1;
    if (strcmp(name, "load_csv") == 0 || strcmp(name, "split") == 0) max = 2;
    if (strcmp(name, "save_csv") == 0) min = max = 2;
    if (n->child_count < min || n->child_count > max) {
        if (min == max) fprintf(stderr, "semantic error: %s() expects %zu argument(s), got %zu\n", name, min, n->child_count);
        else fprintf(stderr, "semantic error: %s() expects %zu or %zu arguments, got %zu\n", name, min, max, n->child_count);
        s->had_error = 1;
        return -1;
    }
    const char *t0 = expr_type(s, n->children[0]);
    if (!t0) return -1;
    if (strcmp(t0, "string") != 0) {
        fprintf(stderr, "semantic error: argument 1 of %s() expects a string, got %s\n", name, t0);
        s->had_error = 1;
        return -1;
    }
    if (strcmp(name, "load_csv") == 0) {
        result = "matrix[float]";
        if (n->child_count == 2) {
            const AstNode *kind = n->children[1];
            const char *text = kind->kind == AST_STRING ? kind->text : NULL;
            if (text && strcmp(text, "float") == 0) result = "matrix[float]";
            else if (text && strcmp(text, "float32") == 0) result = "matrix[float32]";
            else if (text && strcmp(text, "int") == 0) result = "matrix[int]";
            else {
                fprintf(stderr, "semantic error: argument 2 of load_csv() must be written \"float\", \"float32\" or \"int\"\n");
                s->had_error = 1;
                return -1;
            }
            if (!expr_type(s, n->children[1])) return -1;
        }
    } else if (strcmp(name, "save_csv") == 0) {
        const char *t1 = expr_type(s, n->children[1]);
        if (!t1) return -1;
        if (strcmp(t1, "matrix[float]") != 0 && strcmp(t1, "matrix[float32]") != 0 && strcmp(t1, "matrix[int]") != 0) {
            fprintf(stderr, "semantic error: argument 2 of save_csv() expects matrix[float], matrix[float32] or "
                            "matrix[int], got %s\n", t1);
            s->had_error = 1;
            return -1;
        }
        result = "int";
    } else if (strcmp(name, "split") == 0) {
        if (n->child_count == 2) {
            const char *t1 = expr_type(s, n->children[1]);
            if (!t1) return -1;
            if (strcmp(t1, "string") != 0) {
                fprintf(stderr, "semantic error: argument 2 of split() expects a string, got %s\n", t1);
                s->had_error = 1;
                return -1;
            }
        }
        result = "list[string]";
    } else {
        result = strcmp(name, "read_lines") == 0 ? "list[string]" : "string";
    }
    char *renamed = malloc(strlen(name) + 2);
    if (!renamed) return -1;
    renamed[0] = '@';
    strcpy(renamed + 1, name);
    free(n->text);
    n->text = renamed;
    return set_type(n, result) ? 1 : -1;
}

/*
 * [expr for v in xs if cond] becomes a call to a small function written here:
 *
 *   def comprehension_1(captured variables...) -> list[T] {
 *       let @result: list[T] = []          (clair_h_result in C)
 *       for v in xs { if cond { @result.append(expr) } }
 *       return @result
 *   }
 *
 * The variables of the enclosing function that the expression reads are
 * passed as arguments (their value at that moment, like Python).
 */
static int comprehension_count = 0;

static AstNode *new_node(AstKind kind, const char *text, const char *type) {
    AstNode *n = ast_new(kind, text, text ? strlen(text) : 0);
    if (n && type && !ast_set_type(n, type, strlen(type))) { ast_free(n); return NULL; }
    return n;
}

static void collect_captures(Semantic *s, const AstNode *n, const char *var, const char **names, const char **types,
                             size_t *count) {
    if (!n) return;
    if (n->kind == AST_IDENTIFIER && n->text && strcmp(n->text, var) != 0) {
        const char *type = find_var(s, n->text);
        size_t k = 0;
        while (k < *count && strcmp(names[k], n->text) != 0) k++;
        if (type && k == *count && *count < 64) {
            names[*count] = n->text;
            types[(*count)++] = type;
        }
    }
    for (size_t i = 0; i < n->child_count; i++) collect_captures(s, n->children[i], var, names, types, count);
}

static const char *check_comprehension(Semantic *s, AstNode *n) {
    if (n->child_count < 3 || n->child_count > 4) return NULL;
    const char *var = n->children[0]->text;
    AstNode *expr = n->children[1], *iter = n->children[2], *cond = n->child_count == 4 ? n->children[3] : NULL;
    int is_range = iter->kind == AST_CALL && iter->text && strcmp(iter->text, "range") == 0 && !find_function(s, "range");
    /* untyped copies for the function, the originals only give the types here */
    AstNode *body_expr = ast_clone(expr), *body_iter = ast_clone(iter), *body_cond = cond ? ast_clone(cond) : NULL;
    if (!body_expr || !body_iter || (cond && !body_cond)) return NULL;

    const char *element = "int";
    if (is_range) {
        if (iter->child_count < 1 || iter->child_count > 3) {
            error_type(s, "range() expects 1 to 3 arguments");
            return NULL;
        }
        for (size_t i = 0; i < iter->child_count; i++) {
            const char *t = expr_type(s, iter->children[i]);
            if (!t) return NULL;
            if (!is_index_type(t)) {
                fprintf(stderr, "semantic error: range() expects int bounds, got %s\n", t);
                s->had_error = 1;
                return NULL;
            }
        }
    } else {
        const char *t = expr_type(s, iter);
        if (!t) return NULL;
        element = is_dict_type(t) ? "string" : list_element_type(t);
        if (!element) {
            fprintf(stderr, "semantic error: [... for %s in ...] needs a list, a dictionary or range(), got %s\n", var, t);
            s->had_error = 1;
            return NULL;
        }
    }
    enter_scope(s);
    const char *et = add_var(s, var, element) ? expr_type(s, expr) : NULL;
    const char *ct = et && cond ? expr_type(s, cond) : "bool";
    leave_scope(s);
    if (!et || !ct) return NULL;
    if (strcmp(ct, "bool") != 0) {
        fprintf(stderr, "semantic error: the condition of [... if ...] must be a bool, got %s\n", ct);
        s->had_error = 1;
        return NULL;
    }
    char list_type[64];
    snprintf(list_type, sizeof(list_type), "list[%s]", et);
    if (!is_list_type(list_type)) {
        fprintf(stderr, "semantic error: [... for ...] makes a list of numbers, bool or strings, got %s\n", et);
        s->had_error = 1;
        return NULL;
    }

    /* the variables read inside, other than v */
    const char *names[64], *types[64];
    size_t count = 0;
    collect_captures(s, body_expr, var, names, types, &count);
    collect_captures(s, body_iter, "", names, types, &count);
    if (body_cond) collect_captures(s, body_cond, var, names, types, &count);

    char fn_name[64];
    do snprintf(fn_name, sizeof(fn_name), "comprehension_%d", ++comprehension_count);
    while (find_function(s, fn_name) || find_class(s, fn_name));
    AstNode *fn = new_node(AST_FUNCTION, fn_name, list_type);
    AstNode *body = new_node(AST_BLOCK, NULL, NULL);
    AstNode *let = new_node(AST_LET, "@result", list_type);
    AstNode *empty = new_node(AST_LIST, NULL, NULL);
    AstNode *loop = new_node(is_range ? AST_FOR : AST_FOR_EACH, var, NULL);
    AstNode *loop_body = new_node(AST_BLOCK, NULL, NULL);
    AstNode *append = new_node(AST_LIST_APPEND, "@result", NULL);
    AstNode *ret = new_node(AST_RETURN, NULL, NULL);
    AstNode *result = new_node(AST_IDENTIFIER, "@result", NULL);
    if (!fn || !body || !let || !empty || !loop || !loop_body || !append || !ret || !result) return NULL;
    for (size_t k = 0; k < count; k++) {
        AstNode *param = new_node(AST_PARAM, names[k], types[k]);
        if (!param || !ast_add_child(fn, param)) return NULL;
    }
    ast_add_child(let, empty);
    ast_add_child(append, body_expr);
    if (body_cond) {
        AstNode *test = new_node(AST_IF, NULL, NULL), *then = new_node(AST_BLOCK, NULL, NULL);
        if (!test || !then) return NULL;
        ast_add_child(test, body_cond);
        ast_add_child(then, append);
        ast_add_child(test, then);
        ast_add_child(loop_body, test);
    } else {
        ast_add_child(loop_body, append);
    }
    if (is_range) {
        for (size_t i = 0; i < body_iter->child_count; i++) {
            ast_add_child(loop, body_iter->children[i]);
            body_iter->children[i] = NULL;
        }
        body_iter->child_count = 0;
        ast_free(body_iter);
    } else {
        ast_add_child(loop, body_iter);
    }
    ast_add_child(loop, loop_body);
    ast_add_child(ret, result);
    ast_add_child(body, let);
    ast_add_child(body, loop);
    ast_add_child(body, ret);
    ast_add_child(fn, body);
    if (!ast_add_child(s->program, fn)) return NULL;        /* checked after the functions already there */

    /* the expression becomes comprehension_1(captured variables...) */
    for (size_t i = 0; i < n->child_count; i++) ast_free(n->children[i]);
    n->child_count = 0;
    free(n->text);
    n->text = malloc(strlen(fn_name) + 1);
    if (!n->text) return NULL;
    strcpy(n->text, fn_name);
    for (size_t k = 0; k < count; k++) {
        AstNode *arg = new_node(AST_IDENTIFIER, names[k], NULL);
        if (!arg || !ast_add_child(n, arg)) return NULL;
    }
    return expr_type(s, n);
}

static int check_builtin_call(Semantic *s, AstNode *n) {
    static const char *const float_unary[] = {
        "sqrt", "exp", "log", "log10", "log2", "sin", "cos", "tan", "asin", "acos", "atan",
        "sinh", "cosh", "tanh", "floor", "ceil", NULL
    };
    static const char *const float_binary[] = { "pow", "atan2", "hypot", NULL };

    const char *name = n->text;

    /* int128(x) ... int1024(x): from int or another big integer, checked when narrowing */
    if (is_big(name)) {
        if (n->child_count != 1) {
            fprintf(stderr, "semantic error: %s() expects 1 argument(s), got %zu\n", name, n->child_count);
            s->had_error = 1;
            return -1;
        }
        AstNode *arg = n->children[0];
        const char *t = is_int_literal(arg) ? type_int_literal(s, arg, name) : expr_type(s, arg);
        if (!t) return -1;
        if (!int_bits(t)) {
            fprintf(stderr, "semantic error: argument 1 of %s() expects an integer, got %s\n", name, t);
            s->had_error = 1;
            return -1;
        }
        char *type = malloc(strlen(name) + 1);
        char *renamed = malloc(sizeof("@convert"));
        if (!type || !renamed) { free(type); free(renamed); return -1; }
        strcpy(type, name);
        strcpy(renamed, "@convert");
        free(n->text);
        n->text = renamed;
        int ok = set_type(n, type);
        free(type);
        return ok ? 1 : -1;
    }

    size_t want = 0;
    int kind = 0; /* 1 float unary, 2 float binary, 3 abs, 4 min/max, 5 int(), 6 float(), 7 float32() */
    for (size_t i = 0; float_unary[i]; i++) if (strcmp(name, float_unary[i]) == 0) { kind = 1; want = 1; }
    for (size_t i = 0; float_binary[i]; i++) if (strcmp(name, float_binary[i]) == 0) { kind = 2; want = 2; }
    if (strcmp(name, "abs") == 0) { kind = 3; want = 1; }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) { kind = 4; want = 2; }
    if (strcmp(name, "int") == 0) { kind = 5; want = 1; }
    if (strcmp(name, "float") == 0) { kind = 6; want = 1; }
    if (strcmp(name, "float32") == 0) { kind = 7; want = 1; }
    if (strcmp(name, "i64") == 0) { kind = 5; want = 1; }                 /* i64 is int */
    if (is_small_machine(name)) { kind = 8; want = 1; }                 /* u8(x): keeps the low bits, like C */
    if (!kind) return 0;

    if (n->child_count != want) {
        fprintf(stderr, "semantic error: %s() expects %zu argument(s), got %zu\n", name, want, n->child_count);
        s->had_error = 1;
        return -1;
    }

    /* abs, min, max and pow of big integers: exact, in the big type */
    int big_math = kind == 3 || kind == 4 || (kind == 2 && strcmp(name, "pow") == 0);
    int any_big = 0;
    const char *args[2] = { NULL, NULL };
    if (kind == 4 && is_int_literal(n->children[0]) != is_int_literal(n->children[1])) {
        /* max(x, 0) or min(x, 10^30): the number takes the big type of x */
        size_t lit = is_int_literal(n->children[0]) ? 0 : 1;
        args[1 - lit] = expr_type(s, n->children[1 - lit]);
        if (!args[1 - lit]) return -1;
        args[lit] = is_big(args[1 - lit]) ? type_int_literal(s, n->children[lit], args[1 - lit])
                                          : expr_type(s, n->children[lit]);
        if (!args[lit]) return -1;
    }
    for (size_t i = 0; i < want; i++) {
        if (!args[i]) args[i] = expr_type(s, n->children[i]);
        if (!args[i]) return -1;
        if (big_math && is_big(args[i])) {
            any_big = 1;
            continue;
        }
        if ((kind == 5 || kind == 6) && is_big(args[i])) {
            /* int(x) is checked (the value must fit), float(x) rounds to the nearest float */
            char *renamed = malloc(sizeof("@convert"));
            if (!renamed) return -1;
            strcpy(renamed, "@convert");
            free(n->text);
            n->text = renamed;
            return set_type(n, kind == 5 ? "int" : "float") ? 1 : -1;
        }
        int ok = is_numeric(args[i]) || ((kind == 5 || kind == 8) && strcmp(args[i], "bool") == 0) ||
                 ((kind == 5 || kind == 6 || kind == 7) && strcmp(args[i], "string") == 0);   /* int("42"), float("2.5") */
        if (!ok) {
            fprintf(stderr, "semantic error: argument %zu of %s() expects int or float, got %s\n",
                    i + 1, name, args[i]);
            s->had_error = 1;
            return -1;
        }
    }

    if ((kind == 2 || kind == 4) && !any_big) {
        /* pow(x, 2.0) or max(x, 0.0) with x a float32: the number written is a float32 */
        for (size_t i = 0; i < 2; i++) {
            if (strcmp(args[1 - i], "float32") == 0 && is_float_literal(n->children[i])) {
                if (!set_float32_literal(n->children[i])) return -1;
                args[i] = "float32";
            }
        }
    }
    const char *result = "float";
    if (kind == 1 && strcmp(args[0], "float32") == 0) result = "float32";
    if (kind == 2 && numeric_result(args[0], args[1]) && strcmp(numeric_result(args[0], args[1]), "float32") == 0) result = "float32";
    if (kind == 3) result = args[0];
    if (kind == 4) result = numeric_result(args[0], args[1]);
    if (kind == 4 && !result && !any_big) {
        mix_error(s, strcmp(name, "min") == 0 ? "min()" : "max()", args[0], args[1]);
        return -1;
    }
    if (kind == 5) result = "int";
    if (kind == 7) result = "float32";
    if (kind == 8) {
        /* the type's own name (n->text is replaced below) */
        static const char *const machine_names[] = { "i8", "i16", "i32", "u8", "u16", "u32", "u64" };
        for (size_t i = 0; i < sizeof(machine_names) / sizeof(machine_names[0]); i++) {
            if (strcmp(name, machine_names[i]) == 0) result = machine_names[i];
        }
    }
    if (any_big && kind == 4) {
        char what[16];
        snprintf(what, sizeof(what), "%s()", name);
        result = big_common_type(s, n, what, args[0], args[1]);
        if (!result) return -1;
    } else if (any_big && kind == 2) {          /* pow(x, n): exact, n an int, 0 or more */
        if (!is_big(args[0]) || strcmp(args[1], "int") != 0) {
            fprintf(stderr, "semantic error: pow() of a big integer expects pow(big integer, int), got pow(%s, %s)\n",
                    args[0], args[1]);
            s->had_error = 1;
            return -1;
        }
        result = args[0];
    }

    if (strcmp(name, "i64") == 0) name = "int";
    char *renamed = malloc(strlen(name) + 2);
    if (!renamed) return -1;
    renamed[0] = '@';
    strcpy(renamed + 1, name);
    free(n->text);
    n->text = renamed;
    set_type(n, result);
    return 1;
}

/* 12345 or -12345 written directly in the program */
static int is_int_literal(const AstNode *n) {
    if (!n) return 0;
    if (n->kind == AST_INTEGER) return 1;
    return n->kind == AST_UNARY && n->text && strcmp(n->text, "-") == 0 && n->child_count == 1 &&
           n->children[0]->kind == AST_INTEGER;
}

/*
 * Gives an integer literal its type (int or a big integer) after checking
 * that the value fits.  For a big integer, -123 becomes a single literal.
 */
static const char *type_int_literal(Semantic *s, AstNode *n, const char *type) {
    int negative = n->kind == AST_UNARY;
    AstNode *digits = negative ? n->children[0] : n;
    if (!digits->text) return NULL;
    size_t len = strlen(digits->text);
    char *text = malloc(len + 2);
    if (!text) return NULL;
    snprintf(text, len + 2, "%s%s", negative ? "-" : "", digits->text);

    int bits = int_bits(type);
    uint64_t words[16];
    if (!bits || !clair_decimal_to_words(text, words, bits / 64, is_ubig(type))) {
        if (strcmp(type, "int") == 0) {
            fprintf(stderr, "semantic error: integer %s is too large for int "
                            "(64 bits): use int128, int256, int512 or int1024\n", text);
        } else if (negative && is_ubig(type)) {
            fprintf(stderr, "semantic error: %s cannot hold the negative integer %s\n", type, text);
        } else {
            fprintf(stderr, "semantic error: integer %s is too large for %s\n", text, type);
        }
        s->had_error = 1;
        free(text);
        return NULL;
    }

    if (negative && is_big(type)) {
        /* one literal "-123": the C code builds the value directly */
        ast_free(digits);
        free(n->children);
        n->children = NULL;
        n->child_count = n->child_capacity = 0;
        n->kind = AST_INTEGER;
        free(n->text);
        n->text = text;
    } else {
        free(text);
        if (negative && !set_type(digits, type)) return NULL;
    }
    return set_type(n, type) ? n->type_name : NULL;
}

/*
 * A number written next to (or into) a u8, i32...: it must fit, and takes
 * that type; -5 becomes a single literal.
 */
static const char *machine_literal(Semantic *s, AstNode *n, const char *type) {
    int negative = n->kind == AST_UNARY;
    const AstNode *digits = negative ? n->children[0] : n;
    if (!digits->text) return NULL;
    int bits = machine_bits(type);
    uint64_t w[2];
    char text[64];
    snprintf(text, sizeof(text), "%s%s", negative ? "-" : "", digits->text);
    /* the value as a signed 128-bit number: enough for every machine integer */
    int ok = strlen(digits->text) <= 40 && clair_decimal_to_words(text, w, 2, 0);
    if (ok) {
        int neg = (w[1] >> 63) != 0;
        uint64_t mag = neg ? (uint64_t)0 - w[0] : w[0];
        if (neg && w[1] != UINT64_MAX) ok = 0;
        else if (!neg && w[1] != 0) ok = 0;
        else if (machine_unsigned(type)) ok = !neg && (bits == 64 || mag < ((uint64_t)1 << bits));
        else ok = neg ? mag <= ((uint64_t)1 << (bits - 1)) : mag < ((uint64_t)1 << (bits - 1));
        if (neg && machine_unsigned(type)) {
            fprintf(stderr, "semantic error: %s cannot hold the negative integer %s\n", type, text);
            s->had_error = 1;
            return NULL;
        }
    }
    if (!ok) {
        fprintf(stderr, "semantic error: integer %s does not fit in %s\n", text, type);
        s->had_error = 1;
        return NULL;
    }
    if (negative) {
        ast_free(n->children[0]);
        free(n->children);
        n->children = NULL;
        n->child_count = n->child_capacity = 0;
        n->kind = AST_INTEGER;
        free(n->text);
        n->text = malloc(strlen(text) + 1);
        if (!n->text) return NULL;
        strcpy(n->text, text);
    }
    return set_type(n, type) ? n->type_name : NULL;
}

/* Turns `n` in place into @convert(n): a value changing to a wider (or, explicitly, narrower) type. */
static int wrap_convert(AstNode *n, const char *type) {
    AstNode *inner = malloc(sizeof(*inner));
    char *name = malloc(sizeof("@convert"));
    if (!inner || !name) { free(inner); free(name); return 0; }
    *inner = *n;
    memset(n, 0, sizeof(*n));
    n->kind = AST_CALL;
    strcpy(name, "@convert");
    n->text = name;
    if (!ast_add_child(n, inner)) return 0;
    return set_type(n, type);
}

/*
 * The type in which two integers meet when one of them is big (for an
 * operator, min or max), after converting the other one:
 *   int and int256 -> int256, int128 and int1024 -> int1024 (never loses anything);
 *   uint128 and uint512 -> uint512;
 *   int and uint256 -> uint256 (a negative int stops the program);
 *   int256 and uint128: refused, the program must choose with int256(x) or uint256(x).
 */
static const char *big_common_type(Semantic *s, AstNode *n, const char *what, const char *left, const char *right) {
    if (is_float_type(left) || is_float_type(right)) {
        fprintf(stderr, "semantic error: %s cannot mix %s and %s: convert with float(x)\n", what, left, right);
        s->had_error = 1;
        return NULL;
    }
    if (!int_bits(left) || !int_bits(right)) {
        fprintf(stderr, "semantic error: %s expects numbers, got %s and %s\n", what, left, right);
        s->had_error = 1;
        return NULL;
    }
    const char *type = int_bits(left) >= int_bits(right) ? left : right;
    if (is_ubig(left) != is_ubig(right)) {
        const char *other = is_ubig(left) ? right : left;
        if (strcmp(other, "int") != 0) {
            int bits = int_bits(type);
            fprintf(stderr, "semantic error: %s cannot mix %s and %s: convert one of them with "
                            "int%d(x) or uint%d(x)\n", what, left, right, bits, bits);
            s->had_error = 1;
            return NULL;
        }
        type = is_ubig(left) ? left : right;
    }
    if (strcmp(left, type) != 0 && !wrap_convert(n->children[0], type)) return NULL;
    if (strcmp(right, type) != 0 && !wrap_convert(n->children[1], type)) return NULL;
    return type;
}

/* + - * / % and comparisons when one operand is a big integer */
static const char *big_binary_type(Semantic *s, AstNode *n, const char *left, const char *right) {
    const char *op = n->text;
    char what[64];
    snprintf(what, sizeof(what), "operator '%s'", op);
    const char *type = big_common_type(s, n, what, left, right);
    if (!type) return NULL;

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 ||
        strcmp(op, "/") == 0 || strcmp(op, "%") == 0) {
        return set_type(n, type) ? n->type_name : NULL;
    }
    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 || strcmp(op, "<") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        return set_type(n, "bool") ? n->type_name : NULL;
    }
    fprintf(stderr, "semantic error: operator '%s' is not supported for %s\n", op, type);
    s->had_error = 1;
    return NULL;
}

static const char *binary_type(Semantic *s, AstNode *n) {
    if (n->child_count != 2 || !n->text) return NULL;
    AstNode *lhs = n->children[0], *rhs = n->children[1];
    const char *left, *right;
    /*
     * A number written next to a big integer or a machine integer (u8, i32...)
     * takes its type (it must fit).  Not for shifts: in 1 << n, 1 is an int.
     */
    int shift = strcmp(n->text, "<<") == 0 || strcmp(n->text, ">>") == 0;
    if (is_int_literal(lhs) && !is_int_literal(rhs)) {
        right = expr_type(s, rhs);
        if (!right) return NULL;
        left = is_big(right) ? type_int_literal(s, lhs, right)
             : (!shift && is_small_machine(right)) ? machine_literal(s, lhs, right) : expr_type(s, lhs);
    } else if (is_int_literal(rhs) && !is_int_literal(lhs)) {
        left = expr_type(s, lhs);
        if (!left) return NULL;
        right = is_big(left) ? type_int_literal(s, rhs, left)
              : (!shift && is_small_machine(left)) ? machine_literal(s, rhs, left) : expr_type(s, rhs);
    } else {
        left = expr_type(s, lhs);
        right = expr_type(s, rhs);
    }
    if (!left || !right) return NULL;
    int dynleft = strcmp(left, "bigint") == 0 || strcmp(left, "bigfloat") == 0;
    int dynright = strcmp(right, "bigint") == 0 || strcmp(right, "bigfloat") == 0;
    if (dynleft || dynright) {
        const char *op = n->text;
        int comparison = !strcmp(op,"==") || !strcmp(op,"!=") || !strcmp(op,"<") ||
                         !strcmp(op,"<=") || !strcmp(op,">") || !strcmp(op,">=");
        int arithmetic = !strcmp(op,"+") || !strcmp(op,"-") || !strcmp(op,"*") || !strcmp(op,"/") ||
                         (!strcmp(op,"%") && !strcmp(left,"bigint"));
        if (strcmp(left,right) || (!comparison && !arithmetic)) {
            fprintf(stderr, "semantic error: unsupported or mixed big-number operands: %s %s %s\n",
                    left,op,right);
            s->had_error=1; return NULL;
        }
        set_type(n, comparison ? "bool" : left);
        return n->type_name;
    }
    if (strcmp(n->text, "in") == 0 || strcmp(n->text, "not in") == 0) {
        /* x in xs (a list), s in text (a string), key in d (a dictionary), like Python */
        const char *element = list_element_type(right);
        int ok = 0;
        if (element) {
            if (strcmp(element, left) == 0) ok = 1;
            else if (is_small_machine(element) && is_int_literal(lhs)) ok = machine_literal(s, lhs, element) != NULL;
            else if (strcmp(element, "float32") == 0 && is_float_literal(lhs)) ok = set_float32_literal(lhs);
            else if (is_numeric(left) && is_numeric(element) && numeric_result(left, element) &&
                     strcmp(numeric_result(left, element), element) == 0) ok = 1;   /* 2 in list[float] */
            if (!ok && !s->had_error) {
                fprintf(stderr, "semantic error: '%s' cannot look for %s in %s: convert it first, e.g. %s(x)\n",
                        n->text, left, right, element);
                s->had_error = 1;
                return NULL;
            }
        } else if (strcmp(right, "string") == 0 || is_dict_type(right)) {
            ok = strcmp(left, "string") == 0;
            if (!ok) {
                fprintf(stderr, "semantic error: '%s' %s expects a string on the left, got %s\n", n->text,
                        is_dict_type(right) ? "with a dictionary" : "in a string", left);
                s->had_error = 1;
                return NULL;
            }
        } else {
            fprintf(stderr, "semantic error: '%s' needs a list, a string or a dictionary on the right, got %s\n",
                    n->text, right);
            s->had_error = 1;
            return NULL;
        }
        if (!ok) return NULL;
        return set_type(n, "bool") ? n->type_name : NULL;
    }
    if (is_big(left) || is_big(right)) return big_binary_type(s, n, left, right);
    if (strcmp(left, "float32") == 0 && is_float_literal(rhs)) {
        if (!set_float32_literal(rhs)) return NULL;
        right = "float32";
    } else if (strcmp(right, "float32") == 0 && is_float_literal(lhs)) {
        if (!set_float32_literal(lhs)) return NULL;
        left = "float32";
    }

    if (strcmp(n->text, "&") == 0 || strcmp(n->text, "|") == 0 || strcmp(n->text, "^") == 0 || shift) {
        char what[32];
        snprintf(what, sizeof(what), "operator '%s'", n->text);
        if (!machine_bits(left) || !machine_bits(right)) {
            fprintf(stderr, "semantic error: %s works on integers (int, u8 ... u64, i8 ... i32), got %s and %s\n",
                    what, left, right);
            s->had_error = 1;
            return NULL;
        }
        /* a << n has the type of a; & | ^ meet like + */
        const char *type = shift ? left : machine_meet(left, right);
        if (!type) {
            mix_error(s, what, left, right);
            return NULL;
        }
        return set_type(n, type) ? n->type_name : NULL;
    }

    if (strcmp(n->text, "and") == 0 || strcmp(n->text, "or") == 0) {
        if (strcmp(left, "bool") != 0 || strcmp(right, "bool") != 0) {
            error_type(s, "logical operators 'and' and 'or' require bool operands");
            return NULL;
        }
        set_type(n, "bool");
        return n->type_name;
    }

    if (strcmp(n->text, "+") == 0) {
        if (strcmp(left, "string") == 0 && strcmp(right, "string") == 0) {
            set_type(n, "string");
            return n->type_name;
        }
        if (strcmp(left, "string") == 0 || strcmp(right, "string") == 0) {
            error_type(s, "operator '+' requires two numbers or two strings");
            return NULL;
        }
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "arithmetic operators require int or float operands");
            return NULL;
        }
        if (!numeric_result(left, right)) {
            char what[32];
            snprintf(what, sizeof(what), "operator '%s'", n->text);
            mix_error(s, what, left, right);
            return NULL;
        }
        set_type(n, numeric_result(left, right));
        return n->type_name;
    }

    if (strcmp(n->text, "-") == 0 || strcmp(n->text, "*") == 0 ||
        strcmp(n->text, "/") == 0) {
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "arithmetic operators '-', '*' and '/' require int or float operands");
            return NULL;
        }
        if (!numeric_result(left, right)) {
            char what[32];
            snprintf(what, sizeof(what), "operator '%s'", n->text);
            mix_error(s, what, left, right);
            return NULL;
        }
        set_type(n, numeric_result(left, right));
        return n->type_name;
    }

    if (strcmp(n->text, "%") == 0) {
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "operator '%' requires int or float operands");
            return NULL;
        }
        if (!numeric_result(left, right)) {
            char what[32];
            snprintf(what, sizeof(what), "operator '%s'", n->text);
            mix_error(s, what, left, right);
            return NULL;
        }
        set_type(n, numeric_result(left, right));
        return n->type_name;
    }

    if (strcmp(n->text, "==") == 0 || strcmp(n->text, "!=") == 0) {
        if (is_matrix_type(left) || is_matrix_type(right) ||
            is_matrix_row_type(left) || is_matrix_row_type(right)) {
            error_type(s, "matrix equality is not supported");
            return NULL;
        }
        if (is_list_type(left) || is_list_type(right) ||
            is_dict_type(left) || is_dict_type(right)) {
            error_type(s, "list or dictionary equality is not supported in Clair V0.15");
            return NULL;
        }
        if (find_class(s, left) || find_class(s, right)) {
            error_type(s, "object equality is not supported in Clair V0.14");
            return NULL;
        }
        int okay = (is_numeric(left) && is_numeric(right)) || strcmp(left, right) == 0;
        if (!okay) {
            error_type(s, "incompatible operands for equality comparison");
            return NULL;
        }
        if (is_numeric(left) && is_numeric(right) && !numeric_result(left, right)) {
            mix_error(s, "comparison", left, right);   /* u32 == i32: -1 would equal 4294967295 in C */
            return NULL;
        }
        set_type(n, "bool");
        return n->type_name;
    }

    if (strcmp(n->text, "<") == 0 || strcmp(n->text, "<=") == 0 ||
        strcmp(n->text, ">") == 0 || strcmp(n->text, ">=") == 0) {
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "ordered comparisons require int or float operands");
            return NULL;
        }
        if (!numeric_result(left, right)) {
            mix_error(s, "comparison", left, right);
            return NULL;
        }
        set_type(n, "bool");
        return n->type_name;
    }

    error_type(s, "unknown binary operator");
    return NULL;
}

static const char *check_matrix_make(Semantic *s, AstNode *n, const char *matrix_type) {
    if (n->child_count != 2) {
        fprintf(stderr, "semantic error: matrix() expects 2 arguments (rows, cols), got %zu\n", n->child_count);
        s->had_error = 1;
        return NULL;
    }
    for (size_t i = 0; i < 2; i++) {
        const char *t = expr_type(s, n->children[i]);
        if (!t) return NULL;
        if (!is_index_type(t)) {
            fprintf(stderr, "semantic error: matrix() %s must be int, got %s\n", i == 0 ? "rows" : "cols", t);
            s->had_error = 1;
            return NULL;
        }
    }
    free(n->text);
    n->text = malloc(8);
    if (!n->text) return NULL;
    strcpy(n->text, "@matrix");
    set_type(n, matrix_type);
    return n->type_name;
}

static const char *expr_type_expected_plain(Semantic *s, AstNode *n, const char *expected);

/* xs[a:b:c] (the parser's @slice call): a new list, or a new string; bounds are int */
static const char *check_slice(Semantic *s, AstNode *n) {
    const char *base = expr_type(s, n->children[0]);
    if (!base) return NULL;
    if (!is_list_type(base) && strcmp(base, "string") != 0) {
        fprintf(stderr, "semantic error: slices [a:b] work on lists and strings, got %s\n", base);
        s->had_error = 1;
        return NULL;
    }
    for (size_t i = 1; i < n->child_count; i++) {
        const char *t = expr_type(s, n->children[i]);
        if (!t) return NULL;
        if (!is_index_type(t)) {
            fprintf(stderr, "semantic error: slice bounds must be int, got %s\n", t);
            s->had_error = 1;
            return NULL;
        }
    }
    return set_type(n, base) ? n->type_name : NULL;
}

/*
 * Integer conversions done without being written:
 *   int -> int256, int128 -> int1024, uint128 -> uint256, uint128 -> int256:
 *   widening, never loses anything;
 *   int -> uint128 ...: a negative int stops the program.
 */
static int implicit_big(const char *actual, const char *expected) {
    if (!is_big(expected) || !int_bits(actual) || strcmp(actual, expected) == 0) return 0;
    if (is_ubig(expected)) return strcmp(actual, "int") == 0 || (is_ubig(actual) && int_bits(actual) < int_bits(expected));
    return int_bits(actual) < int_bits(expected);
}

static const char *expr_type_expected(Semantic *s, AstNode *n, const char *expected) {
    if (is_big(expected) && is_int_literal(n)) return type_int_literal(s, n, expected);
    if (is_small_machine(expected) && is_int_literal(n)) return machine_literal(s, n, expected);
    if (expected && strcmp(expected, "float32") == 0 && is_float_literal(n)) {
        /* let x: float32 = 0.1 : the number written is a float32 (0.1f in C) */
        if (!expr_type(s, n) || !set_float32_literal(n)) return NULL;
        return n->type_name;
    }
    /* pow(2, 1000) where an int1024 is expected: 2 is an int1024 */
    if (is_big(expected) && n && n->kind == AST_CALL && n->text && strcmp(n->text, "pow") == 0 &&
        n->child_count == 2 && is_int_literal(n->children[0]) && !find_function(s, "pow") &&
        !type_int_literal(s, n->children[0], expected)) return NULL;
    const char *actual = expr_type_expected_plain(s, n, expected);
    if (actual && implicit_big(actual, expected)) return wrap_convert(n, expected) ? n->type_name : NULL;
    return actual;
}

static const char *expr_type_expected_plain(Semantic *s, AstNode *n, const char *expected) {
    if (n && n->kind == AST_CALL && n->text && strcmp(n->text, "matrix") == 0 &&
        is_matrix_type(expected) && !find_function(s, "matrix")) {
        return check_matrix_make(s, n, expected);
    }
    if (n && n->kind == AST_LIST && is_list_type(expected)) {
        return check_list_literal_as(s, n, expected);
    }
    if (n && n->kind == AST_DICT && is_dict_type(expected)) {
        return check_dict_literal_as(s, n, expected);
    }
    return expr_type(s, n);
}

static const char *expr_type(Semantic *s, AstNode *n) {
    if (!n) return NULL;

    switch (n->kind) {
        case AST_INTEGER:
            if (is_big(n->type_name)) return n->type_name;   /* typed by its context */
            return type_int_literal(s, n, "int");
        case AST_FLOAT:
            if (n->type_name && strcmp(n->type_name, "float32") == 0) return n->type_name;   /* typed by its context */
            set_type(n, "float");
            return n->type_name;
        case AST_BOOL:
            set_type(n, "bool");
            return n->type_name;
        case AST_STRING:
            set_type(n, "string");
            return n->type_name;
        case AST_LIST:
            return infer_list_literal(s, n);
        case AST_DICT:
            return infer_dict_literal(s, n);
        case AST_INDEX: {
            if (n->child_count != 2) return NULL;
            const char *base = expr_type(s, n->children[0]);
            const char *index = expr_type(s, n->children[1]);

            if (base && is_list_type(base)) {
                if (!index || !is_index_type(index)) {
                    fprintf(stderr, "semantic error: list index must be int, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
                set_type(n, list_element_type(base));
                return n->type_name;
            }

            if (base && (is_matrix_type(base) || is_matrix_row_type(base))) {
                if (!index || !is_index_type(index)) {
                    fprintf(stderr, "semantic error: matrix index must be int, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
                if (is_matrix_type(base)) {
                    char row[32];
                    snprintf(row, sizeof(row), "matrix_row[%s]", matrix_element_type(base));
                    set_type(n, row);
                } else {
                    set_type(n, matrix_element_type(base));
                }
                return n->type_name;
            }

            if (base && is_dict_type(base)) {
                if (!index || strcmp(index, "string") != 0) {
                    fprintf(stderr, "semantic error: dictionary key must be string, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
                set_type(n, dict_value_type(base));
                return n->type_name;
            }

            fprintf(stderr, "semantic error: indexing expects a list or dictionary, got %s\n",
                    base ? base : "invalid");
            s->had_error = 1;
            return NULL;
        }
        case AST_MEMBER: {
            if (n->child_count != 1 || !n->text) return NULL;
            const char *base = expr_type(s, n->children[0]);
            const AstNode *cls = base ? find_class(s, base) : NULL;
            if (!cls) {
                fprintf(stderr, "semantic error: member access expects a class object, got %s\n",
                        base ? base : "invalid");
                s->had_error = 1;
                return NULL;
            }
            const AstNode *field = find_field(cls, n->text);
            if (!field) {
                fprintf(stderr, "semantic error: class '%s' has no field '%s'\n", base, n->text);
                s->had_error = 1;
                return NULL;
            }
            set_type(n, field->type_name);
            return n->type_name;
        }
        case AST_IDENTIFIER: {
            const char *type = find_var(s, n->text);
            if (!type) {
                fprintf(stderr, "semantic error: undefined variable '%s'\n", n->text);
                s->had_error = 1;
                return NULL;
            }
            set_type(n, type);
            return n->type_name;
        }
        case AST_CALL: {
            if (n->text && (strcmp(n->text, "bigint") == 0 || strcmp(n->text, "bigfloat") == 0)) {
                int flt = strcmp(n->text, "bigfloat") == 0;
                size_t count = flt ? 2 : 1;
                if (n->child_count != count) {
                    fprintf(stderr, "semantic error: %s() expects %zu arguments\n", n->text, count);
                    s->had_error = 1; return NULL;
                }
                const char *arg = expr_type(s, n->children[0]);
                if (!arg || strcmp(arg, "string") != 0) {
                    fprintf(stderr, "semantic error: %s() expects a decimal string\n", n->text);
                    s->had_error = 1; return NULL;
                }
                if (flt) {
                    const char *p = expr_type(s, n->children[1]);
                    if (!p || strcmp(p, "int") != 0) {
                        fprintf(stderr, "semantic error: bigfloat() precision must be int\n");
                        s->had_error = 1; return NULL;
                    }
                }
                set_type(n, flt ? "bigfloat" : "bigint");
                return n->type_name;
            }
            if (n->text && strncmp(n->text, "@slice", 6) == 0 && !n->type_name) return check_slice(s, n);
            if (n->text && strcmp(n->text, "@comprehension") == 0) return check_comprehension(s, n);
            if (n->text && n->text[0] == '@') return n->type_name;  /* builtin already checked */
            if (n->text && strcmp(n->text, "input_int") == 0) {
                if (n->child_count != 0) {
                    fprintf(stderr, "semantic error: input_int() expects 0 arguments, got %zu\n", n->child_count);
                    s->had_error = 1;
                    return NULL;
                }
                set_type(n, "int");
                return n->type_name;
            }

            if (n->text && strcmp(n->text, "len") == 0) {
                if (n->child_count != 1) {
                    fprintf(stderr, "semantic error: len() expects 1 argument, got %zu\n", n->child_count);
                    s->had_error = 1;
                    return NULL;
                }
                const char *arg = expr_type(s, n->children[0]);
                if (arg && is_matrix_type(arg)) {
                    error_type(s, "len() of a matrix is ambiguous: use rows(m) or cols(m)");
                    return NULL;
                }
                if (!arg || (strcmp(arg, "string") != 0 && !is_list_type(arg) && !is_dict_type(arg))) {
                    fprintf(stderr, "semantic error: len() expects string, list or dictionary, got %s\n", arg ? arg : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
                set_type(n, "int");
                return n->type_name;
            }

            const AstNode *cls = find_class(s, n->text);
            if (cls) {
                if (n->child_count != cls->child_count) {
                    fprintf(stderr, "semantic error: class '%s' constructor expects %zu argument(s), got %zu\n",
                            n->text, cls->child_count, n->child_count);
                    s->had_error = 1;
                    return NULL;
                }
                for (size_t i = 0; i < cls->child_count; i++) {
                    const AstNode *field = cls->children[i];
                    const char *expected = field->type_name;
                    const char *actual = expr_type_expected(s, n->children[i], expected);
                    if (!actual || !assignable(expected, actual)) {
                        fprintf(stderr,
                                "semantic error: constructor argument %zu for '%s.%s' expects %s, got %s\n",
                                i + 1, cls->text, field->text, expected ? expected : "?",
                                actual ? actual : "invalid");
                        s->had_error = 1;
                        return NULL;
                    }
                }
                n->kind = AST_CONSTRUCT;
                set_type(n, cls->text);
                return n->type_name;
            }

            const AstNode *fn = find_function(s, n->text);
            if (!fn) {
                int builtin = check_matrix_builtin(s, n);
                if (builtin == 0) builtin = check_builtin_call(s, n);
                if (builtin == 0) builtin = check_random_call(s, n);
                if (builtin == 0) builtin = check_text_call(s, n);
                if (builtin == 1) return n->type_name;
                if (builtin == -1) return NULL;
                fprintf(stderr, "semantic error: undefined function or class '%s'\n", n->text);
                s->had_error = 1;
                return NULL;
            }
            size_t param_count = fn->child_count ? fn->child_count - 1 : 0;
            if (n->child_count != param_count) {
                fprintf(stderr, "semantic error: function '%s' expects %zu argument(s), got %zu\n",
                        n->text, param_count, n->child_count);
                s->had_error = 1;
                return NULL;
            }
            for (size_t i = 0; i < param_count; i++) {
                const char *expected = fn->children[i]->type_name;
                const char *actual = expr_type_expected(s, n->children[i], expected);
                if (!actual || !assignable(expected, actual)) {
                    fprintf(stderr, "semantic error: argument %zu of '%s' expects %s, got %s\n",
                            i + 1, n->text, expected ? expected : "?", actual ? actual : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
            }
            set_type(n, fn->type_name);
            return n->type_name;
        }
        case AST_METHOD_CALL: {
            /*
             * receiver.method(args) is resolved to the plain function
             * Class__method(receiver, args), so code generation needs no
             * knowledge of methods.  Children are [receiver, args...].
             */
            if (n->child_count < 1 || !n->text) return NULL;
            const char *receiver = expr_type(s, n->children[0]);
            const AstNode *cls = receiver ? find_class(s, receiver) : NULL;
            if (!cls) {
                if (receiver && is_list_type(receiver) && strcmp(n->text, "append") == 0) {
                    error_type(s, "append() needs a list variable or field (v.append(x), obj.items.append(x))");
                    return NULL;
                }
                fprintf(stderr, "semantic error: method call '%s' expects a class object, got %s\n",
                        n->text, receiver ? receiver : "invalid");
                s->had_error = 1;
                return NULL;
            }

            size_t mangled_len = strlen(cls->text) + 2 + strlen(n->text);
            char *mangled = malloc(mangled_len + 1);
            if (!mangled) return NULL;
            snprintf(mangled, mangled_len + 1, "%s__%s", cls->text, n->text);

            const AstNode *fn = find_function(s, mangled);
            if (!fn) {
                if (find_field(cls, n->text)) {
                    fprintf(stderr, "semantic error: '%s.%s' is a field, not a method\n",
                            cls->text, n->text);
                } else {
                    fprintf(stderr, "semantic error: class '%s' has no method '%s'\n",
                            cls->text, n->text);
                }
                s->had_error = 1;
                free(mangled);
                return NULL;
            }

            size_t param_count = fn->child_count ? fn->child_count - 1 : 0; /* includes self */
            if (n->child_count != param_count) {
                fprintf(stderr, "semantic error: method '%s.%s' expects %zu argument(s), got %zu\n",
                        cls->text, n->text, param_count - 1, n->child_count - 1);
                s->had_error = 1;
                free(mangled);
                return NULL;
            }
            for (size_t i = 1; i < param_count; i++) {
                const char *expected = fn->children[i]->type_name;
                const char *actual = expr_type_expected(s, n->children[i], expected);
                if (!actual || !assignable(expected, actual)) {
                    fprintf(stderr, "semantic error: argument %zu of '%s.%s' expects %s, got %s\n",
                            i, cls->text, n->text, expected ? expected : "?",
                            actual ? actual : "invalid");
                    s->had_error = 1;
                    free(mangled);
                    return NULL;
                }
            }

            free(n->text);
            n->text = mangled;
            n->kind = AST_CALL;
            set_type(n, fn->type_name);
            return n->type_name;
        }
        case AST_CONSTRUCT:
            return n->type_name;
        case AST_BINARY:
            return binary_type(s, n);
        case AST_UNARY: {
            if (n->child_count != 1 || !n->text) return NULL;
            if (is_int_literal(n)) return type_int_literal(s, n, "int");   /* -9223372036854775808 fits */
            const char *operand = expr_type(s, n->children[0]);
            if (!operand) return NULL;
            if (!strcmp(operand,"bigint") || !strcmp(operand,"bigfloat")) {
                if (strcmp(n->text,"-")) {error_type(s,"unary operator not supported for big numbers");return NULL;}
                set_type(n,operand);return n->type_name;
            }
            if (strcmp(n->text, "-") == 0) {
                if (is_ubig(operand)) {
                    fprintf(stderr, "semantic error: unary '-' is not allowed on %s (never negative): "
                                    "convert with int%d(x)\n", operand, 2 * big_bits(operand) > 1024 ? 1024 : 2 * big_bits(operand));
                    s->had_error = 1;
                    return NULL;
                }
                if (is_big(operand)) {
                    set_type(n, operand);
                    return n->type_name;
                }
                if (!is_numeric(operand)) {
                    error_type(s, "unary '-' requires an int or float operand");
                    return NULL;
                }
                set_type(n, operand);
                return n->type_name;
            }
            if (strcmp(n->text, "~") == 0) {
                if (!machine_bits(operand)) {
                    error_type(s, "'~' works on integers (int, u8 ... u64, i8 ... i32)");
                    return NULL;
                }
                set_type(n, operand);
                return n->type_name;
            }
            if (strcmp(n->text, "not") == 0) {
                if (strcmp(operand, "bool") != 0) {
                    error_type(s, "'not' requires a bool operand");
                    return NULL;
                }
                set_type(n, "bool");
                return n->type_name;
            }
            error_type(s, "unknown unary operator");
            return NULL;
        }
        default:
            error_type(s, "invalid expression node");
            return NULL;
    }
}

static int check_block(Semantic *s, AstNode *block, int create_scope);

/*
 * Type of the target of a write: a variable named by `text`, or a field
 * chain stored as an extra last child (then `text` is NULL).
 */
static const char *write_target_type(Semantic *s, AstNode *n, size_t plain_children, const char *what) {
    if (!n->text) {
        if (n->child_count != plain_children + 1) return NULL;
        return expr_type(s, n->children[plain_children]);
    }
    const char *base = find_var(s, n->text);
    if (!base) {
        fprintf(stderr, "semantic error: %s undefined variable '%s'\n", what, n->text);
        s->had_error = 1;
    }
    return base;
}

static int check_stmt(Semantic *s, AstNode *n) {
    switch (n->kind) {
        case AST_LET: {
            const char *actual = expr_type_expected(s, n->children[0], n->type_name);
            if (!actual) return 0;
            if (!assignable(n->type_name, actual)) {
                if (is_matrix_row_type(actual)) {
                    error_type(s, "m[i] alone is a matrix row: write m[i][j]");
                    return 0;
                }
                fprintf(stderr, "semantic error: cannot initialize %s '%s' with %s\n",
                        n->type_name, n->text, actual);
                s->had_error = 1;
                return 0;
            }
            return add_var(s, n->text, n->type_name);
        }
        case AST_ASSIGN: {
            const char *expected = find_var(s, n->text);
            if (!expected) {
                fprintf(stderr, "semantic error: assignment to undefined variable '%s'\n", n->text);
                s->had_error = 1;
                return 0;
            }
            const char *actual = expr_type_expected(s, n->children[0], expected);
            if (!actual || !assignable(expected, actual)) {
                fprintf(stderr, "semantic error: cannot assign %s to %s variable '%s'\n",
                        actual ? actual : "invalid", expected, n->text);
                s->had_error = 1;
                return 0;
            }
            return 1;
        }
        case AST_MEMBER_ASSIGN: {
            if (n->child_count != 2 || !n->text) return 0;
            const char *base = expr_type(s, n->children[0]);
            const AstNode *cls = base ? find_class(s, base) : NULL;
            if (!cls) {
                fprintf(stderr, "semantic error: member assignment expects a class object, got %s\n",
                        base ? base : "invalid");
                s->had_error = 1;
                return 0;
            }
            const AstNode *field = find_field(cls, n->text);
            if (!field) {
                fprintf(stderr, "semantic error: class '%s' has no field '%s'\n", base, n->text);
                s->had_error = 1;
                return 0;
            }
            const char *actual = expr_type_expected(s, n->children[1], field->type_name);
            if (!actual || !assignable(field->type_name, actual)) {
                fprintf(stderr, "semantic error: field '%s.%s' expects %s, got %s\n",
                        base, n->text, field->type_name,
                        actual ? actual : "invalid");
                s->had_error = 1;
                return 0;
            }
            set_type(n, field->type_name);
            return 1;
        }
        case AST_INDEX_ASSIGN: {
            const char *base = write_target_type(s, n, 2, "assignment to");
            if (!base) return 0;

            const char *index = expr_type(s, n->children[0]);

            if (is_list_type(base)) {
                if (!index || !is_index_type(index)) {
                    fprintf(stderr, "semantic error: list index must be int, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return 0;
                }
                const char *element = list_element_type(base);
                const char *value = expr_type_expected(s, n->children[1], element);
                if (!value || !assignable(element, value)) {
                    fprintf(stderr, "semantic error: %s item assignment expects %s, got %s\n",
                            base, element, value ? value : "invalid");
                    s->had_error = 1;
                    return 0;
                }
                set_type(n, base);
                return 1;
            }

            if (is_dict_type(base)) {
                if (!index || strcmp(index, "string") != 0) {
                    fprintf(stderr, "semantic error: dictionary key must be string, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return 0;
                }
                const char *value_type = dict_value_type(base);
                const char *value = expr_type_expected(s, n->children[1], value_type);
                if (!value || !assignable(value_type, value)) {
                    fprintf(stderr, "semantic error: %s item assignment expects %s, got %s\n",
                            base, value_type, value ? value : "invalid");
                    s->had_error = 1;
                    return 0;
                }
                set_type(n, base);
                return 1;
            }

            fprintf(stderr, "semantic error: indexed assignment expects a list or dictionary, got %s\n", base);
            s->had_error = 1;
            return 0;
        }
        case AST_MATRIX_SET: {
            if (n->child_count != 3 && n->child_count != 4) return 0;
            const char *base = write_target_type(s, n, 3, "assignment to");
            if (!base) return 0;
            if (!is_matrix_type(base)) {
                fprintf(stderr, "semantic error: '%s[i][j] = ...' needs a matrix, got %s\n",
                        n->text ? n->text : "field", base);
                s->had_error = 1;
                return 0;
            }
            for (size_t i = 0; i < 2; i++) {
                const char *idx = expr_type(s, n->children[i]);
                if (!idx || !is_index_type(idx)) {
                    fprintf(stderr, "semantic error: matrix index must be int, got %s\n", idx ? idx : "invalid");
                    s->had_error = 1;
                    return 0;
                }
            }
            const char *element = matrix_element_type(base);
            const char *value = expr_type_expected(s, n->children[2], element);
            if (!value || !assignable(element, value)) {
                fprintf(stderr, "semantic error: %s element assignment expects %s, got %s\n",
                        base, element, value ? value : "invalid");
                s->had_error = 1;
                return 0;
            }
            set_type(n, base);
            return 1;
        }
        case AST_LIST_APPEND: {
            const char *base = write_target_type(s, n, 1, "append on");
            if (!base) return 0;
            if (!is_list_type(base)) {
                fprintf(stderr, "semantic error: append() expects a list, got %s\n", base);
                s->had_error = 1;
                return 0;
            }
            const char *element = list_element_type(base);
            const char *value = expr_type_expected(s, n->children[0], element);
            if (!value || !assignable(element, value)) {
                fprintf(stderr, "semantic error: %s.append() expects %s, got %s\n",
                        base, element, value ? value : "invalid");
                s->had_error = 1;
                return 0;
            }
            set_type(n, base);
            return 1;
        }
        case AST_RETURN: {
            const char *actual = expr_type_expected(s, n->children[0], s->return_type);
            if (!actual || !assignable(s->return_type, actual)) {
                fprintf(stderr, "semantic error: return expects %s, got %s\n",
                        s->return_type ? s->return_type : "?", actual ? actual : "invalid");
                s->had_error = 1;
                return 0;
            }
            return 1;
        }
        case AST_PRINT: {
            const char *type = expr_type(s, n->children[0]);
            if (!type) return 0;
            if (!machine_bits(type) && !is_float_type(type) &&
                strcmp(type, "bool") != 0 && strcmp(type, "string") != 0 && !is_big(type) &&
                strcmp(type, "bigint") != 0 && strcmp(type, "bigfloat") != 0) {
                error_type(s, "print() received an unsupported type");
                return 0;
            }
            return 1;
        }
        case AST_IF: {
            const char *cond = expr_type(s, n->children[0]);
            if (!cond || strcmp(cond, "bool") != 0) {
                error_type(s, "if condition must be bool");
                return 0;
            }
            if (!check_block(s, n->children[1], 1)) return 0;
            if (n->child_count == 3 && !check_block(s, n->children[2], 1)) return 0;
            return 1;
        }
        case AST_WHILE: {
            const char *cond = expr_type(s, n->children[0]);
            if (!cond || strcmp(cond, "bool") != 0) {
                error_type(s, "while condition must be bool");
                return 0;
            }
            return check_block(s, n->children[1], 1);
        }
        case AST_FOR: {
            size_t argc = n->child_count - 1;
            for (size_t i = 0; i < argc; i++) {
                const char *type = expr_type(s, n->children[i]);
                if (!type || !is_index_type(type)) {
                    error_type(s, "range() arguments must be int");
                    return 0;
                }
            }
            enter_scope(s);
            int ok = add_var(s, n->text, "int") && check_block(s, n->children[n->child_count - 1], 0);
            leave_scope(s);
            return ok;
        }
        case AST_FOR_EACH: {
            /* the list is evaluated before the loop variable exists */
            const char *list = expr_type(s, n->children[0]);
            if (!list) return 0;
            const char *element = list_element_type(list);
            if (is_dict_type(list)) element = "string";             /* for k in d: the keys, in order */
            if (!element) {
                char message[160];
                if (is_matrix_type(list)) {
                    snprintf(message, sizeof(message),
                             "for ... in needs a list, got %s: use for i in range(rows(m))", list);
                } else {
                    snprintf(message, sizeof(message), "for ... in needs a list, got %s", list);
                }
                error_type(s, message);
                return 0;
            }
            if (!set_type(n, list)) return 0;
            enter_scope(s);
            int ok = add_var(s, n->text, element) && check_block(s, n->children[1], 0);
            leave_scope(s);
            return ok;
        }
        case AST_EXPR_STMT:
            /* A call used as a statement; its result, if any, is discarded. */
            return n->child_count == 1 && expr_type(s, n->children[0]) != NULL;
        case AST_BREAK:
        case AST_CONTINUE:
            return 1;
        default:
            error_type(s, "invalid statement node");
            return 0;
    }
}

static int check_block(Semantic *s, AstNode *block, int create_scope) {
    if (!block || block->kind != AST_BLOCK) return 0;
    if (create_scope) enter_scope(s);
    int ok = 1;
    for (size_t i = 0; i < block->child_count; i++) {
        if (!check_stmt(s, block->children[i])) {
            ok = 0;
            break;
        }
    }
    if (create_scope) leave_scope(s);
    return ok;
}

static int check_class_decl(Semantic *s, AstNode *cls) {
    if (!cls || cls->kind != AST_CLASS || !cls->text) return 0;

    for (size_t i = 0; i < cls->child_count; i++) {
        AstNode *field = cls->children[i];
        if (!field || field->kind != AST_FIELD || !field->text || !field->type_name) return 0;

        if (!is_known_type(s, field->type_name)) {
            fprintf(stderr, "semantic error: unknown type '%s' for field '%s.%s'\n",
                    field->type_name, cls->text, field->text);
            s->had_error = 1;
            return 0;
        }

        for (size_t j = 0; j < i; j++) {
            if (strcmp(field->text, cls->children[j]->text) == 0) {
                fprintf(stderr, "semantic error: duplicate field '%s.%s'\n", cls->text, field->text);
                s->had_error = 1;
                return 0;
            }
        }
    }
    return 1;
}

static int check_function_signature(Semantic *s, AstNode *fn) {
    if (!fn || fn->kind != AST_FUNCTION || !fn->type_name) return 0;
    if (!is_known_type(s, fn->type_name)) {
        fprintf(stderr, "semantic error: unknown return type '%s' for function '%s'\n",
                fn->type_name, fn->text);
        s->had_error = 1;
        return 0;
    }

    size_t param_count = fn->child_count ? fn->child_count - 1 : 0;
    for (size_t i = 0; i < param_count; i++) {
        AstNode *param = fn->children[i];
        if (!param->type_name || !is_known_type(s, param->type_name)) {
            fprintf(stderr, "semantic error: unknown type '%s' for parameter '%s' of '%s'\n",
                    param->type_name ? param->type_name : "?", param->text, fn->text);
            s->had_error = 1;
            return 0;
        }
    }
    return 1;
}

static int check_function(Semantic *s, AstNode *fn) {
    s->return_type = fn->type_name;
    enter_scope(s);

    size_t param_count = fn->child_count ? fn->child_count - 1 : 0;
    for (size_t i = 0; i < param_count; i++) {
        AstNode *param = fn->children[i];
        if (!add_var(s, param->text, param->type_name)) {
            leave_scope(s);
            return 0;
        }
    }

    AstNode *body = fn->children[fn->child_count - 1];
    int ok = check_block(s, body, 0);
    leave_scope(s);
    s->return_type = NULL;
    return ok;
}

int semantic_check(AstNode *program) {
    if (!program || program->kind != AST_PROGRAM) return 0;

    Semantic s = {0};
    s.program = program;

    /* Classes and functions share one top-level namespace because ClassName(...)
       is constructor syntax, just like a call. */
    for (size_t i = 0; i < program->child_count; i++) {
        AstNode *decl = program->children[i];
        if (!decl->text || (decl->kind != AST_CLASS && decl->kind != AST_FUNCTION)) {
            error_type(&s, "invalid top-level declaration");
            goto done;
        }
        for (size_t j = 0; j < i; j++) {
            AstNode *prev = program->children[j];
            if (prev->text && strcmp(decl->text, prev->text) == 0) {
                fprintf(stderr, "semantic error: duplicate top-level name '%s'\n", decl->text);
                s.had_error = 1;
                goto done;
            }
        }
    }

    for (size_t i = 0; i < program->child_count && !s.had_error; i++) {
        AstNode *decl = program->children[i];
        if (decl->kind == AST_CLASS) {
            if (!check_class_decl(&s, decl)) break;
        } else if (decl->kind == AST_FUNCTION) {
            if (!check_function_signature(&s, decl)) break;
        }
    }

    for (size_t i = 0; i < program->child_count && !s.had_error; i++) {
        AstNode *decl = program->children[i];
        if (decl->kind == AST_FUNCTION && !check_function(&s, decl)) break;
    }

done:
    while (s.var_count > 0) free(s.vars[--s.var_count].name);
    free(s.vars);
    return !s.had_error;
}

/*
 * C identifier hygiene.
 *
 * Variables, parameters and fields keep their Clair spelling in the generated
 * C, which breaks when that spelling is a C keyword or a macro / name the
 * generated code itself relies on (`char`, `default`, `NULL`, `strlen`, ...).
 * Such names get a `clair_u_` prefix.  The rename is a pure function of the
 * name, so every reference stays consistent.  It runs after semantic_check so
 * error messages keep showing the original spelling.  Functions need no
 * renaming here: code generation prefixes all of them with `clair_fn_`.
 */
static int collides_with_c(const char *name) {
    static const char *const reserved[] = {
        "auto", "case", "char", "const", "default", "do", "double", "enum",
        "extern", "goto", "inline", "long", "register", "restrict", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile",
        "NULL", "EOF", "errno", "stdin", "stdout", "stderr", "offsetof",
        "strlen", "strcmp", "int64_t", "uint64_t", "intmax_t", "size_t",
        "INT64_MIN", "INT64_MAX", "SIZE_MAX",
        NULL
    };
    for (size_t i = 0; reserved[i]; i++) {
        if (strcmp(name, reserved[i]) == 0) return 1;
    }
    if (strncmp(name, "clair_", 6) == 0 || strncmp(name, "Clair", 5) == 0) return 1;
    if (name[0] == '_' && (name[1] == '_' || (name[1] >= 'A' && name[1] <= 'Z'))) return 1;
    return 0;
}

static void mangle_node(AstNode *n) {
    if (!n) return;

    switch (n->kind) {
        case AST_IDENTIFIER:
        case AST_LET:
        case AST_ASSIGN:
        case AST_INDEX_ASSIGN:
        case AST_MATRIX_SET:
        case AST_LIST_APPEND:
        case AST_FOR:
        case AST_FOR_EACH:
        case AST_PARAM:
        case AST_FIELD:
        case AST_MEMBER:
        case AST_MEMBER_ASSIGN:
            if (n->text && n->text[0] == '@' && n->kind != AST_MEMBER) {
                /* a variable made by the parser (@enum1): clair_h_enum1, out of reach of user names */
                size_t len = strlen(n->text);
                char *renamed = malloc(len + 8);
                if (renamed) {
                    memcpy(renamed, "clair_h_", 8);
                    memcpy(renamed + 8, n->text + 1, len);
                    free(n->text);
                    n->text = renamed;
                }
            } else if (n->text && collides_with_c(n->text)) {
                size_t len = strlen(n->text);
                char *renamed = malloc(len + 9);
                if (renamed) {
                    memcpy(renamed, "clair_u_", 8);
                    memcpy(renamed + 8, n->text, len + 1);
                    free(n->text);
                    n->text = renamed;
                }
            }
            break;
        default:
            break;
    }

    for (size_t i = 0; i < n->child_count; i++) mangle_node(n->children[i]);
}

void semantic_mangle_c_identifiers(AstNode *program) {
    mangle_node(program);
}
