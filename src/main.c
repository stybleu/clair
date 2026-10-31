#include "parser.h"
#include "codegen_c.h"
#include "semantic.h"
#include "modules.h"
#include "units.h"
#include "dce.h"
#include "ad.h"
#include "ad_reverse.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static char *read_file(const char *path) {
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

/*
 * Floating point: by default every + - * / is rounded on its own, like Python.
 * Clang may otherwise fuse a * b + c into one instruction (FMA) where it wants,
 * so the same formula could give a different last digit in two loops or on two
 * machines.  --fma allows that fusion (a little faster on ARM).
 */
static int allow_fma = 0;

static int run_clang(const char *cpath, const char *output) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("clair: fork");
        return 1;
    }

    if (pid == 0) {
        char *const args[] = {
            "clang",
            "-std=c11",
            "-O2",
            "-fwrapv",
            "-Wall",
            "-Wextra",
            "-Wno-unused-function",
            "-Wno-implicit-const-int-float-conversion",   /* let x: float32 = 16777217 rounds, like NumPy */
            "-fno-math-errno",   /* Clair never reads errno: lets Clang vectorize sqrt() and friends */
            allow_fma ? "-ffp-contract=on" : "-ffp-contract=off",
            "-falign-loops=64",  /* hot loops start on a 64-byte line: same speed whatever the code layout */
            (char *)cpath,
            "-o",
            (char *)output,
            "-lm",
            "-pthread",          /* big matrix products and dense layers use every core */
            NULL
        };
        execvp("clang", args);
        perror("clair: clang");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("clair: waitpid");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "clair: clang failed\n");
        return 1;
    }
    return 0;
}

/* DCE V7.2 : suppression conservative de declarations
   locales int dont l'initialiseur est un entier litteral.
   Aucune suppression de calcul, appel ou effet de bord. */
static int clair_v7_name_elsewhere(
    const AstNode *n,
    const AstNode *declaration,
    const char *name
) {
    if (!n) return 0;

    if (
        n != declaration &&
        n->text &&
        strcmp(n->text, name) == 0
    ) return 1;

    for (size_t i = 0; i < n->child_count; ++i) {
        if (clair_v7_name_elsewhere(
            n->children[i], declaration, name
        )) return 1;
    }
    return 0;
}


/* V7.3 : expression entiere prouvee constante et sure.
   Les divisions et appels sont volontairement exclus. */
static int clair_v73_safe_int_expr(
    const AstNode *n, int64_t *out
) {
    if (!n || !out) return 0;

    if (n->kind == AST_INTEGER && n->text) {
        char *end = NULL;
        errno = 0;
        long long value = strtoll(n->text, &end, 10);

        if (errno || end == n->text || *end != '\0')
            return 0;

        *out = (int64_t)value;
        return 1;
    }

    if (n->kind == AST_UNARY &&
        n->text && n->child_count == 1) {
        int64_t value;
        if (!clair_v73_safe_int_expr(
            n->children[0], &value
        )) return 0;

        if (strcmp(n->text, "+") == 0) {
            *out = value;
            return 1;
        }

        if (strcmp(n->text, "-") == 0 &&
            value != INT64_MIN) {
            *out = -value;
            return 1;
        }

        return 0;
    }

    if (n->kind == AST_BINARY &&
        n->text && n->child_count == 2) {
        int64_t a, b, result;

        if (!clair_v73_safe_int_expr(
            n->children[0], &a
        )) return 0;

        if (!clair_v73_safe_int_expr(
            n->children[1], &b
        )) return 0;

        int overflow = 1;

        if (strcmp(n->text, "+") == 0)
            overflow = __builtin_add_overflow(
                a, b, &result
            );
        else if (strcmp(n->text, "-") == 0)
            overflow = __builtin_sub_overflow(
                a, b, &result
            );
        else if (strcmp(n->text, "*") == 0)
            overflow = __builtin_mul_overflow(
                a, b, &result
            );

        if (overflow) return 0;

        *out = result;
        return 1;
    }

    return 0;
}

