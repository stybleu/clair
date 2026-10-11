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

static int is_numeric(const char *type) {
    return type && (strcmp(type, "int") == 0 || strcmp(type, "float") == 0);
}

static int is_list_type(const char *type) {
    return type && (
        strcmp(type, "list[int]") == 0 ||
        strcmp(type, "list[float]") == 0 ||
        strcmp(type, "list[bool]") == 0 ||
        strcmp(type, "list[string]") == 0
    );
}

static const char *list_element_type(const char *type) {
    if (!type) return NULL;
    if (strcmp(type, "list[int]") == 0) return "int";
    if (strcmp(type, "list[float]") == 0) return "float";
    if (strcmp(type, "list[bool]") == 0) return "bool";
    if (strcmp(type, "list[string]") == 0) return "string";
    return NULL;
}

static const char *list_type_for_element(const char *type) {
    if (!type) return NULL;
    if (strcmp(type, "int") == 0) return "list[int]";
    if (strcmp(type, "float") == 0) return "list[float]";
    if (strcmp(type, "bool") == 0) return "list[bool]";
    if (strcmp(type, "string") == 0) return "list[string]";
    return NULL;
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


static int assignable(const char *dst, const char *src) {
    if (!dst || !src) return 0;
    if (strcmp(dst, src) == 0) return 1;
    return strcmp(dst, "float") == 0 && strcmp(src, "int") == 0;
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
    if (strcmp(type, "int128") == 0 ||
        strcmp(type, "uint128") == 0 ||
        strcmp(type, "float128") == 0) return 1;
    if (strcmp(type, "int") == 0 || strcmp(type, "float") == 0 ||
        strcmp(type, "bool") == 0 || strcmp(type, "string") == 0 ||
        (is_list_type(type) || is_dict_type(type))) {
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
        const char *item = expr_type(s, n->children[i]);
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

    int numeric_float = strcmp(element, "float") == 0;
    for (size_t i = 1; i < n->child_count; i++) {
        const char *item = expr_type(s, n->children[i]);
        if (!item) return NULL;

        if (is_numeric(element) && is_numeric(item)) {
            if (strcmp(item, "float") == 0) numeric_float = 1;
            continue;
        }

        if (strcmp(item, element) != 0) {
            fprintf(stderr, "semantic error: list literal mixes incompatible element types %s and %s\n",
                    element, item);
            s->had_error = 1;
            return NULL;
        }
    }

    if (is_numeric(element) && numeric_float) element = "float";
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


static const char *binary_type(Semantic *s, AstNode *n) {
    if (n->child_count != 2 || !n->text) return NULL;
    const char *left = expr_type(s, n->children[0]);
    const char *right = expr_type(s, n->children[1]);
    if (!left || !right) return NULL;

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
        set_type(n, (strcmp(left, "float") == 0 || strcmp(right, "float") == 0) ? "float" : "int");
        return n->type_name;
    }

    if (strcmp(n->text, "-") == 0 || strcmp(n->text, "*") == 0 ||
        strcmp(n->text, "/") == 0) {
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "arithmetic operators '-', '*' and '/' require int or float operands");
            return NULL;
        }
        set_type(n, (strcmp(left, "float") == 0 || strcmp(right, "float") == 0) ? "float" : "int");
        return n->type_name;
    }

    if (strcmp(n->text, "==") == 0 || strcmp(n->text, "!=") == 0) {
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
        set_type(n, "bool");
        return n->type_name;
    }

    if (strcmp(n->text, "<") == 0 || strcmp(n->text, "<=") == 0 ||
        strcmp(n->text, ">") == 0 || strcmp(n->text, ">=") == 0) {
        if (!is_numeric(left) || !is_numeric(right)) {
            error_type(s, "ordered comparisons require int or float operands");
            return NULL;
        }
        set_type(n, "bool");
        return n->type_name;
    }

    error_type(s, "unknown binary operator");
    return NULL;
}


static int clair_128_decimal_fits(
    const char *digits,
    const char *limit
) {
    if (!digits || !*digits) return 0;

    for (const char *p = digits; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
    }

    while (*digits == '0' && digits[1])
        ++digits;

    size_t actual = strlen(digits);
    size_t maximum = strlen(limit);

    if (actual != maximum)
        return actual < maximum;

    return strcmp(digits, limit) <= 0;
}

