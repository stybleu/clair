#ifndef CLAIR_TESTING_H
#define CLAIR_TESTING_H

#include "ast.h"

/*
 * V0.59: the blocks test "name" { ... } of a program.  Without --test they are
 * taken out (the program does not change).  With --test, the program's main is
 * taken out instead, and code generation writes a main that runs every test
 * in its own process (codegen_set_tests).  With --check (mode 2), the tests and
 * main are both kept, so that both are checked.  Returns 0 after an error.
 */
int testing_prepare(AstNode *program, int test_mode);

/* a function made from a test block (its decorators hold the name) */
int testing_is_test(const AstNode *fn);

#endif