static int clair_v7_prune_literals(AstNode *program) {
    int removed = 0;

    if (!program || program->kind != AST_PROGRAM)
        return 0;

    for (size_t f = 0; f < program->child_count; ++f) {
        AstNode *fn = program->children[f];

        if (
            !fn ||
            fn->kind != AST_FUNCTION ||
            !fn->text ||
            strcmp(fn->text, "main") != 0 ||
            !fn->child_count
        ) continue;

        AstNode *body =
            fn->children[fn->child_count - 1];

        if (!body || body->kind != AST_BLOCK)
            continue;

        size_t write = 0;
        size_t count = body->child_count;

        for (size_t i = 0; i < count; ++i) {
            AstNode *n = body->children[i];
            int64_t clair_v73_constant = 0;
            int dead = (
                n &&
                n->kind == AST_LET &&
                n->text &&
                n->type_name &&
                strcmp(n->type_name, "int") == 0 &&
                n->child_count == 1 &&
                n->children[0] &&
                clair_v73_safe_int_expr(n->children[0], &clair_v73_constant) &&
                !clair_v7_name_elsewhere(
                    program, n, n->text
                )
            );

            if (dead) {
                ast_free(n);
                ++removed;
            } else {
                body->children[write++] = n;
            }
        }
        body->child_count = write;
    }
    return removed;
}


/* V7.5 : elimination conservative des methodes et classes.
   Toute fonction ordinaire est une racine.
   Les appels sont deja resolus par l'analyse semantique. */

static int clair_v75_is_method(
    const AstNode *fn, const AstNode *program
) {
    if (!fn || fn->kind != AST_FUNCTION || !fn->text)
        return 0;

    for (size_t i = 0; i < program->child_count; ++i) {
        const AstNode *cls = program->children[i];
        if (!cls || cls->kind != AST_CLASS || !cls->text)
            continue;

        size_t len = strlen(cls->text);

        if (
            strncmp(fn->text, cls->text, len) == 0 &&
            fn->text[len] == '_' &&
            fn->text[len + 1] == '_' &&
            fn->text[len + 2] != '\0'
        ) return 1;
    }
    return 0;
}

static int clair_v75_calls(
    const AstNode *n, const char *name
) {
    if (!n) return 0;

    if (
        n->kind == AST_CALL &&
        n->text &&
        strcmp(n->text, name) == 0
    ) return 1;

    for (size_t i = 0; i < n->child_count; ++i) {
        if (clair_v75_calls(n->children[i], name))
            return 1;
    }
    return 0;
}

static int clair_v75_class_referenced(
    const AstNode *n, const char *name
) {
    if (!n) return 0;

    /* Conservateur : une mention dans un type
       suffit pour proteger la classe. */
    if (
        n->type_name &&
        strstr(n->type_name, name) != NULL
    ) return 1;

    if (
        n->kind == AST_CONSTRUCT &&
        n->text &&
        strcmp(n->text, name) == 0
    ) return 1;

    for (size_t i = 0; i < n->child_count; ++i) {
        if (clair_v75_class_referenced(
            n->children[i], name
        )) return 1;
    }
    return 0;
}

static void clair_v75_prune(
    AstNode *program
) {
    if (!program || program->kind != AST_PROGRAM)
        return;

    size_t count = program->child_count;
    unsigned char *live = calloc(count, 1);

    if (!live) return; /* Echec allocation : ne rien enlever */

    /* Toutes les fonctions ordinaires sont conservees
       comme points de depart de l'analyse. */
    for (size_t i = 0; i < count; ++i) {
        const AstNode *fn = program->children[i];
        if (
            fn &&
            fn->kind == AST_FUNCTION &&
            !clair_v75_is_method(fn, program)
        ) live[i] = 1;
    }

    /* Point fixe du graphe des appels.
       Une methode peut appeler une autre methode. */
    int changed = 1;

    while (changed) {
        changed = 0;

        for (size_t i = 0; i < count; ++i) {
            const AstNode *target = program->children[i];

            if (
                !target ||
                target->kind != AST_FUNCTION ||
                live[i] ||
                !clair_v75_is_method(target, program)
            ) continue;

            for (size_t j = 0; j < count; ++j) {
                const AstNode *caller = program->children[j];

                if (
                    live[j] &&
                    caller &&
                    caller->kind == AST_FUNCTION &&
                    clair_v75_calls(caller, target->text)
                ) {
                    live[i] = 1;
                    changed = 1;
                    break;
                }
            }
        }
    }

    /* Retirer les methodes non accessibles. */
    size_t write = 0;

    for (size_t i = 0; i < count; ++i) {
        AstNode *n = program->children[i];

        if (
            n &&
            n->kind == AST_FUNCTION &&
            !live[i]
        ) {
            ast_free(n);
        } else {
            program->children[write++] = n;
        }
    }

    program->child_count = write;
    free(live);

    /* Supprimer uniquement les classes sans usage
       ni methode conservee. Recommencer pour permettre
       de retirer des dependances devenues inutiles. */
    changed = 1;

    while (changed) {
        changed = 0;
        count = program->child_count;

        for (size_t i = 0; i < count; ++i) {
            AstNode *cls = program->children[i];

            if (
                !cls ||
                cls->kind != AST_CLASS ||
                !cls->text
            ) continue;

            int referenced = 0;

            for (size_t j = 0; j < count; ++j) {
                if (i == j) continue;

                const AstNode *other = program->children[j];

                if (
                    other->kind == AST_FUNCTION &&
                    other->text &&
                    strncmp(
                        other->text,
                        cls->text,
                        strlen(cls->text)
                    ) == 0 &&
                    other->text[strlen(cls->text)] == '_' &&
                    other->text[strlen(cls->text) + 1] == '_'
                ) {
                    referenced = 1;
                    break;
                }

                if (clair_v75_class_referenced(
                    other, cls->text
                )) {
                    referenced = 1;
                    break;
                }
            }

            if (!referenced) {
                ast_free(cls);

                for (size_t j = i + 1; j < count; ++j)
                    program->children[j - 1] =
                        program->children[j];

                program->child_count--;
                changed = 1;
                break;
            }
        }
    }
}



