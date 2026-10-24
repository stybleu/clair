#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void next(Parser *p) { p->current = lexer_next(&p->lexer); }

static int expect(Parser *p, TokenKind kind, const char *what) {
    if (p->current.kind == kind) { next(p); return 1; }
    fprintf(stderr, "parse error at %zu:%zu: expected %s, got %s\n",
            p->current.line, p->current.column, what, token_kind_name(p->current.kind));
    p->had_error = 1;
    return 0;
}

static AstNode *parse_expression(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_function_impl(Parser *p, const char *owner, size_t owner_len);

static AstNode *make_binary(Token op, AstNode *left, AstNode *right) {
    AstNode *bin = ast_new(AST_BINARY, op.start, op.length);
    if (!bin) {
        ast_free(left);
        ast_free(right);
        return NULL;
    }
    if (!ast_add_child(bin, left)) {
        ast_free(bin);
        ast_free(left);
        ast_free(right);
        return NULL;
    }
    if (!ast_add_child(bin, right)) {
        ast_free(bin);
        ast_free(right);
        return NULL;
    }
    return bin;
}

static AstNode *parse_call_after_name(Parser *p, Token name) {
    AstNode *call = ast_new(AST_CALL, name.start, name.length);
    if (!call) return NULL;

    if (!expect(p, TOKEN_LPAREN, "'('") ) { ast_free(call); return NULL; }

    if (p->current.kind != TOKEN_RPAREN) {
        for (;;) {
            AstNode *arg = parse_expression(p);
            if (!arg) { ast_free(call); return NULL; }
            if (!ast_add_child(call, arg)) { ast_free(arg); ast_free(call); return NULL; }

            if (p->current.kind != TOKEN_COMMA) break;
            next(p);
        }
    }

    if (!expect(p, TOKEN_RPAREN, "')'")) { ast_free(call); return NULL; }
    return call;
}

static AstNode *parse_atom(Parser *p) {
    Token t = p->current;

    if (t.kind == TOKEN_INTEGER) {
        next(p);
        return ast_new(AST_INTEGER, t.start, t.length);
    }

    if (t.kind == TOKEN_FLOAT_LITERAL) {
        next(p);
        return ast_new(AST_FLOAT, t.start, t.length);
    }

    if (t.kind == TOKEN_TRUE || t.kind == TOKEN_FALSE) {
        next(p);
        return ast_new(AST_BOOL, t.start, t.length);
    }

    if (t.kind == TOKEN_STRING) {
        next(p);
        return ast_new(AST_STRING, t.start, t.length);
    }

    if (t.kind == TOKEN_LBRACKET) {
        next(p);
        AstNode *list = ast_new(AST_LIST, NULL, 0);
        if (!list) return NULL;

        if (p->current.kind != TOKEN_RBRACKET) {
            for (;;) {
                AstNode *item = parse_expression(p);
                if (!item) { ast_free(list); return NULL; }
                if (!ast_add_child(list, item)) { ast_free(item); ast_free(list); return NULL; }
                if (p->current.kind != TOKEN_COMMA) break;
                next(p);
            }
        }

        if (!expect(p, TOKEN_RBRACKET, "']'")) { ast_free(list); return NULL; }
        return list;
    }

    if (t.kind == TOKEN_LBRACE) {
        next(p);
        AstNode *dict = ast_new(AST_DICT, NULL, 0);
        if (!dict) return NULL;

        if (p->current.kind != TOKEN_RBRACE) {
            for (;;) {
                AstNode *key = parse_expression(p);
                if (!key) { ast_free(dict); return NULL; }
                if (!expect(p, TOKEN_COLON, "':'")) {
                    ast_free(key);
                    ast_free(dict);
                    return NULL;
                }

                AstNode *value = parse_expression(p);
                if (!value) {
                    ast_free(key);
                    ast_free(dict);
                    return NULL;
                }

                if (!ast_add_child(dict, key)) {
                    ast_free(key);
                    ast_free(value);
                    ast_free(dict);
                    return NULL;
                }
                if (!ast_add_child(dict, value)) {
                    ast_free(value);
                    ast_free(dict);
                    return NULL;
                }

                if (p->current.kind != TOKEN_COMMA) break;
                next(p);
            }
        }

        if (!expect(p, TOKEN_RBRACE, "'}'")) { ast_free(dict); return NULL; }
        return dict;
    }

    if (t.kind == TOKEN_IDENTIFIER) {
        next(p);
        if (p->current.kind == TOKEN_LPAREN) return parse_call_after_name(p, t);
        return ast_new(AST_IDENTIFIER, t.start, t.length);
    }

    /* conversions int(x) and float(x) */
    if ((t.kind == TOKEN_INT || t.kind == TOKEN_FLOAT) ) {
        next(p);
        if (p->current.kind == TOKEN_LPAREN) return parse_call_after_name(p, t);
        fprintf(stderr, "parse error at %zu:%zu: expected '(' after %s\n",
                t.line, t.column, token_kind_name(t.kind));
        p->had_error = 1;
        return NULL;
    }

    if (t.kind == TOKEN_LPAREN) {
        next(p);
        AstNode *e = parse_expression(p);
        if (!e) return NULL;
        if (!expect(p, TOKEN_RPAREN, "')'")) { ast_free(e); return NULL; }
        return e;
    }

    fprintf(stderr, "parse error at %zu:%zu: expected expression, got %s\n",
            t.line, t.column, token_kind_name(t.kind));
    p->had_error = 1;
    return NULL;
}

static AstNode *parse_primary(Parser *p) {
    AstNode *base = parse_atom(p);
    if (!base) return NULL;

    for (;;) {
        if (p->current.kind == TOKEN_LBRACKET) {
            next(p);
            AstNode *index = parse_expression(p);
            if (!index) { ast_free(base); return NULL; }
            if (!expect(p, TOKEN_RBRACKET, "']'")) {
                ast_free(index);
                ast_free(base);
                return NULL;
            }

            AstNode *node = ast_new(AST_INDEX, NULL, 0);
            if (!node) {
                ast_free(base);
                ast_free(index);
                return NULL;
            }
            if (!ast_add_child(node, base)) {
                ast_free(node);
                ast_free(base);
                ast_free(index);
                return NULL;
            }
            if (!ast_add_child(node, index)) {
                ast_free(node);
                ast_free(index);
                return NULL;
            }
            base = node;
            continue;
        }

        if (p->current.kind == TOKEN_DOT) {
            next(p);
            Token field = p->current;
            if (!expect(p, TOKEN_IDENTIFIER, "field name")) {
                ast_free(base);
                return NULL;
            }

            /* receiver.method(args): children are [receiver, arg1, arg2, ...] */
            if (p->current.kind == TOKEN_LPAREN) {
                AstNode *call = parse_call_after_name(p, field);
                if (!call) {
                    ast_free(base);
                    return NULL;
                }

                AstNode *method = ast_new(AST_METHOD_CALL, field.start, field.length);
                if (!method) {
                    ast_free(call);
                    ast_free(base);
                    return NULL;
                }
                if (!ast_add_child(method, base)) {
                    ast_free(method);
                    ast_free(call);
                    ast_free(base);
                    return NULL;
                }
                for (size_t i = 0; i < call->child_count; i++) {
                    if (!ast_add_child(method, call->children[i])) {
                        for (size_t j = i; j < call->child_count; j++) ast_free(call->children[j]);
                        call->child_count = 0;
                        ast_free(call);
                        ast_free(method);
                        return NULL;
                    }
                }
                call->child_count = 0;
                ast_free(call);
                base = method;
                continue;
            }

            AstNode *node = ast_new(AST_MEMBER, field.start, field.length);
            if (!node) {
                ast_free(base);
                return NULL;
            }
            if (!ast_add_child(node, base)) {
                ast_free(node);
                ast_free(base);
                return NULL;
            }
            base = node;
            continue;
        }

        break;
    }

    return base;
}

static AstNode *parse_unary(Parser *p) {
    if (p->current.kind == TOKEN_MINUS) {
        Token op = p->current;
        next(p);

        AstNode *operand = parse_unary(p);
        if (!operand) return NULL;

        AstNode *unary = ast_new(AST_UNARY, op.start, op.length);
        if (!unary || !ast_add_child(unary, operand)) {
            ast_free(unary);
            ast_free(operand);
            return NULL;
        }
        return unary;
    }

    return parse_primary(p);
}

static AstNode *parse_factor(Parser *p) {
    AstNode *left = parse_unary(p);
    if (!left) return NULL;

    while (p->current.kind == TOKEN_STAR || p->current.kind == TOKEN_SLASH ||
           p->current.kind == TOKEN_PERCENT) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_unary(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }

    return left;
}

static AstNode *parse_additive(Parser *p) {
    AstNode *left = parse_factor(p);
    if (!left) return NULL;

    while (p->current.kind == TOKEN_PLUS || p->current.kind == TOKEN_MINUS) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_factor(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }

    return left;
}

static int is_comparison(TokenKind kind) {
    return kind == TOKEN_EQUAL_EQUAL || kind == TOKEN_BANG_EQUAL ||
           kind == TOKEN_LESS || kind == TOKEN_LESS_EQUAL ||
           kind == TOKEN_GREATER || kind == TOKEN_GREATER_EQUAL;
}

static AstNode *parse_comparison(Parser *p) {
    AstNode *left = parse_additive(p);
    if (!left) return NULL;

    if (is_comparison(p->current.kind)) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_additive(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }

    return left;
}

static AstNode *parse_not(Parser *p) {
    if (p->current.kind == TOKEN_NOT) {
        Token op = p->current;
        next(p);

        AstNode *operand = parse_not(p);
        if (!operand) return NULL;

        AstNode *unary = ast_new(AST_UNARY, op.start, op.length);
        if (!unary || !ast_add_child(unary, operand)) {
            ast_free(unary);
            ast_free(operand);
            return NULL;
        }
        return unary;
    }

    return parse_comparison(p);
}

static AstNode *parse_and(Parser *p) {
    AstNode *left = parse_not(p);
    if (!left) return NULL;

    while (p->current.kind == TOKEN_AND) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_not(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }

    return left;
}

static AstNode *parse_or(Parser *p) {
    AstNode *left = parse_and(p);
    if (!left) return NULL;

    while (p->current.kind == TOKEN_OR) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_and(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }

    return left;
}

static AstNode *parse_expression(Parser *p) {
    return parse_or(p);
}

static AstNode *parse_if_statement(Parser *p) {
    if (!expect(p, TOKEN_IF, "if")) return NULL;

    AstNode *condition = parse_expression(p);
    if (!condition) return NULL;

    if (!expect(p, TOKEN_LBRACE, "'{'")) {
        ast_free(condition);
        return NULL;
    }

    AstNode *then_block = parse_block(p);
    if (!then_block) {
        ast_free(condition);
        return NULL;
    }

    if (!expect(p, TOKEN_RBRACE, "'}'")) {
        ast_free(condition);
        ast_free(then_block);
        return NULL;
    }

    AstNode *n = ast_new(AST_IF, NULL, 0);
    if (!n) {
        ast_free(condition);
        ast_free(then_block);
        return NULL;
    }

    if (!ast_add_child(n, condition)) {
        ast_free(n);
        ast_free(condition);
        ast_free(then_block);
        return NULL;
    }
    if (!ast_add_child(n, then_block)) {
        ast_free(n);
        ast_free(then_block);
        return NULL;
    }

    if (p->current.kind == TOKEN_ELSE) {
        next(p);
        if (!expect(p, TOKEN_LBRACE, "'{'")) {
            ast_free(n);
            return NULL;
        }

        AstNode *else_block = parse_block(p);
        if (!else_block) {
            ast_free(n);
            return NULL;
        }

        if (!expect(p, TOKEN_RBRACE, "'}'")) {
            ast_free(else_block);
            ast_free(n);
            return NULL;
        }

        if (!ast_add_child(n, else_block)) {
            ast_free(else_block);
            ast_free(n);
            return NULL;
        }
    }

    return n;
}

static AstNode *parse_while_statement(Parser *p) {
    if (!expect(p, TOKEN_WHILE, "while")) return NULL;

    AstNode *condition = parse_expression(p);
    if (!condition) return NULL;

    if (!expect(p, TOKEN_LBRACE, "'{'") ) {
        ast_free(condition);
        return NULL;
    }

    p->loop_depth++;
    AstNode *body = parse_block(p);
    p->loop_depth--;
    if (!body) {
        ast_free(condition);
        return NULL;
    }

    if (!expect(p, TOKEN_RBRACE, "'}'")) {
        ast_free(condition);
        ast_free(body);
        return NULL;
    }

    AstNode *n = ast_new(AST_WHILE, NULL, 0);
    if (!n) {
        ast_free(condition);
        ast_free(body);
        return NULL;
    }

    if (!ast_add_child(n, condition)) {
        ast_free(n);
        ast_free(condition);
        ast_free(body);
        return NULL;
    }
    if (!ast_add_child(n, body)) {
        ast_free(n);
        ast_free(body);
        return NULL;
    }

    return n;
}

static AstNode *parse_for_statement(Parser *p) {
    if (!expect(p, TOKEN_FOR, "for")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "loop variable")) return NULL;
    if (!expect(p, TOKEN_IN, "in")) return NULL;
    if (!expect(p, TOKEN_RANGE, "range")) return NULL;
    if (!expect(p, TOKEN_LPAREN, "'('") ) return NULL;

    AstNode *range_args[3] = { NULL, NULL, NULL };
    size_t arg_count = 0;

    if (p->current.kind == TOKEN_RPAREN) {
        fprintf(stderr, "parse error at %zu:%zu: range() expects 1 to 3 arguments\n",
                p->current.line, p->current.column);
        p->had_error = 1;
        return NULL;
    }

    for (;;) {
        if (arg_count == 3) {
            fprintf(stderr, "parse error at %zu:%zu: range() accepts at most 3 arguments\n",
                    p->current.line, p->current.column);
            p->had_error = 1;
            for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
            return NULL;
        }

        AstNode *arg = parse_expression(p);
        if (!arg) {
            for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
            return NULL;
        }
        range_args[arg_count++] = arg;

        if (p->current.kind != TOKEN_COMMA) break;
        next(p);
    }

    if (!expect(p, TOKEN_RPAREN, "')'")) {
        for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
        return NULL;
    }

    if (!expect(p, TOKEN_LBRACE, "'{'") ) {
        for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
        return NULL;
    }

    p->loop_depth++;
    AstNode *body = parse_block(p);
    p->loop_depth--;
    if (!body) {
        for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
        return NULL;
    }

    if (!expect(p, TOKEN_RBRACE, "'}'")) {
        for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
        ast_free(body);
        return NULL;
    }

    AstNode *n = ast_new(AST_FOR, name.start, name.length);
    if (!n) {
        for (size_t i = 0; i < arg_count; i++) ast_free(range_args[i]);
        ast_free(body);
        return NULL;
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (!ast_add_child(n, range_args[i])) {
            for (size_t j = i; j < arg_count; j++) ast_free(range_args[j]);
            ast_free(body);
            ast_free(n);
            return NULL;
        }
    }

    if (!ast_add_child(n, body)) {
        ast_free(body);
        ast_free(n);
        return NULL;
    }

    return n;
}

