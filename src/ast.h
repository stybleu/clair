#ifndef CLAIR_AST_H
#define CLAIR_AST_H
#include <stddef.h>

typedef enum {
    AST_PROGRAM,
    AST_CLASS,
    AST_FIELD,
    AST_FUNCTION,
    AST_PARAM,
    AST_BLOCK,
    AST_LET,
    AST_RETURN,
    AST_PRINT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FOR_EACH,       /* for v in list { } ; text: v, type_name: the list type ; children: list, body */
    AST_BREAK,
    AST_CONTINUE,
    AST_ASSIGN,
    AST_INDEX_ASSIGN,
    AST_MATRIX_SET,     /* name[i][j] = value ; children: i, j, value */
    AST_LIST_APPEND,
    AST_CALL,
    AST_METHOD_CALL,
    AST_EXPR_STMT,
    AST_CONSTRUCT,
    AST_MEMBER,
    AST_MEMBER_ASSIGN,
    AST_INTEGER,
    AST_FLOAT,
    AST_BOOL,
    AST_STRING,
    AST_LIST,
    AST_DICT,
    AST_INDEX,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY
} AstKind;

typedef struct AstNode {
    AstKind kind;
    char *text;
    char *type_name;
    struct AstNode **children;
    size_t child_count;
    size_t child_capacity;
} AstNode;

AstNode *ast_new(AstKind kind, const char *text, size_t text_len);
int ast_add_child(AstNode *parent, AstNode *child);
int ast_set_type(AstNode *node, const char *type_name, size_t type_len);
void ast_free(AstNode *node);

#endif
