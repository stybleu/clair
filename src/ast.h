#ifndef CLAIR_AST_H
#define CLAIR_AST_H
#include <stddef.h>
#include <stdint.h>

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
    AST_UNARY,
    AST_IMPORT          /* V0.47: import m [as a] (type_name NULL, child: the alias) or
                           from m import x [as y], ... (type_name "from", children: x, each with its alias);
                           resolved and removed by modules_resolve before the semantic pass */
} AstKind;

typedef struct AstNode {
    AstKind kind;
    char *text;
    char *type_name;
    struct AstNode **children;
    size_t child_count;
    size_t child_capacity;
    char *unit;          /* V0.48: the unit written (float[m/s], 9.81[m/s^2]), canonical ("m^1 s^-1"), or NULL */
    size_t line;         /* V0.49: the line of the program where the node was read (0: made by the compiler) */
    const char *file;    /* V0.49: NULL for the program, "orbite.cl" for a module */
    const char *measure_src;   /* V0.53: a constant of phys known to ±: its sources, "2:1 1:-2" (static text) */
} AstNode;

/* V0.49: given to every new node; the parser keeps them up to date */
extern size_t ast_current_line;
extern const char *ast_current_file;

AstNode *ast_new(AstKind kind, const char *text, size_t text_len);
int ast_add_child(AstNode *parent, AstNode *child);
int ast_set_type(AstNode *node, const char *type_name, size_t type_len);
void ast_free(AstNode *node);
/* deep copy (x[i] += v is written x[i] = x[i] + v) */
AstNode *ast_clone(const AstNode *node);

/*
 * Integer literal text (decimal digits, optionally after a '-') to `words`
 * 64-bit words, least significant first, two's complement.  Returns 1 when
 * the value fits in a signed integer of 64 * words bits (words <= 16), or,
 * with is_unsigned, in 0 .. 2^(64 * words) - 1.
 */
int clair_decimal_to_words(const char *text, uint64_t *out, int words, int is_unsigned);

/*
 * An integer as written (12, 1_000, 0xFF, 0b1010, 0o17) to plain decimal
 * digits.  Returns 0 for a malformed or absurdly large number.
 */
int clair_integer_to_decimal(const char *text, size_t len, char *out, size_t out_size);

#endif