static int parse_and_set_type(Parser *p, AstNode *node) {
    Token t = p->current;

    /* matrix[float] / matrix[int]: "matrix" stays an ordinary identifier elsewhere */
    if (t.kind == TOKEN_IDENTIFIER && t.length == 6 && strncmp(t.start, "matrix", 6) == 0) {
        next(p);
        if (p->current.kind == TOKEN_LBRACKET) {
            next(p);
            const char *matrix_type = NULL;
            if (p->current.kind == TOKEN_FLOAT) matrix_type = "matrix[float]";
            else if (p->current.kind == TOKEN_INT) matrix_type = "matrix[int]";
            else {
                fprintf(stderr, "parse error at %zu:%zu: matrix element type must be float or int\n",
                        p->current.line, p->current.column);
                p->had_error = 1;
                return 0;
            }
            next(p);
            if (!expect(p, TOKEN_RBRACKET, "']'")) return 0;
            return ast_set_type(node, matrix_type, strlen(matrix_type));
        }
        return ast_set_type(node, t.start, t.length);  /* a class named matrix */
    }

    if (t.kind == TOKEN_INT || t.kind == TOKEN_FLOAT ||
        t.kind == TOKEN_BOOL || t.kind == TOKEN_STRING_TYPE ||
        t.kind == TOKEN_IDENTIFIER) {
        next(p);
        return ast_set_type(node, t.start, t.length);
    }

    if (t.kind == TOKEN_LIST) {
        next(p);
        if (!expect(p, TOKEN_LBRACKET, "'['")) return 0;

        const char *list_type = NULL;
        size_t list_type_len = 0;
        switch (p->current.kind) {
            case TOKEN_INT:         list_type = "list[int]";    list_type_len = 9;  break;
            case TOKEN_FLOAT:       list_type = "list[float]";  list_type_len = 11; break;
            case TOKEN_BOOL:        list_type = "list[bool]";   list_type_len = 10; break;
            case TOKEN_STRING_TYPE: list_type = "list[string]"; list_type_len = 12; break;
            default:
                fprintf(stderr,
                        "parse error at %zu:%zu: list element type must be int, float, bool or string\n",
                        p->current.line, p->current.column);
                p->had_error = 1;
                return 0;
        }

        next(p);
        if (!expect(p, TOKEN_RBRACKET, "']'")) return 0;
        return ast_set_type(node, list_type, list_type_len);
    }

    if (t.kind == TOKEN_DICT) {
        next(p);
        if (!expect(p, TOKEN_LBRACKET, "'['")) return 0;

        if (p->current.kind != TOKEN_STRING_TYPE) {
            fprintf(stderr,
                    "parse error at %zu:%zu: dictionary key type must be string\n",
                    p->current.line, p->current.column);
            p->had_error = 1;
            return 0;
        }
        next(p);

        if (!expect(p, TOKEN_COMMA, "','")) return 0;

        const char *dict_type = NULL;
        size_t dict_type_len = 0;
        switch (p->current.kind) {
            case TOKEN_INT:         dict_type = "dict[string,int]";    dict_type_len = 16; break;
            case TOKEN_FLOAT:       dict_type = "dict[string,float]";  dict_type_len = 18; break;
            case TOKEN_BOOL:        dict_type = "dict[string,bool]";   dict_type_len = 17; break;
            case TOKEN_STRING_TYPE: dict_type = "dict[string,string]"; dict_type_len = 19; break;
            default:
                fprintf(stderr,
                        "parse error at %zu:%zu: dictionary value type must be int, float, bool or string\n",
                        p->current.line, p->current.column);
                p->had_error = 1;
                return 0;
        }

        next(p);
        if (!expect(p, TOKEN_RBRACKET, "']'")) return 0;
        return ast_set_type(node, dict_type, dict_type_len);
    }

    fprintf(stderr, "parse error at %zu:%zu: expected a Clair type, got %s\n",
            p->current.line, p->current.column, token_kind_name(p->current.kind));
    p->had_error = 1;
    return 0;
}