int main(int argc, char **argv) {
    const char *input = NULL, *output = NULL;
    int unchecked = 0;
    int dce_enabled = 0;
    int ad_enabled = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--sans-verif") == 0 || strcmp(argv[i], "--unchecked") == 0) unchecked = 1;
        else if (strcmp(argv[i], "--ad") == 0) ad_enabled = 1;
        else if (strcmp(argv[i], "--dce") == 0) dce_enabled = 1;
        else if (strcmp(argv[i], "--fma") == 0) allow_fma = 1;
        else if (!input && argv[i][0] != '-') input = argv[i];
        else { input = NULL; break; }
    }
    if (!input || !output) {
        fprintf(stderr, "usage: %s input.cl -o output [--sans-verif] [--fma] [--dce] [--ad]\n", argv[0]);
        return 2;
    }
    codegen_set_unchecked(unchecked);

    char *source = read_file(input);
    if (!source) {
        fprintf(stderr, "clair: cannot read %s\n", input);
        return 1;
    }

    Parser parser;
    ast_current_file = NULL;
    parser_init(&parser, source);
    AstNode *program = parser_parse_program(&parser);
    ast_current_line = 0;                 /* V0.49: nodes made by the compiler have no line */
    if (!program || parser.had_error) {
        ast_free(program);
        free(source);
        return 1;
    }

    /* V0.47: import math as m, import orbite (orbite.cl next to the program)... */
    if (!modules_resolve(program, input)) {
        ast_free(program);
        free(source);
        return 1;
    }

    if (ad_enabled && !clair_ad_reverse_generate(program)) {
        ast_free(program);
        free(source);
        return 1;
    }
    if (ad_enabled && !clair_ad_generate(program)) {
        ast_free(program);
        free(source);
        return 1;
    }
    if (!semantic_check(program)) {
        ast_free(program);
        free(source);
        return 1;
    }

    /* V0.48: float[m], float[m/s]... checked here, then forgotten */
    if (!units_check(program)) {
        ast_free(program);
        free(source);
        return 1;
    }
    semantic_mangle_c_identifiers(program);
    if (dce_enabled) {
        (void)clair_dce_functions(program);
        (void)clair_v7_prune_literals(program);
        clair_v75_prune(program);
    }

    size_t cpath_len = strlen(output) + strlen(".generated.c") + 1;
    char *cpath = malloc(cpath_len);
    if (!cpath) {
        ast_free(program);
        free(source);
        return 1;
    }
    snprintf(cpath, cpath_len, "%s.generated.c", output);

    if (!codegen_c_file(program, cpath)) {
        fprintf(stderr, "clair: C generation failed\n");
        free(cpath);
        ast_free(program);
        free(source);
        return 1;
    }

    int rc = run_clang(cpath, output);

    free(cpath);
    ast_free(program);
    free(source);
    return rc;
}
