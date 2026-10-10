#ifndef CLAIR_CODEGEN_C_H
#define CLAIR_CODEGEN_C_H
#include "ast.h"
int codegen_c_file(const AstNode *program, const char *path);
#endif