/* Builds a node and adopts `a` and `b` (either may be NULL); frees them on failure. */
static AstNode *make_node(AstKind kind, const char *text, AstNode *a, AstNode *b) {
    AstNode *n = ast_new(kind, text, text ? strlen(text) : 0);
    if (!n) {
        ast_free(a);
        ast_free(b);
        return NULL;
    }
    if (a && !ast_add_child(n, a)) {
        ast_free(a);
        ast_free(b);
        ast_free(n);
        return NULL;
    }
    if (b && !ast_add_child(n, b)) {
        ast_free(b);
        ast_free(n);
        return NULL;
    }
    return n;
}

/* Detaches a child so it can be reused; ast_free() ignores the NULL left behind. */
static AstNode *take_child(AstNode *node, size_t index) {
    AstNode *child = node->children[index];
    node->children[index] = NULL;
    return child;
}

/* obj.field or obj.a.b: a pure field chain that can be the target of a write */
static int is_field_chain(const AstNode *n) {
    if (!n || n->kind != AST_MEMBER || n->child_count != 1) return 0;
    const AstNode *base = n->children[0];
    return base->kind == AST_IDENTIFIER || is_field_chain(base);
}

/*
 * Builds a write whose target is either a variable (stored in `text`) or a
 * field chain (stored as an extra last child, `text` left NULL).
 */