static int clair_128_literal_check(
    Semantic *s,
    AstNode *node,
    const char *limit
) {
    if (!clair_128_decimal_fits(node->text, limit)) {
        fprintf(stderr,
            "semantic error: 128-bit integer literal out of range\n");
        s->had_error = 1;
        return 0;
    }
    return 1;
}

static const char *expr_type_expected(Semantic *s, AstNode *n, const char *expected) {

    if (n && expected) {
        int signed128 = strcmp(expected, "int128") == 0;
        int unsigned128 = strcmp(expected, "uint128") == 0;
        int floating128 = strcmp(expected, "float128") == 0;

        if (n->kind == AST_INTEGER &&
            (signed128 || unsigned128)) {

            const char *limit = signed128
                ? "170141183460469231731687303715884105727"
                : "340282366920938463463374607431768211455";

            if (!clair_128_literal_check(s, n, limit))
                return NULL;

            set_type(n, expected);
            return n->type_name;
        }

        if (n->kind == AST_FLOAT && floating128) {
            set_type(n, "float128");
            return n->type_name;
        }

        if (n->kind == AST_UNARY && n->text &&
            strcmp(n->text, "-") == 0 &&
            n->child_count == 1) {

            AstNode *child = n->children[0];

            if (signed128 && child->kind == AST_INTEGER) {
                if (!clair_128_literal_check(
                        s, child,
                        "170141183460469231731687303715884105728"))
                    return NULL;

                set_type(child, "int128");
                set_type(n, "int128");
                return n->type_name;
            }

            if (floating128 && child->kind == AST_FLOAT) {
                set_type(child, "float128");
                set_type(n, "float128");
                return n->type_name;
            }
        }
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
            if (n->type_name &&
                (strcmp(n->type_name, "int128") == 0 ||
                 strcmp(n->type_name, "uint128") == 0))
                return n->type_name;
            set_type(n, "int");
            return n->type_name;
        case AST_FLOAT:
            if (n->type_name &&
                strcmp(n->type_name, "float128") == 0)
                return n->type_name;
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
                if (!index || strcmp(index, "int") != 0) {
                    fprintf(stderr, "semantic error: list index must be int, got %s\n",
                            index ? index : "invalid");
                    s->had_error = 1;
                    return NULL;
                }
                set_type(n, list_element_type(base));
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
        case AST_CONSTRUCT:
            return n->type_name;
        case AST_BINARY:
            return binary_type(s, n);
        case AST_UNARY: {
            if (n->child_count != 1 || !n->text) return NULL;
            const char *operand = expr_type(s, n->children[0]);
            if (!operand) return NULL;
            if (strcmp(n->text, "-") == 0) {
                if (!is_numeric(operand)) {
                    error_type(s, "unary '-' requires an int or float operand");
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

static int check_stmt(Semantic *s, AstNode *n) {
    switch (n->kind) {
        case AST_LET: {
            const char *actual = expr_type_expected(s, n->children[0], n->type_name);
            if (!actual) return 0;
            if (!assignable(n->type_name, actual)) {
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
            const char *base = find_var(s, n->text);
            if (!base) {
                fprintf(stderr, "semantic error: assignment to undefined variable '%s'\n", n->text);
                s->had_error = 1;
                return 0;
            }

            const char *index = expr_type(s, n->children[0]);

            if (is_list_type(base)) {
                if (!index || strcmp(index, "int") != 0) {
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
        case AST_LIST_APPEND: {
            const char *base = find_var(s, n->text);
            if (!base) {
                fprintf(stderr, "semantic error: append on undefined variable '%s'\n", n->text);
                s->had_error = 1;
                return 0;
            }
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
            if (strcmp(type, "int") != 0 && strcmp(type, "float") != 0 &&
                strcmp(type, "bool") != 0 && strcmp(type, "string") != 0 &&
                strcmp(type, "int128") != 0 &&
                strcmp(type, "uint128") != 0 &&
                strcmp(type, "float128") != 0) {
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
                if (!type || strcmp(type, "int") != 0) {
                    error_type(s, "range() arguments must be int");
                    return 0;
                }
            }
            enter_scope(s);
            int ok = add_var(s, n->text, "int") && check_block(s, n->children[n->child_count - 1], 0);
            leave_scope(s);
            return ok;
        }
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
