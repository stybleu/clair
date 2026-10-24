#ifndef CLAIR_CODEGEN_C_H
#define CLAIR_CODEGEN_C_H
#include "ast.h"
int codegen_c_file(const AstNode *program, const char *path);

/* --sans-verif: no bounds checks on lists and matrices (division by zero is still checked). */
void codegen_set_unchecked(int unchecked);
#endif