static AstNode *make_write(AstKind kind, AstNode *target, AstNode **children, size_t count) {
    int is_var = target->kind == AST_IDENTIFIER;
    AstNode *n = ast_new(kind, is_var ? target->text : NULL, is_var ? strlen(target->text) : 0);
    if (!n) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (!ast_add_child(n, children[i])) { ast_free(n); return NULL; }
        children[i] = NULL;
    }
    return n;
}

/*
 * Statements that start with an identifier.  The left-hand side is parsed
 * as an ordinary expression, then its shape decides the statement:
 *
 *   name = value                 assignment
 *   name[index] = value          indexed assignment
 *   a.b.field = value            field assignment (any object expression)
 *   name.append(value)           list append
 *   f(args) / a.b.method(args)   call used as a statement
 */
static AstNode *parse_identifier_statement(Parser *p) {
    Token start = p->current;
    AstNode *lhs = parse_primary(p);
    if (!lhs) return NULL;

    if (lhs->kind == AST_METHOD_CALL && lhs->text && strcmp(lhs->text, "append") == 0 &&
        (lhs->children[0]->kind == AST_IDENTIFIER || is_field_chain(lhs->children[0]))) {
        if (lhs->child_count != 2) {
            fprintf(stderr, "parse error at %zu:%zu: append() expects exactly 1 argument\n",
                    start.line, start.column);
            p->had_error = 1;
            ast_free(lhs);
            return NULL;
        }
        AstNode *target = lhs->children[0];
        AstNode *kids[2] = { take_child(lhs, 1), NULL };
        size_t count = 1;
        if (target->kind != AST_IDENTIFIER) { kids[1] = take_child(lhs, 0); count = 2; }
        AstNode *n = make_write(AST_LIST_APPEND, kids[1] ? kids[1] : target, kids, count);
        for (size_t i = 0; i < 2; i++) ast_free(kids[i]);
        ast_free(lhs);
        return n;
    }

    if (p->current.kind == TOKEN_EQUAL) {
        next(p);
        AstNode *value = parse_expression(p);
        if (!value) {
            ast_free(lhs);
            return NULL;
        }

        AstNode *n = NULL;
        switch (lhs->kind) {
            case AST_IDENTIFIER:
                n = make_node(AST_ASSIGN, lhs->text, value, NULL);
                break;

            case AST_INDEX:
                if (lhs->children[0]->kind == AST_INDEX &&
                    (lhs->children[0]->children[0]->kind == AST_IDENTIFIER ||
                     is_field_chain(lhs->children[0]->children[0]))) {
                    /* m[i][j] = value  or  obj.m[i][j] = value */
                    AstNode *row = lhs->children[0];
                    AstNode *target = row->children[0];
                    AstNode *kids[4] = { take_child(row, 1), take_child(lhs, 1), value, NULL };
                    size_t count = 3;
                    if (target->kind != AST_IDENTIFIER) { kids[3] = take_child(row, 0); count = 4; }
                    n = make_write(AST_MATRIX_SET, kids[3] ? kids[3] : target, kids, count);
                    for (size_t i = 0; i < 4; i++) ast_free(kids[i]);
                    break;
                }
                if (is_field_chain(lhs->children[0])) {
                    /* obj.items[i] = value */
                    AstNode *kids[3] = { take_child(lhs, 1), value, take_child(lhs, 0) };
                    n = make_write(AST_INDEX_ASSIGN, kids[2], kids, 3);
                    for (size_t i = 0; i < 3; i++) ast_free(kids[i]);
                    break;
                }
                if (lhs->children[0]->kind != AST_IDENTIFIER) {
                    fprintf(stderr,
                            "parse error at %zu:%zu: indexed assignment target must be a variable or a field\n",
                            start.line, start.column);
                    p->had_error = 1;
                    ast_free(value);
                    ast_free(lhs);
                    return NULL;
                }
                n = make_node(AST_INDEX_ASSIGN, lhs->children[0]->text,
                              take_child(lhs, 1), value);
                break;

            case AST_MEMBER:
                n = make_node(AST_MEMBER_ASSIGN, lhs->text, take_child(lhs, 0), value);
                break;

            default:
                fprintf(stderr, "parse error at %zu:%zu: invalid assignment target\n",
                        start.line, start.column);
                p->had_error = 1;
                ast_free(value);
                ast_free(lhs);
                return NULL;
        }
        ast_free(lhs);
        return n;
    }

    if (lhs->kind == AST_CALL || lhs->kind == AST_METHOD_CALL) {
        return make_node(AST_EXPR_STMT, NULL, lhs, NULL);
    }

    expect(p, TOKEN_EQUAL, "'='");
    ast_free(lhs);
    return NULL;
}

