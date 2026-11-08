
#include "dce.h"
#include <stdlib.h>
#include <string.h>

/* Désactiver le DCE pour les constructions complexes. */
static int dynamic_calls(const AstNode *node) {
    if (!node) return 0;

    if (node->kind == AST_CLASS ||
        node->kind == AST_METHOD_CALL ||
        node->kind == AST_CONSTRUCT)
        return 1;

    for (size_t i = 0; i < node->child_count; ++i)
        if (dynamic_calls(node->children[i]))
            return 1;

    return 0;
}

/* Identifier les fonctions appelées. */
static void mark_calls(
    const AstNode *node,
    const AstNode *program,
    unsigned char *live,
    int *changed
) {
    if (!node) return;

    if (node->kind == AST_CALL &&
        node->text &&
        node->text[0] != '@') {

        for (size_t i = 0; i < program->child_count; ++i) {
            const AstNode *fn = program->children[i];

            if (fn->kind == AST_FUNCTION &&
                fn->text &&
                strcmp(fn->text, node->text) == 0 &&
                !live[i]) {

                live[i] = 1;
                *changed = 1;
            }
        }
    }

    for (size_t i = 0; i < node->child_count; ++i)
        mark_calls(
            node->children[i],
            program,
            live,
            changed
        );
}

/* Supprimer les fonctions non accessibles depuis main. */
size_t clair_dce_functions(AstNode *program) {
    if (!program ||
        program->kind != AST_PROGRAM ||
        dynamic_calls(program))
        return 0;

    size_t n = program->child_count;

    unsigned char *live = calloc(n ? n : 1, 1);
    if (!live) return 0;

    int has_main = 0;

    for (size_t i = 0; i < n; ++i) {
        AstNode *node = program->children[i];

        if (node->kind != AST_FUNCTION || !node->text) {
            live[i] = 1;
        } else if (strcmp(node->text, "main") == 0) {
            live[i] = 1;
            has_main = 1;
        }
    }

    if (!has_main) {
        free(live);
        return 0;
    }

    int changed;

    do {
        changed = 0;

        for (size_t i = 0; i < n; ++i) {
            if (live[i] &&
                program->children[i]->kind == AST_FUNCTION) {

                mark_calls(
                    program->children[i],
                    program,
                    live,
                    &changed
                );
            }
        }
    } while (changed);

    size_t out = 0;
    size_t removed = 0;

    for (size_t i = 0; i < n; ++i) {
        if (live[i]) {
            program->children[out++] = program->children[i];
        } else {
            ast_free(program->children[i]);
            ++removed;
        }
    }

    program->child_count = out;
    free(live);

    return removed;
}
