#ifndef CLAIR_CODEGEN_C_H
#define CLAIR_CODEGEN_C_H
#include "ast.h"
int codegen_c_file(const AstNode *program, const char *path);

/* --sans-verif: no bounds checks on lists and matrices (division by zero is still checked). */
void codegen_set_unchecked(int unchecked);

/* V0.66: the program written by codegen_c_file imports gui: link it with -ldl (SDL3 is loaded when a window opens) */
int codegen_needs_dl(void);

/* V0.59: clair --test: the main written runs these test functions (names as written, C functions) */
void codegen_set_tests(const char **names, const char **fns, size_t count);
#endif