static AstNode *parse_statement(Parser *p) {
    if (p->current.kind == TOKEN_LET) {
        next(p);
        Token name = p->current;
        if (!expect(p, TOKEN_IDENTIFIER, "variable name")) return NULL;
        if (!expect(p, TOKEN_COLON, "':'")) return NULL;
        AstNode *n = ast_new(AST_LET, name.start, name.length);
        if (!n) return NULL;
        if (!parse_and_set_type(p, n)) { ast_free(n); return NULL; }
        if (!expect(p, TOKEN_EQUAL, "'='")) { ast_free(n); return NULL; }

        AstNode *value = parse_expression(p);
        if (!value) { ast_free(n); return NULL; }

        if (!ast_add_child(n, value)) {
            ast_free(n);
            ast_free(value);
            return NULL;
        }
        return n;
    }

    if (p->current.kind == TOKEN_IDENTIFIER) {
        return parse_identifier_statement(p);
    }

    if (p->current.kind == TOKEN_RETURN) {
        next(p);
        AstNode *value = parse_expression(p);
        if (!value) return NULL;

        AstNode *n = ast_new(AST_RETURN, NULL, 0);
        if (!n || !ast_add_child(n, value)) {
            ast_free(n);
            ast_free(value);
            return NULL;
        }
        return n;
    }

    if (p->current.kind == TOKEN_PRINT) {
        next(p);
        if (!expect(p, TOKEN_LPAREN, "'('") ) return NULL;

        AstNode *value = parse_expression(p);
        if (!value) return NULL;

        if (!expect(p, TOKEN_RPAREN, "')'")) {
            ast_free(value);
            return NULL;
        }

        AstNode *n = ast_new(AST_PRINT, NULL, 0);
        if (!n || !ast_add_child(n, value)) {
            ast_free(n);
            ast_free(value);
            return NULL;
        }
        return n;
    }

    if (p->current.kind == TOKEN_IF) {
        return parse_if_statement(p);
    }

    if (p->current.kind == TOKEN_WHILE) {
        return parse_while_statement(p);
    }

    if (p->current.kind == TOKEN_FOR) {
        return parse_for_statement(p);
    }

    if (p->current.kind == TOKEN_BREAK || p->current.kind == TOKEN_CONTINUE) {
        Token keyword = p->current;
        AstKind kind = keyword.kind == TOKEN_BREAK ? AST_BREAK : AST_CONTINUE;

        if (p->loop_depth == 0) {
            fprintf(stderr, "parse error at %zu:%zu: '%s' is only valid inside a loop\n",
                    keyword.line, keyword.column, token_kind_name(keyword.kind));
            p->had_error = 1;
            return NULL;
        }

        next(p);
        return ast_new(kind, NULL, 0);
    }

    fprintf(stderr, "parse error at %zu:%zu: unexpected token %s in block\n",
            p->current.line, p->current.column, token_kind_name(p->current.kind));
    p->had_error = 1;
    return NULL;
}

