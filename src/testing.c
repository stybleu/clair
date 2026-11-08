/*
 * V0.59: the tests written in the program:
 *
 *   test "carre de 3" {
 *       assert_equal(carre(3), 9)
 *       assert(carre(-2) > 0, "un carre est positif")
 *   }
 *
 * The parser makes each block a function __test_N() -> int, its name kept in
 * its decorators.  clair prog.cl -o prog: the tests are taken out.
 * clair --test prog.cl -o tests: main is taken out, and the main written by
 * code generation runs each test in its own process: a test that fails (an
 * assert, any runtime error, a crash, more than 10 s) does not stop the others.
 */
#include "testing.h"
#include "codegen_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int testing_is_test(const AstNode *fn) {
    return fn && fn->kind == AST_FUNCTION && fn->decorators && fn->decorators->child_count == 1 &&
           fn->decorators->children[0]->kind == AST_STRING;
}

int testing_prepare(AstNode *program, int mode) {
    int test_mode = mode == 1, check_mode = mode == 2;
    size_t count = 0;
    for (size_t i = 0; i < program->child_count; i++) count += testing_is_test(program->children[i]);
    for (size_t i = 0; i < program->child_count; i++) {
        if (!testing_is_test(program->children[i])) continue;
        const char *a = program->children[i]->decorators->children[0]->text;
        for (size_t j = i + 1; j < program->child_count; j++) {
            if (testing_is_test(program->children[j]) && strcmp(a, program->children[j]->decorators->children[0]->text) == 0) {
                fprintf(stderr, "test error: two tests are named \"%s\" (line %zu): give each test its own name\n",
                        a, program->children[j]->line);
                return 0;
            }
        }
    }
    if (test_mode && count == 0) {
        fprintf(stderr, "test error: this program has no test: write test \"its name\" { assert(...) }\n");
        return 0;
    }
    const char **names = test_mode ? calloc(count, sizeof(char *)) : NULL;
    const char **fns = test_mode ? calloc(count, sizeof(char *)) : NULL;
    if (test_mode && (!names || !fns)) { free(names); free(fns); return 0; }
    size_t out = 0, k = 0;
    for (size_t i = 0; i < program->child_count; i++) {
        AstNode *c = program->children[i];
        int is_test = testing_is_test(c);
        int is_main = c->kind == AST_FUNCTION && c->text && strcmp(c->text, "main") == 0;
        if (check_mode) {
            /* clair --check: everything stays, the tests are checked like any function */
            if (is_test) {
                ast_free(c->decorators);
                c->decorators = NULL;
            }
            program->children[out++] = c;
            continue;
        }
        if (test_mode ? is_main : is_test) {
            ast_free(c);
            continue;
        }
        if (is_test) {
            /* the names live as long as the program: they are taken from the decorators */
            AstNode *name = c->decorators->children[0];
            c->decorators->child_count = 0;
            ast_free(c->decorators);
            c->decorators = NULL;
            names[k] = name->text;
            name->text = NULL;
            ast_free(name);
            fns[k++] = strdup(c->text);
        }
        program->children[out++] = c;
    }
    program->child_count = out;
    if (test_mode) codegen_set_tests(names, fns, count);
    return 1;
}
