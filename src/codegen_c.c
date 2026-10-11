#include "codegen_c.h"
#include <stdio.h>
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


static const char *c_type_name(const char *clair_type) {
    static char class_type[512];

    if (!clair_type) return NULL;
    if (strcmp(clair_type, "int128") == 0) return "__int128";
    if (strcmp(clair_type, "uint128") == 0)
        return "unsigned __int128";
    if (strcmp(clair_type, "float128") == 0)
        return "long double";
    if (strcmp(clair_type, "int") == 0) return "int64_t";
    if (strcmp(clair_type, "float") == 0) return "double";
    if (strcmp(clair_type, "bool") == 0) return "bool";
    if (strcmp(clair_type, "string") == 0) return "const char *";
    if (is_list_type(clair_type)) return "ClairList";
    if (is_dict_type(clair_type)) return "ClairDict";

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

static int emit_expr(FILE *f, const AstNode *n) {
    switch (n->kind) {
        case AST_INTEGER:
            if (n->type_name &&
                (strcmp(n->type_name, "int128") == 0 ||
                 strcmp(n->type_name, "uint128") == 0)) {

                int signed_type =
                    strcmp(n->type_name, "int128") == 0;

                if (signed_type)
                    fputs("((__int128)(", f);
                else
                    fputc('(', f);

                size_t count = strlen(n->text);

                for (size_t i = 0; i < count; ++i)
                    fputc('(', f);

                fputs("(unsigned __int128)0", f);

                for (size_t i = 0; i < count; ++i) {
                    char digit = n->text[i];
                    if (digit < '0' || digit > '9')
                        return 0;
                    fprintf(f, " * 10 + %c)", digit);
                }

                fputc(')', f);
                if (signed_type) fputc(')', f);
                return 1;
            }

            const char *digits = n->text;

            while (*digits == '0' && digits[1])
                ++digits;

            fputs(digits, f);
            return 1;

        case AST_FLOAT:
            fputs(n->text, f);
            if (n->type_name &&
                strcmp(n->type_name, "float128") == 0)
                fputc('L', f);
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
            const char *element_type = c_type_name(n->type_name);
            if (!element_type || is_list_type(n->type_name) || is_dict_type(n->type_name)) return 0;

            if (is_list_type(n->children[0]->type_name)) {
                fprintf(f, "(*(%s *)%s(", element_type,
                         strcmp(element_type, "int64_t") == 0
    ? "clair_rt_list_at_i64_checked"
    : (strcmp(element_type, "double") == 0
        ? "clair_rt_list_at_f64_checked"
        : "clair_rt_list_at"));
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fputs("))", f);
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
            fputs("((", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fprintf(f, ")->%s)", n->text);
            return 1;

        case AST_CALL:
            if (n->text && strcmp(n->text, "input_int") == 0) {
                if (n->child_count != 0) return 0;
                fputs("clair_rt_input_int()", f);
                return 1;
            }
            if (n->text && strcmp(n->text, "len") == 0) {
                if (n->child_count != 1 || !n->children[0]->type_name) return 0;
                if (strcmp(n->children[0]->type_name, "string") == 0) {
                    fputs("((int64_t)strlen(", f);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs("))", f);
                    return 1;
                }
                if (is_list_type(n->children[0]->type_name)) {
                    fputs("((int64_t)(", f);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs(")->len)", f);
                    return 1;
                }
                if (is_dict_type(n->children[0]->type_name)) {
                    fputs("((int64_t)(", f);
                    if (!emit_expr(f, n->children[0])) return 0;
                    fputs(")->len)", f);
                    return 1;
                }
                return 0;
            }
            fprintf(f, "%s(", n->text);
            for (size_t i = 0; i < n->child_count; i++) {
                if (i != 0) fputs(", ", f);
                if (!emit_expr(f, n->children[i])) return 0;
            }
            fputc(')', f);
            return 1;

        case AST_BINARY: {
            if (n->child_count != 2 || !n->text) return 0;
            const char *op = n->text;

            const char *left_type =
                n->children[0]->type_name;
            const char *right_type =
                n->children[1]->type_name;

            if (left_type && right_type &&
                strcmp(left_type, right_type) == 0 &&
                (strcmp(left_type, "int128") == 0 ||
                 strcmp(left_type, "uint128") == 0 ||
                 strcmp(left_type, "float128") == 0)) {

                int signed128 =
                    strcmp(left_type, "int128") == 0;
                int floating128 =
                    strcmp(left_type, "float128") == 0;

                int arithmetic =
                    strcmp(op, "+") == 0 ||
                    strcmp(op, "-") == 0 ||
                    strcmp(op, "*") == 0 ||
                    strcmp(op, "/") == 0;

                if (arithmetic && !floating128) {
                    const char *ctype = signed128 ?
                        "__int128" : "unsigned __int128";

                    fprintf(f,
                        "({ %s __clair_v2_a = (", ctype);

                    if (!emit_expr(f, n->children[0]))
                        return 0;

                    fprintf(f,
                        "); %s __clair_v2_b = (", ctype);

                    if (!emit_expr(f, n->children[1]))
                        return 0;

                    fputs("); ", f);

                    if (strcmp(op, "/") == 0) {
                        fputs(
                            "if (__clair_v2_b == 0", f);

                        if (signed128) {
                            fputs(
                                " || (__clair_v2_a == "
                                "(-(((__int128)1 << 126)) - "
                                "((__int128)1 << 126)) && "
                                "__clair_v2_b == -1)", f);
                        }

                        fputs(
                            ") { "
                            "fputs(\"clair runtime error: "
                            "128-bit division\\n\", stderr); "
                            "exit(1); } "
                            "(__clair_v2_a / __clair_v2_b); })",
                            f);

                        return 1;
                    }

                    const char *builtin =
                        strcmp(op, "+") == 0 ? "add" :
                        strcmp(op, "-") == 0 ? "sub" :
                        "mul";

                    fprintf(f,
                        "%s __clair_v2_result; "
                        "if (__builtin_%s_overflow("
                        "__clair_v2_a, __clair_v2_b, "
                        "&__clair_v2_result)) { ",
                        ctype, builtin);

                    fputs(
                        "fputs(\"clair runtime error: "
                        "128-bit overflow\\n\", stderr); "
                        "exit(1); } "
                        "__clair_v2_result; })", f);

                    return 1;
                }

                fputc('(', f);

                if (!emit_expr(f, n->children[0]))
                    return 0;

                fprintf(f, " %s ", op);

                if (!emit_expr(f, n->children[1]))
                    return 0;

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
                fputs("(strcmp(", f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputs(", ", f);
                if (!emit_expr(f, n->children[1])) return 0;
                fprintf(f, ") %s 0)", strcmp(op, "==") == 0 ? "==" : "!=");
                return 1;
            }

            if (strcmp(op, "and") == 0) op = "&&";
            else if (strcmp(op, "or") == 0) op = "||";

            fputc('(', f);
            if (!emit_expr(f, n->children[0])) return 0;
            fprintf(f, " %s ", op);
            if (!emit_expr(f, n->children[1])) return 0;
            fputc(')', f);
            return 1;
        }

        case AST_UNARY:
            if (n->text &&
                strcmp(n->text, "-") == 0 &&
                n->child_count == 1 &&
                n->children[0] &&
                n->children[0]->kind == AST_INTEGER &&
                n->children[0]->text &&
                n->type_name) {

                const char *digits =
                    n->children[0]->text;

                while (*digits == '0' && digits[1])
                    ++digits;

                if (strcmp(n->type_name, "int") == 0 &&
                    strcmp(digits,
                        "9223372036854775808") == 0) {

                    fputs(
                        "(-9223372036854775807LL - 1LL)",
                        f);
                    return 1;
                }

                if (strcmp(n->type_name, "int128") == 0 &&
                    strcmp(digits,
                        "170141183460469231731687303715884105728") == 0) {

                    fputs(
                        "(-(((__int128)1) << 126) - "
                        "(((__int128)1) << 126))",
                        f);
                    return 1;
                }
            }

            if (n->type_name &&
                strcmp(n->type_name, "int128") == 0 &&
                n->text && strcmp(n->text, "-") == 0 &&
                n->child_count == 1 &&
                n->children[0] &&
                n->children[0]->kind == AST_INTEGER &&
                n->children[0]->text &&
                strcmp(n->children[0]->text,
                    "170141183460469231731687303715884105728") == 0) {
                fputs(
                    "(-(((__int128)1) << 126) - "
                    "(((__int128)1) << 126))", f);
                return 1;
            }
 {
            if (n->child_count != 1 || !n->text) return 0;
            const char *op = strcmp(n->text, "not") == 0 ? "!" : n->text;
            fputc('(', f);
            fputs(op, f);
            if (!emit_expr(f, n->children[0])) return 0;
            fputc(')', f);
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

        case AST_MEMBER_ASSIGN:
            if (n->child_count != 2 || !n->text) return 0;
            fputs("((", f);
            if (!emit_expr(f, n->children[0])) return 0;
            fprintf(f, ")->%s) = ", n->text);
            if (!emit_expr(f, n->children[1])) return 0;
            fputs(";\n", f);
            return 1;

        case AST_INDEX_ASSIGN: {
            if (!n->type_name) return 0;
            size_t id = list_mutation_counter++;

            if (is_list_type(n->type_name)) {
                const char *element_type = list_element_c_type(n->type_name);
                if (!element_type) return 0;
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
                fprintf(f, "*(%s *)%s(%s, __clair_list_index_%zu) = __clair_list_value_%zu;\n",
        element_type,
        strcmp(element_type, "double") == 0
            ? "clair_rt_list_at_f64_checked"
            : "clair_rt_list_at",
        n->text, id, id);
                indent(f, depth);
                fputs("}\n", f);
                return 1;
            }

            if (is_dict_type(n->type_name)) {
                const char *value_type = dict_value_c_type(n->type_name);
                if (!value_type) return 0;
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
                fprintf(f,
                        "*(%s *)clair_rt_dict_set_slot(%s, __clair_dict_key_%zu) = __clair_dict_value_%zu;\n",
                        value_type, n->text, id, id);
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
            fprintf(f, "*(%s *)clair_rt_list_append_slot(%s) = __clair_list_append_value_%zu;\n",
                    element_type, n->text, id);
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
            if (strcmp(value->type_name, "int128") == 0 ||
                strcmp(value->type_name, "uint128") == 0) {

                int signed_type =
                    strcmp(value->type_name, "int128") == 0;

                if (signed_type)
                    fputs("{ __int128 __clair_v = ", f);
                else
                    fputs("{ unsigned __int128 __clair_v = ", f);

                if (!emit_expr(f, value)) return 0;

                if (signed_type) {
                    fputs(
                        "; unsigned __int128 __clair_mag = "
                        "(__clair_v < 0) ? "
                        "((unsigned __int128)(-(__clair_v + 1)) + 1) : "
                        "(unsigned __int128)__clair_v;\n", f);
                } else {
                    fputs(
                        "; unsigned __int128 __clair_mag = __clair_v;\n",
                        f);
                }

                fputs(
                    "char __clair_digits[40]; "
                    "size_t __clair_count = 0;\n"
                    "do { "
                    "__clair_digits[__clair_count++] = "
                    "(char)('0' + (__clair_mag % 10)); "
                    "__clair_mag /= 10; "
                    "} while (__clair_mag);\n", f);

                if (signed_type)
                    fputs(
                        "if (__clair_v < 0) putchar('-');\n", f);

                fputs(
                    "while (__clair_count) "
                    "putchar(__clair_digits[--__clair_count]);\n"
                    "putchar('\\n'); }\n", f);

            } else if (strcmp(value->type_name, "float128") == 0) {
                fputs("printf(\"%.36Lg\\n\", (long double)(", f);
                if (!emit_expr(f, value)) return 0;
                fputs("));\n", f);
            } else if (strcmp(value->type_name, "string") == 0) {
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
                    fputs("while ", f);

                    if (!emit_expr(f, n->children[0]))
                        return 0;

                    fputs(" {\n", f);

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
            fputs("while ", f);
            if (n->children[0]->kind == AST_BINARY) {
                if (!emit_expr(f, n->children[0])) return 0;
            } else {
                fputc('(', f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputc(')', f);
            }
            fputs(" {\n", f);
            if (!emit_block(f, n->children[1], depth + 1)) return 0;
            indent(f, depth);
            fputs("}\n", f);
            return 1;

        case AST_FOR: {
            if (!n->text || n->child_count < 2 || n->child_count > 4) return 0;

            size_t argc = n->child_count - 1;
            const AstNode *body = n->children[n->child_count - 1];
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
            fputs("if ", f);
            if (n->children[0]->kind == AST_BINARY) {
                if (!emit_expr(f, n->children[0])) return 0;
            } else {
                fputc('(', f);
                if (!emit_expr(f, n->children[0])) return 0;
                fputc(')', f);
            }
            fputs(" {\n", f);
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
        fprintf(f, "%s %s(", return_type, fn->text);
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
    if (n->kind == AST_LIST || n->kind == AST_INDEX || n->kind == AST_INDEX_ASSIGN || n->kind == AST_LIST_APPEND ||
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

    fputs(
        "#include <stdint.h>\n"
        "#include <stdbool.h>\n"
        "#include <inttypes.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "\n"
        "static int64_t clair_rt_int_div_checked(int64_t a, int64_t b) {\n"
        "    if (b == 0) {\n"
        "        fputs(\"Clair runtime error: division by zero\\n\", stderr);\n"
        "        exit(EXIT_FAILURE);\n"
        "    }\n"
        "\n"
        "    if (a == INT64_MIN && b == -1) {\n"
        "        fputs(\"Clair runtime error: integer division overflow\\n\", stderr);\n"
        "        exit(EXIT_FAILURE);\n"
        "    }\n"
        "\n"
        "    return a / b;\n"
        "}\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <string.h>\n",
        f
    );
    if (uses_input_int) fputs("#include <errno.h>\n", f);
    fputc('\n', f);

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
                "static void *clair_rt_list_at(ClairList list, int64_t index) {\n"
                "    if (!list) { fputs(\"Clair runtime error: null list\\n\", stderr); exit(EXIT_FAILURE); }\n"
                "    if (index < 0 || (uint64_t)index >= (uint64_t)list->len) {\n"
                "        fprintf(stderr, \"Clair runtime error: list index %\" PRId64 \" out of bounds for length %zu\\n\", index, list->len);\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n"
                "    return (unsigned char *)list->data + ((size_t)index * list->elem_size);\n"
                "}\n\n",
                f
            );
        }

        if (uses_list_at) {
            fputs(
                "static int64_t *clair_rt_list_at_i64_checked(ClairList list, int64_t index) {\n"
                "    if (!list) {\n"
                "        fputs(\"Clair runtime error: null list\\n\", stderr);\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n"
                "    if (index < 0 || (uint64_t)index >= (uint64_t)list->len) {\n"
                "        fprintf(stderr, \"Clair runtime error: list index %\" PRId64 \" out of bounds for length %zu\\n\", index, list->len);\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n"
                "    return (int64_t *)list->data + (size_t)index;\n"
                "}\n"
                "\n",
                f
            );
        }

        if (uses_list_at) {
            fputs(
                "static double *clair_rt_list_at_f64_checked(ClairList list, int64_t index) {\n"
                "    if (!list) {\n"
                "        fputs(\"Clair runtime error: null list\\n\", stderr);\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n"
                "    if (index < 0 || (uint64_t)index >= (uint64_t)list->len) {\n"
                "        fprintf(stderr,\n"
                "            \"Clair runtime error: list index %\" PRId64\n"
                "            \" out of bounds for length %zu\\n\",\n"
                "            index, list->len);\n"
                "        exit(EXIT_FAILURE);\n"
                "    }\n"
                "    return (double *)list->data + (size_t)index;\n"
                "}\n"
                "\n",
                f
            );
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

    if (!emit_class_declarations(f, program)) {
        fclose(f);
        return 0;
    }

    if (node_uses_kind(program, AST_CONSTRUCT)) {
        fputs(
            "static void **clair_rt_objects = NULL;\n"
            "static size_t clair_rt_object_count = 0;\n"
            "static size_t clair_rt_object_capacity = 0;\n"
            "static int clair_rt_object_cleanup_registered = 0;\n\n"
            "static void clair_rt_cleanup_objects(void) {\n"
            "    for (size_t i = 0; i < clair_rt_object_count; i++) free(clair_rt_objects[i]);\n"
            "    free(clair_rt_objects);\n"
            "    clair_rt_objects = NULL;\n"
            "    clair_rt_object_count = 0;\n"
            "    clair_rt_object_capacity = 0;\n"
            "}\n\n"
            "static void clair_rt_object_runtime_error(const char *message, void *owned) {\n"
            "    free(owned);\n"
            "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
            "    exit(EXIT_FAILURE);\n"
            "}\n\n"
            "static void clair_rt_track_object(void *object) {\n"
            "    if (!clair_rt_object_cleanup_registered) {\n"
            "        if (atexit(clair_rt_cleanup_objects) != 0) {\n"
            "            clair_rt_object_runtime_error(\"cannot register object cleanup\", object);\n"
            "        }\n"
            "        clair_rt_object_cleanup_registered = 1;\n"
            "    }\n"
            "    if (clair_rt_object_count == clair_rt_object_capacity) {\n"
            "        size_t new_capacity = clair_rt_object_capacity ? clair_rt_object_capacity * 2 : 16;\n"
            "        if (new_capacity < clair_rt_object_capacity || new_capacity > SIZE_MAX / sizeof(*clair_rt_objects)) {\n"
            "            clair_rt_object_runtime_error(\"object allocation registry overflow\", object);\n"
            "        }\n"
            "        void **new_items = realloc(clair_rt_objects, new_capacity * sizeof(*new_items));\n"
            "        if (!new_items) clair_rt_object_runtime_error(\"out of memory while tracking objects\", object);\n"
            "        clair_rt_objects = new_items;\n"
            "        clair_rt_object_capacity = new_capacity;\n"
            "    }\n"
            "    clair_rt_objects[clair_rt_object_count++] = object;\n"
            "}\n\n"
            "static void *clair_rt_object_make(const void *value, size_t size) {\n"
            "    if (!value || size == 0) { fputs(\"Clair runtime error: invalid object construction\\n\", stderr); exit(EXIT_FAILURE); }\n"
            "    void *object = malloc(size);\n"
            "    if (!object) { fputs(\"Clair runtime error: out of memory while creating object\\n\", stderr); exit(EXIT_FAILURE); }\n"
            "    memcpy(object, value, size);\n"
            "    clair_rt_track_object(object);\n"
            "    return object;\n"
            "}\n\n",
            f
        );
    }

    if (node_uses_string_concat(program)) {
        fputs(
            "static char **clair_rt_strings = NULL;\n"
            "static size_t clair_rt_string_count = 0;\n"
            "static size_t clair_rt_string_capacity = 0;\n"
            "static int clair_rt_string_cleanup_registered = 0;\n\n"
            "static void clair_rt_cleanup_strings(void) {\n"
            "    for (size_t i = 0; i < clair_rt_string_count; i++) free(clair_rt_strings[i]);\n"
            "    free(clair_rt_strings);\n"
            "    clair_rt_strings = NULL;\n"
            "    clair_rt_string_count = 0;\n"
            "    clair_rt_string_capacity = 0;\n"
            "}\n\n"
            "static void clair_rt_string_runtime_error(const char *message, char *owned) {\n"
            "    free(owned);\n"
            "    fprintf(stderr, \"Clair runtime error: %s\\n\", message);\n"
            "    exit(EXIT_FAILURE);\n"
            "}\n\n"
            "static void clair_rt_track_string(char *value) {\n"
            "    if (!clair_rt_string_cleanup_registered) {\n"
            "        if (atexit(clair_rt_cleanup_strings) != 0) {\n"
            "            clair_rt_string_runtime_error(\"cannot register string cleanup\", value);\n"
            "        }\n"
            "        clair_rt_string_cleanup_registered = 1;\n"
            "    }\n"
            "    if (clair_rt_string_count == clair_rt_string_capacity) {\n"
            "        size_t new_capacity = clair_rt_string_capacity ? clair_rt_string_capacity * 2 : 16;\n"
            "        if (new_capacity < clair_rt_string_capacity || new_capacity > SIZE_MAX / sizeof(*clair_rt_strings)) {\n"
            "            clair_rt_string_runtime_error(\"string allocation registry overflow\", value);\n"
            "        }\n"
            "        char **new_items = realloc(clair_rt_strings, new_capacity * sizeof(*new_items));\n"
            "        if (!new_items) {\n"
            "            clair_rt_string_runtime_error(\"out of memory while tracking strings\", value);\n"
            "        }\n"
            "        clair_rt_strings = new_items;\n"
            "        clair_rt_string_capacity = new_capacity;\n"
            "    }\n"
            "    clair_rt_strings[clair_rt_string_count++] = value;\n"
            "}\n\n"
            "static const char *clair_rt_string_concat(const char *left, const char *right) {\n"
            "    size_t left_len = strlen(left);\n"
            "    size_t right_len = strlen(right);\n"
            "    if (right_len > SIZE_MAX - left_len - 1) {\n"
            "        fprintf(stderr, \"Clair runtime error: string concatenation overflow\\n\");\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    size_t total = left_len + right_len;\n"
            "    char *result = malloc(total + 1);\n"
            "    if (!result) {\n"
            "        fprintf(stderr, \"Clair runtime error: out of memory during string concatenation\\n\");\n"
            "        exit(EXIT_FAILURE);\n"
            "    }\n"
            "    memcpy(result, left, left_len);\n"
            "    memcpy(result + left_len, right, right_len);\n"
            "    result[total] = '\\0';\n"
            "    clair_rt_track_string(result);\n"
            "    return result;\n"
            "}\n\n",
            f
        );
    }

    range_counter = 0;
    list_mutation_counter = 0;

    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION) continue;
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