static AstNode *parse_block(Parser *p) {
    AstNode *block = ast_new(AST_BLOCK, NULL, 0);
    if (!block) return NULL;

    while (p->current.kind != TOKEN_RBRACE && p->current.kind != TOKEN_EOF) {
        AstNode *stmt = parse_statement(p);
        if (!stmt) { ast_free(block); return NULL; }
        if (!ast_add_child(block, stmt)) {
            ast_free(stmt);
            ast_free(block);
            return NULL;
        }
    }

    return block;
}

/*
 * Parses a class.  Fields become children of the class node.  Methods are
 * appended to `program` as top-level functions named `Class__method` (see
 * parse_function_impl), so the rest of the compiler needs no special case
 * for them beyond resolving `value.method(...)` calls.
 */
static AstNode *parse_class(Parser *p, AstNode *program) {
    if (!expect(p, TOKEN_CLASS, "Class")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "class name")) return NULL;
    if (!expect(p, TOKEN_LBRACE, "'{'")) return NULL;

    AstNode *cls = ast_new(AST_CLASS, name.start, name.length);
    if (!cls) return NULL;

    size_t first_method = program->child_count;
    size_t prefix_len = strlen(cls->text) + 2;

    while (p->current.kind != TOKEN_RBRACE && p->current.kind != TOKEN_EOF) {
        if (p->current.kind == TOKEN_DEF) {
            Token def_token = p->current;
            AstNode *method = parse_function_impl(p, cls->text, strlen(cls->text));
            if (!method) {
                ast_free(cls);
                return NULL;
            }

            for (size_t i = first_method; i < program->child_count; i++) {
                if (strcmp(program->children[i]->text, method->text) == 0) {
                    fprintf(stderr, "parse error at %zu:%zu: duplicate method '%s.%s'\n",
                            def_token.line, def_token.column,
                            cls->text, method->text + prefix_len);
                    p->had_error = 1;
                    ast_free(method);
                    ast_free(cls);
                    return NULL;
                }
            }

            if (!ast_add_child(program, method)) {
                ast_free(method);
                ast_free(cls);
                return NULL;
            }
            continue;
        }

        Token field_name = p->current;
        if (!expect(p, TOKEN_IDENTIFIER, "field name")) {
            ast_free(cls);
            return NULL;
        }
        if (!expect(p, TOKEN_COLON, "':'")) {
            ast_free(cls);
            return NULL;
        }

        AstNode *field = ast_new(AST_FIELD, field_name.start, field_name.length);
        if (!field) {
            ast_free(cls);
            return NULL;
        }
        if (!parse_and_set_type(p, field)) {
            ast_free(field);
            ast_free(cls);
            return NULL;
        }
        if (!ast_add_child(cls, field)) {
            ast_free(field);
            ast_free(cls);
            return NULL;
        }
    }

    if (!expect(p, TOKEN_RBRACE, "'}'")) {
        ast_free(cls);
        return NULL;
    }

    for (size_t m = first_method; m < program->child_count; m++) {
        const char *method_name = program->children[m]->text + prefix_len;
        for (size_t j = 0; j < cls->child_count; j++) {
            if (strcmp(cls->children[j]->text, method_name) == 0) {
                fprintf(stderr,
                        "parse error: method '%s.%s' conflicts with a field of the same name\n",
                        cls->text, method_name);
                p->had_error = 1;
                ast_free(cls);
                return NULL;
            }
        }
    }

    return cls;
}

