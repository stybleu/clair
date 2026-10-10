#ifndef CLAIR_PARSER_H
#define CLAIR_PARSER_H
#include "lexer.h"
#include "ast.h"
#include <stddef.h>

typedef struct {
    Lexer lexer;
    Token current;
    int had_error;
    size_t loop_depth;
} Parser;

void parser_init(Parser *parser, const char *source);
AstNode *parser_parse_program(Parser *parser);

#endif
