#ifndef CLAIR_SEMANTIC_H
#define CLAIR_SEMANTIC_H

#include "ast.h"

int semantic_check(AstNode *program);

/* Renames variables/fields whose spelling would clash with C (call after semantic_check). */
void semantic_mangle_c_identifiers(AstNode *program);

#endif