/*
 * Parses `def name(params) -> type { body }`.
 *
 * With owner == NULL this is an ordinary function.  With an owner class
 * it is a method: it becomes the top-level function `Owner__name` whose
 * first parameter is the implicit `self: Owner`.  Calls written
 * `value.name(args)` are resolved to it by the semantic pass.
 */
static AstNode *parse_function_impl(Parser *p, const char *owner, size_t owner_len) {
    if (!expect(p, TOKEN_DEF, "def")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "function name")) return NULL;
    if (!expect(p, TOKEN_LPAREN, "'('") ) return NULL;

    AstNode *fn = NULL;
    if (owner) {
        size_t len = owner_len + 2 + name.length;
        char *mangled = malloc(len + 1);
        if (!mangled) return NULL;
        memcpy(mangled, owner, owner_len);
        memcpy(mangled + owner_len, "__", 2);
        memcpy(mangled + owner_len + 2, name.start, name.length);
        mangled[len] = '\0';
        fn = ast_new(AST_FUNCTION, mangled, len);
        free(mangled);
    } else {
        fn = ast_new(AST_FUNCTION, name.start, name.length);
    }
    if (!fn) return NULL;

    if (owner) {
        AstNode *self = ast_new(AST_PARAM, "self", 4);
        if (!self || !ast_set_type(self, owner, owner_len) || !ast_add_child(fn, self)) {
            ast_free(self);
            ast_free(fn);
            return NULL;
        }
    }

    if (p->current.kind != TOKEN_RPAREN) {
        for (;;) {
            Token param_name = p->current;
            if (!expect(p, TOKEN_IDENTIFIER, "parameter name")) { ast_free(fn); return NULL; }
            if (!expect(p, TOKEN_COLON, "':'")) { ast_free(fn); return NULL; }
            AstNode *param = ast_new(AST_PARAM, param_name.start, param_name.length);
            if (!param) { ast_free(fn); return NULL; }
            if (!parse_and_set_type(p, param)) { ast_free(param); ast_free(fn); return NULL; }

            if (!ast_add_child(fn, param)) {
                ast_free(param);
                ast_free(fn);
                return NULL;
            }

            if (p->current.kind != TOKEN_COMMA) break;
            next(p);
        }
    }

    if (!expect(p, TOKEN_RPAREN, "')'")) { ast_free(fn); return NULL; }
    if (!expect(p, TOKEN_ARROW, "'->'")) { ast_free(fn); return NULL; }
    if (!parse_and_set_type(p, fn)) { ast_free(fn); return NULL; }
    if (!expect(p, TOKEN_LBRACE, "'{'")) { ast_free(fn); return NULL; }

    AstNode *body = parse_block(p);
    if (!body) { ast_free(fn); return NULL; }

    if (!expect(p, TOKEN_RBRACE, "'}'")) {
        ast_free(body);
        ast_free(fn);
        return NULL;
    }

    if (!ast_add_child(fn, body)) {
        ast_free(body);
        ast_free(fn);
        return NULL;
    }

    return fn;
}

