#include "parser.h"
#include <stdio.h>
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

    if (t.kind == TOKEN_INT) {
        next(p);
        return parse_call_after_name(p, t);
    }


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

    while (p->current.kind == TOKEN_STAR || p->current.kind == TOKEN_SLASH) {
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
        Token name = p->current;
        next(p);

        if (p->current.kind == TOKEN_LBRACKET) {
            next(p);
            AstNode *index = parse_expression(p);
            if (!index) return NULL;
            if (!expect(p, TOKEN_RBRACKET, "']'")) { ast_free(index); return NULL; }
            if (!expect(p, TOKEN_EQUAL, "'='")) { ast_free(index); return NULL; }

            AstNode *value = parse_expression(p);
            if (!value) { ast_free(index); return NULL; }

            AstNode *n = ast_new(AST_INDEX_ASSIGN, name.start, name.length);
            if (!n) { ast_free(index); ast_free(value); return NULL; }
            if (!ast_add_child(n, index)) { ast_free(n); ast_free(index); ast_free(value); return NULL; }
            if (!ast_add_child(n, value)) { ast_free(n); ast_free(value); return NULL; }
            return n;
        }

        if (p->current.kind == TOKEN_DOT) {
            next(p);
            Token member = p->current;
            if (!expect(p, TOKEN_IDENTIFIER, "member name")) return NULL;

            if (member.length == 6 && strncmp(member.start, "append", 6) == 0 &&
                p->current.kind == TOKEN_LPAREN) {
                next(p);
                AstNode *value = parse_expression(p);
                if (!value) return NULL;
                if (!expect(p, TOKEN_RPAREN, "')'")) { ast_free(value); return NULL; }

                AstNode *n = ast_new(AST_LIST_APPEND, name.start, name.length);
                if (!n) { ast_free(value); return NULL; }
                if (!ast_add_child(n, value)) { ast_free(n); ast_free(value); return NULL; }
                return n;
            }

            if (!expect(p, TOKEN_EQUAL, "'='")) return NULL;
            AstNode *value = parse_expression(p);
            if (!value) return NULL;

            AstNode *base = ast_new(AST_IDENTIFIER, name.start, name.length);
            AstNode *n = ast_new(AST_MEMBER_ASSIGN, member.start, member.length);
            if (!base || !n) {
                ast_free(base);
                ast_free(n);
                ast_free(value);
                return NULL;
            }
            if (!ast_add_child(n, base)) {
                ast_free(n);
                ast_free(base);
                ast_free(value);
                return NULL;
            }
            if (!ast_add_child(n, value)) {
                ast_free(n);
                ast_free(value);
                return NULL;
            }
            return n;
        }

        if (!expect(p, TOKEN_EQUAL, "'='")) return NULL;

        AstNode *value = parse_expression(p);
        if (!value) return NULL;

        AstNode *n = ast_new(AST_ASSIGN, name.start, name.length);
        if (!n || !ast_add_child(n, value)) {
            ast_free(n);
            if (!n) ast_free(value);
            return NULL;
        }
        return n;
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

static AstNode *parse_class(Parser *p) {
    if (!expect(p, TOKEN_CLASS, "Class")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "class name")) return NULL;
    if (!expect(p, TOKEN_LBRACE, "'{'")) return NULL;

    AstNode *cls = ast_new(AST_CLASS, name.start, name.length);
    if (!cls) return NULL;

    while (p->current.kind != TOKEN_RBRACE && p->current.kind != TOKEN_EOF) {
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

    return cls;
}

static AstNode *parse_function(Parser *p) {
    if (!expect(p, TOKEN_DEF, "def")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "function name")) return NULL;
    if (!expect(p, TOKEN_LPAREN, "'('") ) return NULL;

    AstNode *fn = ast_new(AST_FUNCTION, name.start, name.length);
    if (!fn) return NULL;

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
            decl = parse_class(p);
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