static AstNode *parse_function(Parser *p) {
    return parse_function_impl(p, NULL, 0);
}

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = 0;
    p->loop_depth = 0;
    next(p);
}

AstNode *parser_parse_program(Parser *p) {
    AstNode *program = ast_new(AST_PROGRAM, NULL, 0);
    if (!program) return NULL;

    while (p->current.kind != TOKEN_EOF) {
        if (p->current.kind == TOKEN_ERROR) {
            fprintf(stderr, "lex error at %zu:%zu\n", p->current.line, p->current.column);
            p->had_error = 1;
            ast_free(program);
            return NULL;
        }

        AstNode *decl = NULL;
        if (p->current.kind == TOKEN_CLASS) {
            decl = parse_class(p, program);
        } else if (p->current.kind == TOKEN_DEF) {
            decl = parse_function(p);
        } else {
            fprintf(stderr, "parse error at %zu:%zu: expected Class or def, got %s\n",
                    p->current.line, p->current.column, token_kind_name(p->current.kind));
            p->had_error = 1;
            ast_free(program);
            return NULL;
        }

        if (!decl) {
            ast_free(program);
            return NULL;
        }

        if (!ast_add_child(program, decl)) {
            ast_free(decl);
            ast_free(program);
            return NULL;
        }
    }

    return program;
}
