#include <ctype.h>
#include "parser.h"
#include "units.h"
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
static int parse_unit(Parser *p, AstNode *node);

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
        /* 1_000, 0xFF, 0b1010, 0o17: kept as plain decimal digits */
        char digits[400];
        if (!clair_integer_to_decimal(t.start, t.length, digits, sizeof(digits))) {
            fprintf(stderr, "parse error at %zu:%zu: invalid number %.*s\n", t.line, t.column, (int)t.length, t.start);
            p->had_error = 1;
            return NULL;
        }
        next(p);
        if (p->current.kind == TOKEN_LBRACKET) {
            /* V0.48: 400000[m] is the float 400000.0[m] */
            if (strlen(digits) + 3 > sizeof(digits)) {
                fprintf(stderr, "parse error at %zu:%zu: number too long for a unit\n", t.line, t.column);
                p->had_error = 1;
                return NULL;
            }
            strcat(digits, ".0");
            AstNode *num = ast_new(AST_FLOAT, digits, strlen(digits));
            if (!num || !parse_unit(p, num)) { ast_free(num); return NULL; }
            return num;
        }
        return ast_new(AST_INTEGER, digits, strlen(digits));
    }

    if (t.kind == TOKEN_FLOAT_LITERAL) {
        /* 1_000.5: the underscores only help reading */
        char text[400];
        size_t n = 0;
        for (size_t i = 0; i < t.length && n + 1 < sizeof(text); i++) {
            if (t.start[i] != '_') text[n++] = t.start[i];
        }
        next(p);
        AstNode *num = ast_new(AST_FLOAT, text, n);
        if (num && p->current.kind == TOKEN_LBRACKET && !parse_unit(p, num)) {   /* V0.48: 9.81[m/s^2] */
            ast_free(num);
            return NULL;
        }
        return num;
    }

    if (t.kind == TOKEN_TRUE || t.kind == TOKEN_FALSE) {
        next(p);
        return ast_new(AST_BOOL, t.start, t.length);
    }

    if (t.kind == TOKEN_STRING) {
        /* \n \t \r \\ \" \' and \xHH, like Python; another \ stays as it is */
        char *text = malloc(t.length + 1);
        if (!text) return NULL;
        size_t n = 0;
        for (size_t i = 0; i < t.length; i++) {
            char c = t.start[i], e = i + 1 < t.length ? t.start[i + 1] : '\0';
            int hex = e == 'x' && i + 3 < t.length && isxdigit((unsigned char)t.start[i + 2]) &&
                      isxdigit((unsigned char)t.start[i + 3]);
            if (c != '\\') text[n++] = c;
            else if (e == 'n') { text[n++] = '\n'; i++; }
            else if (e == 't') { text[n++] = '\t'; i++; }
            else if (e == 'r') { text[n++] = '\r'; i++; }
            else if (e == '\\' || e == '"' || e == '\'') { text[n++] = e; i++; }
            else if (hex && !(t.start[i + 2] == '0' && t.start[i + 3] == '0')) {
                char digits[3] = { t.start[i + 2], t.start[i + 3], '\0' };
                text[n++] = (char)strtol(digits, NULL, 16);
                i += 3;
            } else {
                text[n++] = c;
            }
        }
        next(p);
        AstNode *node = ast_new(AST_STRING, text, n);
        free(text);
        return node;
    }

    if (t.kind == TOKEN_LBRACKET) {
        next(p);
        AstNode *list = ast_new(AST_LIST, NULL, 0);
        if (!list) return NULL;

        if (p->current.kind != TOKEN_RBRACKET) {
            for (;;) {
                AstNode *item = parse_expression(p);
                if (!item) { ast_free(list); return NULL; }
                if (list->child_count == 0 && p->current.kind == TOKEN_FOR) {
                    /*
                     * [expr for v in xs if cond]: @comprehension(v, expr, xs, cond),
                     * turned into a small function by the semantic pass
                     */
                    ast_free(list);
                    next(p);
                    Token var = p->current;
                    AstNode *comp = ast_new(AST_CALL, "@comprehension", strlen("@comprehension"));
                    AstNode *name = var.kind == TOKEN_IDENTIFIER ? ast_new(AST_IDENTIFIER, var.start, var.length) : NULL;
                    if (!comp || !name || !ast_add_child(comp, name) || !ast_add_child(comp, item)) {
                        if (var.kind != TOKEN_IDENTIFIER)
                            fprintf(stderr, "parse error at %zu:%zu: expected a variable after 'for'\n", var.line, var.column);
                        p->had_error = 1;
                        ast_free(comp);
                        return NULL;
                    }
                    next(p);
                    if (!expect(p, TOKEN_IN, "in")) { ast_free(comp); return NULL; }
                    AstNode *iterable = NULL;
                    if (p->current.kind == TOKEN_RANGE) {
                        /* range(a, b, step): kept as a call, the semantic pass makes it a for loop */
                        next(p);
                        iterable = ast_new(AST_CALL, "range", 5);
                        if (!iterable || !expect(p, TOKEN_LPAREN, "'('")) { ast_free(iterable); ast_free(comp); return NULL; }
                        while (p->current.kind != TOKEN_RPAREN) {
                            AstNode *arg = parse_expression(p);
                            if (!arg || !ast_add_child(iterable, arg)) { ast_free(arg); ast_free(iterable); ast_free(comp); return NULL; }
                            if (p->current.kind != TOKEN_COMMA) break;
                            next(p);
                        }
                        if (!expect(p, TOKEN_RPAREN, "')'")) { ast_free(iterable); ast_free(comp); return NULL; }
                    } else {
                        iterable = parse_expression(p);
                    }
                    if (!iterable || !ast_add_child(comp, iterable)) { ast_free(iterable); ast_free(comp); return NULL; }
                    if (p->current.kind == TOKEN_IF) {
                        next(p);
                        AstNode *cond = parse_expression(p);
                        if (!cond || !ast_add_child(comp, cond)) { ast_free(cond); ast_free(comp); return NULL; }
                    }
                    if (!expect(p, TOKEN_RBRACKET, "']'")) { ast_free(comp); return NULL; }
                    return comp;
                }
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

/*
 * xs[a:b], xs[:b], xs[a:], xs[::c] ...  (after '[' and the optional first
 * bound): the call @slice<s><e><c> whose children are the list, then the
 * bounds that are written, <s><e><c> being 1 for a written part, 0 otherwise.
 */
static AstNode *parse_slice_after(Parser *p, AstNode *base, AstNode *start) {
    AstNode *parts[3] = { start, NULL, NULL };
    if (p->current.kind != TOKEN_COLON) {
        fprintf(stderr, "parse error at %zu:%zu: expected an index or a slice between [ and ]\n",
                p->current.line, p->current.column);
        p->had_error = 1;
        ast_free(start);
        ast_free(base);
        return NULL;
    }
    next(p);
    for (int k = 1; k < 3; k++) {
        if (p->current.kind != TOKEN_COLON && p->current.kind != TOKEN_RBRACKET) {
            parts[k] = parse_expression(p);
            if (!parts[k]) {
                for (int j = 0; j < 3; j++) ast_free(parts[j]);
                ast_free(base);
                return NULL;
            }
        }
        if (k == 1 && p->current.kind == TOKEN_COLON) next(p);
        else break;
    }
    if (!expect(p, TOKEN_RBRACKET, "']'")) {
        for (int j = 0; j < 3; j++) ast_free(parts[j]);
        ast_free(base);
        return NULL;
    }
    char name[16];
    snprintf(name, sizeof(name), "@slice%d%d%d", parts[0] != NULL, parts[1] != NULL, parts[2] != NULL);
    AstNode *call = ast_new(AST_CALL, name, strlen(name));
    int ok = call && ast_add_child(call, base);
    if (!ok) ast_free(base);
    for (int k = 0; k < 3; k++) {
        if (!parts[k]) continue;
        if (ok && ast_add_child(call, parts[k])) continue;
        ok = 0;
        ast_free(parts[k]);
    }
    if (!ok) {
        ast_free(call);
        return NULL;
    }
    return call;
}

static AstNode *parse_primary(Parser *p) {
    AstNode *base = parse_atom(p);
    if (!base) return NULL;

    for (;;) {
        if (p->current.kind == TOKEN_LBRACKET) {
            next(p);
            if (p->current.kind == TOKEN_COLON || p->current.kind == TOKEN_RBRACKET) {
                base = parse_slice_after(p, base, NULL);
                if (!base) return NULL;
                continue;
            }
            AstNode *index = parse_expression(p);
            if (!index) { ast_free(base); return NULL; }
            if (p->current.kind == TOKEN_COLON) {
                base = parse_slice_after(p, base, index);
                if (!base) return NULL;
                continue;
            }
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
    if (p->current.kind == TOKEN_MINUS || p->current.kind == TOKEN_TILDE) {
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

/*
 * Bit operators, with Python's priorities (not C's): below + and -, above the
 * comparisons, so a & b == c means (a & b) == c.
 *   shift: << >>   then &   then ^   then |
 */
static AstNode *parse_binary_level(Parser *p, int level);

static AstNode *parse_binary_level(Parser *p, int level) {
    static const TokenKind ops[4][2] = {
        { TOKEN_SHL, TOKEN_SHR }, { TOKEN_AMP, TOKEN_AMP }, { TOKEN_CARET, TOKEN_CARET }, { TOKEN_PIPE, TOKEN_PIPE },
    };
    AstNode *left = level == 0 ? parse_additive(p) : parse_binary_level(p, level - 1);
    if (!left) return NULL;
    while (p->current.kind == ops[level][0] || p->current.kind == ops[level][1]) {
        Token op = p->current;
        next(p);
        AstNode *right = level == 0 ? parse_additive(p) : parse_binary_level(p, level - 1);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    }
    return left;
}

static AstNode *parse_bitor(Parser *p) { return parse_binary_level(p, 3); }

static AstNode *parse_comparison(Parser *p) {
    AstNode *left = parse_bitor(p);
    if (!left) return NULL;

    if (is_comparison(p->current.kind)) {
        Token op = p->current;
        next(p);

        AstNode *right = parse_bitor(p);
        if (!right) { ast_free(left); return NULL; }
        left = make_binary(op, left, right);
        if (!left) return NULL;
    } else if (p->current.kind == TOKEN_IN || p->current.kind == TOKEN_NOT) {
        /* x in xs, x not in xs: like Python, at the level of the comparisons */
        Token op = p->current;
        int negated = op.kind == TOKEN_NOT;
        next(p);
        if (negated && !expect(p, TOKEN_IN, "in")) { ast_free(left); return NULL; }
        AstNode *right = parse_bitor(p);
        if (!right) { ast_free(left); return NULL; }
        Token text = op;
        text.start = negated ? "not in" : "in";
        text.length = negated ? 6 : 2;
        left = make_binary(text, left, right);
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

static AstNode *parse_for_statement(Parser *p);

static AstNode *identifier_node(const char *name) { return ast_new(AST_IDENTIFIER, name, strlen(name)); }

/*
 *   for i, v in enumerate(xs, start) { body }
 * is read as (start evaluated once, 0 when absent):
 *   let @enumN: int = start
 *   for v in xs { let i: int = @enumN ; @enumN = @enumN + 1 ; body }
 * returned as a block marked "@splice": its statements go straight into the
 * enclosing block.  The hidden name becomes clair_h_enumN in C.
 */
/* name (as a variable) somewhere in n */
static int mentions_name(const AstNode *n, const char *name, size_t len) {
    if (!n) return 0;
    if (n->text && strlen(n->text) == len && strncmp(n->text, name, len) == 0 &&
        (n->kind == AST_IDENTIFIER || n->kind == AST_ASSIGN || n->kind == AST_INDEX_ASSIGN ||
         n->kind == AST_MATRIX_SET || n->kind == AST_LIST_APPEND)) return 1;
    for (size_t i = 0; i < n->child_count; i++) {
        if (mentions_name(n->children[i], name, len)) return 1;
    }
    return 0;
}

/* 1, -1: nothing to evaluate */
static int is_plain_literal(const AstNode *n) {
    if (n->kind == AST_INTEGER) return 1;
    return n->kind == AST_UNARY && n->child_count == 1 && n->children[0]->kind == AST_INTEGER;
}

static AstNode *parse_enumerate_after(Parser *p, Token index) {
    next(p);                                                    /* , */
    Token value = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "loop variable")) return NULL;
    if (!expect(p, TOKEN_IN, "in")) return NULL;
    if (p->current.kind != TOKEN_IDENTIFIER || p->current.length != 9 ||
        strncmp(p->current.start, "enumerate", 9) != 0) {
        fprintf(stderr, "parse error at %zu:%zu: for %.*s, %.*s in ... needs enumerate(list)\n",
                p->current.line, p->current.column, (int)index.length, index.start, (int)value.length, value.start);
        p->had_error = 1;
        return NULL;
    }
    next(p);
    if (!expect(p, TOKEN_LPAREN, "'('")) return NULL;
    AstNode *list = parse_expression(p);
    if (!list) return NULL;
    AstNode *start = NULL;
    if (p->current.kind == TOKEN_COMMA) {
        next(p);
        start = parse_expression(p);
        if (!start) { ast_free(list); return NULL; }
    }
    if (!expect(p, TOKEN_RPAREN, "')'") || !expect(p, TOKEN_LBRACE, "'{'")) {
        ast_free(list);
        ast_free(start);
        return NULL;
    }
    p->loop_depth++;
    AstNode *body = parse_block(p);
    p->loop_depth--;
    if (!body || !expect(p, TOKEN_RBRACE, "'}'")) {
        ast_free(list);
        ast_free(start);
        ast_free(body);
        return NULL;
    }
    if (!mentions_name(body, index.start, index.length)) {
        /* the index is not used: an ordinary for v in xs (a start written as a number changes nothing) */
        if (!start || is_plain_literal(start)) {
            ast_free(start);
            AstNode *loop = ast_new(AST_FOR_EACH, value.start, value.length);
            if (!loop || !ast_add_child(loop, list) || !ast_add_child(loop, body)) {
                fprintf(stderr, "clair: out of memory\n");
                exit(EXIT_FAILURE);
            }
            return loop;
        }
    }
    if (!start) start = ast_new(AST_INTEGER, "0", 1);

    char hidden[32];
    snprintf(hidden, sizeof(hidden), "@enum%zu", ++p->hidden_count);
    AstNode *counter = ast_new(AST_LET, hidden, strlen(hidden));
    AstNode *loop = ast_new(AST_FOR_EACH, value.start, value.length);
    AstNode *let_index = ast_new(AST_LET, index.start, index.length);
    AstNode *read = identifier_node(hidden);
    AstNode *step = ast_new(AST_ASSIGN, hidden, strlen(hidden));
    AstNode *plus = ast_new(AST_BINARY, "+", 1);
    AstNode *old = identifier_node(hidden);
    AstNode *one = ast_new(AST_INTEGER, "1", 1);
    AstNode *block = ast_new(AST_BLOCK, "@splice", 7);
    AstNode *inner = ast_new(AST_BLOCK, NULL, 0);
    AstNode *all[] = { counter, loop, let_index, read, step, plus, old, one, block, inner, start };
    int ok = 1;
    for (size_t k = 0; k < sizeof(all) / sizeof(all[0]); k++) ok = ok && all[k];
    ok = ok && ast_set_type(counter, "int", 3) && ast_set_type(let_index, "int", 3) &&
         ast_add_child(counter, start) && ast_add_child(let_index, read) &&
         ast_add_child(plus, old) && ast_add_child(plus, one) && ast_add_child(step, plus) &&
         ast_add_child(inner, let_index) && ast_add_child(inner, step);
    for (size_t k = 0; ok && k < body->child_count; k++) ok = ast_add_child(inner, body->children[k]);
    if (ok) body->child_count = 0;
    ok = ok && ast_add_child(loop, list) && ast_add_child(loop, inner) &&
         ast_add_child(block, counter) && ast_add_child(block, loop);
    ast_free(body);
    if (!ok) {
        /* allocation failure: give up (the pieces may be partly linked) */
        fprintf(stderr, "clair: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return block;
}

static AstNode *parse_for_statement(Parser *p) {
    if (!expect(p, TOKEN_FOR, "for")) return NULL;

    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "loop variable")) return NULL;
    if (p->current.kind == TOKEN_COMMA) return parse_enumerate_after(p, name);
    if (!expect(p, TOKEN_IN, "in")) return NULL;

    if (p->current.kind != TOKEN_RANGE) {
        /* for v in list { ... } : the list expression is evaluated once */
        AstNode *iterable = parse_expression(p);
        if (!iterable) return NULL;
        if (!expect(p, TOKEN_LBRACE, "'{'")) {
            ast_free(iterable);
            return NULL;
        }
        p->loop_depth++;
        AstNode *body = parse_block(p);
        p->loop_depth--;
        if (!body) {
            ast_free(iterable);
            return NULL;
        }
        if (!expect(p, TOKEN_RBRACE, "'}'")) {
            ast_free(iterable);
            ast_free(body);
            return NULL;
        }
        AstNode *n = ast_new(AST_FOR_EACH, name.start, name.length);
        if (!n) {
            ast_free(iterable);
            ast_free(body);
            return NULL;
        }
        if (!ast_add_child(n, iterable)) {
            ast_free(iterable);
            ast_free(body);
            ast_free(n);
            return NULL;
        }
        if (!ast_add_child(n, body)) {
            ast_free(body);
            ast_free(n);
            return NULL;
        }
        return n;
    }

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

/* float32 is an ordinary identifier outside types */
static int is_float32_token(Token t) {
    return t.kind == TOKEN_IDENTIFIER && t.length == 7 && strncmp(t.start, "float32", 7) == 0;
}

/* machine integers u8 .. u64, i8 .. i32 (i64 is int): their name, or NULL */
static const char *machine_type_token(Token t) {
    static const char *const names[] = { "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", NULL };
    if (t.kind != TOKEN_IDENTIFIER) return NULL;
    for (size_t i = 0; names[i]; i++) {
        if (t.length == strlen(names[i]) && strncmp(t.start, names[i], t.length) == 0) {
            return strcmp(names[i], "i64") == 0 ? "int" : names[i];
        }
    }
    return NULL;
}

/*
 * V0.48: [m/s^2] after float, float32 or a number: the unit, kept in canonical
 * form in node->unit.  The tokens between the brackets are put back together
 * and read by units_normalize.
 */
static int parse_unit(Parser *p, AstNode *node) {
    Token open = p->current;
    next(p);                                         /* [ */
    char text[512];
    size_t n = 0;
    int depth = 0;
    while (!(p->current.kind == TOKEN_RBRACKET && depth == 0)) {
        if (p->current.kind == TOKEN_EOF || p->current.kind == TOKEN_ERROR ||
            p->current.kind == TOKEN_LBRACE || n + p->current.length + 2 >= sizeof(text)) {
            fprintf(stderr, "parse error at %zu:%zu: unit not closed by ']'\n", open.line, open.column);
            p->had_error = 1;
            return 0;
        }
        if (p->current.kind == TOKEN_LPAREN) depth++;
        if (p->current.kind == TOKEN_RPAREN) depth--;
        memcpy(text + n, p->current.start, p->current.length);
        n += p->current.length;
        text[n++] = ' ';
        next(p);
    }
    text[n] = '\0';
    next(p);                                         /* ] */
    char err[160];
    char *canon = units_normalize(text, err, sizeof(err));
    if (!canon) {
        fprintf(stderr, "parse error at %zu:%zu: %s\n", open.line, open.column, err);
        p->had_error = 1;
        return 0;
    }
    free(node->unit);
    node->unit = canon;
    return 1;
}

static int parse_and_set_type(Parser *p, AstNode *node) {
    Token t = p->current;

    /* ptr[float32]: the address of numbers in memory (only inside absolue blocks) */
    if (t.kind == TOKEN_IDENTIFIER && t.length == 3 && strncmp(t.start, "ptr", 3) == 0) {
        next(p);
        if (p->current.kind != TOKEN_LBRACKET) return ast_set_type(node, t.start, t.length);
        next(p);
        const char *element = p->current.kind == TOKEN_FLOAT ? "float" : p->current.kind == TOKEN_INT ? "int"
                            : is_float32_token(p->current) ? "float32" : machine_type_token(p->current);
        if (!element) {
            fprintf(stderr, "parse error at %zu:%zu: ptr element type must be int, float, float32 or u8 ... u64, i8 ... i32\n",
                    p->current.line, p->current.column);
            p->had_error = 1;
            return 0;
        }
        next(p);
        if (!expect(p, TOKEN_RBRACKET, "']'")) return 0;
        char type[32];
        snprintf(type, sizeof(type), "ptr[%s]", element);
        return ast_set_type(node, type, strlen(type));
    }

    /* matrix[float] / matrix[int]: "matrix" stays an ordinary identifier elsewhere */
    if (t.kind == TOKEN_IDENTIFIER && t.length == 6 && strncmp(t.start, "matrix", 6) == 0) {
        next(p);
        if (p->current.kind == TOKEN_LBRACKET) {
            next(p);
            const char *matrix_type = NULL;
            if (p->current.kind == TOKEN_FLOAT) matrix_type = "matrix[float]";
            else if (p->current.kind == TOKEN_INT) matrix_type = "matrix[int]";
            else if (is_float32_token(p->current)) matrix_type = "matrix[float32]";
            else if (machine_type_token(p->current)) {
                static char machine_matrix[32];
                snprintf(machine_matrix, sizeof(machine_matrix), "matrix[%s]", machine_type_token(p->current));
                matrix_type = machine_matrix;
            }
            else {
                fprintf(stderr, "parse error at %zu:%zu: matrix element type must be float, float32, int or u8 ... u64, i8 ... i32\n",
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

    if (machine_type_token(t)) {
        const char *name = machine_type_token(t);
        next(p);
        return ast_set_type(node, name, strlen(name));
    }

    if (t.kind == TOKEN_INT || t.kind == TOKEN_FLOAT ||
        t.kind == TOKEN_BOOL || t.kind == TOKEN_STRING_TYPE ||
        t.kind == TOKEN_IDENTIFIER) {
        next(p);
        if (p->current.kind == TOKEN_LBRACKET) {
            /* V0.48: float[m/s] */
            if (t.kind != TOKEN_FLOAT && !is_float32_token(t)) {
                fprintf(stderr, "parse error at %zu:%zu: a unit goes with float or float32, not %.*s\n",
                        p->current.line, p->current.column, (int)t.length, t.start);
                p->had_error = 1;
                return 0;
            }
            if (!parse_unit(p, node)) return 0;
        }
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
                if (is_float32_token(p->current)) {
                    list_type = "list[float32]";
                    list_type_len = 13;
                    break;
                }
                if (machine_type_token(p->current)) {
                    static char machine_list[32];
                    snprintf(machine_list, sizeof(machine_list), "list[%s]", machine_type_token(p->current));
                    list_type = machine_list;
                    list_type_len = strlen(machine_list);
                    break;
                }
                fprintf(stderr,
                        "parse error at %zu:%zu: list element type must be int, float, float32, bool, string or u8 ... u64, i8 ... i32\n",
                        p->current.line, p->current.column);
                p->had_error = 1;
                return 0;
        }

        Token element = p->current;
        next(p);
        if (p->current.kind == TOKEN_LBRACKET) {
            /* V0.48: list[float[m]] */
            if (element.kind != TOKEN_FLOAT && !is_float32_token(element)) {
                fprintf(stderr, "parse error at %zu:%zu: a unit goes with float or float32\n",
                        p->current.line, p->current.column);
                p->had_error = 1;
                return 0;
            }
            if (!parse_unit(p, node)) return 0;
        }
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

/* "+" for +=, ... ; NULL for any other token */
static const char *compound_operator(TokenKind kind) {
    switch (kind) {
        case TOKEN_PLUS_EQUAL: return "+";
        case TOKEN_MINUS_EQUAL: return "-";
        case TOKEN_STAR_EQUAL: return "*";
        case TOKEN_SLASH_EQUAL: return "/";
        case TOKEN_PERCENT_EQUAL: return "%";
        case TOKEN_AMP_EQUAL: return "&";
        case TOKEN_PIPE_EQUAL: return "|";
        case TOKEN_CARET_EQUAL: return "^";
        case TOKEN_SHL_EQUAL: return "<<";
        case TOKEN_SHR_EQUAL: return ">>";
        default: return NULL;
    }
}

/*
 * A call inside the target of x[...] += v, which would run twice (read, then
 * write).  len, rows, cols, abs, min, max, int and float only compute.
 */
static const AstNode *target_call(const AstNode *n) {
    if (!n) return NULL;
    if (n->kind == AST_METHOD_CALL) return n;
    if (n->kind == AST_CALL && n->text) {
        static const char *const pure[] = { "len", "rows", "cols", "abs", "min", "max", "int", "float", NULL };
        int known = 0;
        for (size_t i = 0; pure[i]; i++) known |= strcmp(n->text, pure[i]) == 0;
        if (!known) return n;
    }
    for (size_t i = 0; i < n->child_count; i++) {
        const AstNode *call = target_call(n->children[i]);
        if (call) return call;
    }
    return NULL;
}

/*
 * Statements that start with an identifier.  The left-hand side is parsed
 * as an ordinary expression, then its shape decides the statement:
 *
 *   name = value                 assignment (also +=, -=, *=, /=, %=)
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

    const char *compound = compound_operator(p->current.kind);
    if (p->current.kind == TOKEN_EQUAL || compound) {
        Token op = p->current;
        next(p);
        AstNode *value = parse_expression(p);
        if (!value) {
            ast_free(lhs);
            return NULL;
        }
        if (compound) {
            /* x += v is x = x + v: same code, same fast loops (s += w[i] * x[i]) */
            if (lhs->kind != AST_IDENTIFIER && lhs->kind != AST_INDEX && lhs->kind != AST_MEMBER) {
                fprintf(stderr, "parse error at %zu:%zu: invalid target for '%s'\n",
                        start.line, start.column, token_kind_name(op.kind));
                p->had_error = 1;
                ast_free(value);
                ast_free(lhs);
                return NULL;
            }
            const AstNode *call = target_call(lhs);
            if (call) {
                fprintf(stderr, "parse error at %zu:%zu: '%s' would call %s() twice: "
                                "put its result in a variable first\n",
                        op.line, op.column, token_kind_name(op.kind), call->text);
                p->had_error = 1;
                ast_free(value);
                ast_free(lhs);
                return NULL;
            }
            AstNode *current = ast_clone(lhs);
            AstNode *bin = current ? ast_new(AST_BINARY, compound, strlen(compound)) : NULL;
            if (!bin || !ast_add_child(bin, current)) {
                ast_free(bin);
                ast_free(current);
                ast_free(value);
                ast_free(lhs);
                return NULL;
            }
            if (!ast_add_child(bin, value)) {
                ast_free(bin);
                ast_free(value);
                ast_free(lhs);
                return NULL;
            }
            value = bin;
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
                if (lhs->kind == AST_CALL && lhs->text && strncmp(lhs->text, "@slice", 6) == 0) {
                    fprintf(stderr, "parse error at %zu:%zu: a slice xs[a:b] cannot be assigned: "
                                    "change the elements one by one\n", start.line, start.column);
                    p->had_error = 1;
                    ast_free(value);
                    ast_free(lhs);
                    return NULL;
                }
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
    if (p->current.kind == TOKEN_ABSOLUE) {
        /* absolue { ... }: kept as `if true { ... }` whose block is marked "absolue" */
        Token at = p->current;
        next(p);
        if (!expect(p, TOKEN_LBRACE, "'{'")) return NULL;
        AstNode *block = parse_block(p);
        if (!block) return NULL;
        if (!expect(p, TOKEN_RBRACE, "'}'")) { ast_free(block); return NULL; }
        free(block->text);
        block->text = malloc(sizeof("absolue"));
        AstNode *test = ast_new(AST_IF, NULL, 0), *yes = ast_new(AST_BOOL, "true", 4);
        if (!block->text || !test || !yes || !ast_add_child(test, yes)) {
            ast_free(block); ast_free(test); ast_free(yes);
            return NULL;
        }
        strcpy(block->text, "absolue");
        if (!ast_add_child(test, block)) { ast_free(block); ast_free(test); return NULL; }
        (void)at;
        return test;
    }
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
        if (stmt->kind == AST_BLOCK && stmt->text && strcmp(stmt->text, "@splice") == 0) {
            /* several statements made from one (enumerate) */
            for (size_t i = 0; i < stmt->child_count; i++) {
                if (!ast_add_child(block, stmt->children[i])) {
                    ast_free(stmt);
                    ast_free(block);
                    return NULL;
                }
                stmt->children[i] = NULL;
            }
            ast_free(stmt);
            continue;
        }
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
    p->hidden_count = 0;
    next(p);
}

/* V0.47: a name, optionally followed by `as alias` (the alias becomes its child) */
static AstNode *parse_import_name(Parser *p) {
    Token name = p->current;
    if (!expect(p, TOKEN_IDENTIFIER, "a module or a name")) return NULL;
    AstNode *node = ast_new(AST_IDENTIFIER, name.start, name.length);
    if (!node) return NULL;
    if (p->current.kind == TOKEN_AS) {
        next(p);
        Token alias = p->current;
        if (!expect(p, TOKEN_IDENTIFIER, "a name after 'as'")) { ast_free(node); return NULL; }
        AstNode *a = ast_new(AST_IDENTIFIER, alias.start, alias.length);
        if (!a || !ast_add_child(node, a)) { ast_free(a); ast_free(node); return NULL; }
    }
    return node;
}

/*
 * import math as m, orbite      -> two AST_IMPORT nodes (text: module, child: alias if any)
 * from astro import GM_mars, UA -> one AST_IMPORT node, type_name "from", children: the names
 */
static int parse_import(Parser *p, AstNode *program) {
    if (p->current.kind == TOKEN_FROM) {
        next(p);
        AstNode *module = parse_import_name(p);
        if (!module) return 0;
        if (module->child_count) {
            fprintf(stderr, "parse error at %zu:%zu: write 'import %s as ...' to rename a module, not 'from'\n",
                    p->current.line, p->current.column, module->text);
            p->had_error = 1;
            ast_free(module);
            return 0;
        }
        AstNode *imp = ast_new(AST_IMPORT, module->text, strlen(module->text));
        ast_free(module);
        if (!imp || !ast_set_type(imp, "from", 4)) { ast_free(imp); return 0; }
        if (!expect(p, TOKEN_IMPORT, "'import'")) { ast_free(imp); return 0; }
        for (;;) {
            AstNode *name = parse_import_name(p);
            if (!name || !ast_add_child(imp, name)) { ast_free(name); ast_free(imp); return 0; }
            if (p->current.kind != TOKEN_COMMA) break;
            next(p);
        }
        if (!ast_add_child(program, imp)) { ast_free(imp); return 0; }
        return 1;
    }
    next(p);                                   /* import */
    for (;;) {
        AstNode *module = parse_import_name(p);
        if (!module) return 0;
        AstNode *imp = ast_new(AST_IMPORT, module->text, strlen(module->text));
        if (!imp) { ast_free(module); return 0; }
        if (module->child_count) {
            AstNode *alias = module->children[0];
            module->child_count = 0;
            if (!ast_add_child(imp, alias)) { ast_free(alias); ast_free(module); ast_free(imp); return 0; }
        }
        ast_free(module);
        if (!ast_add_child(program, imp)) { ast_free(imp); return 0; }
        if (p->current.kind != TOKEN_COMMA) break;
        next(p);
    }
    return 1;
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
        if (p->current.kind == TOKEN_IMPORT || p->current.kind == TOKEN_FROM) {
            if (!parse_import(p, program)) {
                p->had_error = 1;
                ast_free(program);
                return NULL;
            }
            continue;
        }
        if (p->current.kind == TOKEN_CLASS) {
            decl = parse_class(p, program);
        } else if (p->current.kind == TOKEN_DEF) {
            decl = parse_function(p);
        } else if (p->current.kind == TOKEN_ABSOLUE) {
            /* absolue def f(...): its whole body is an absolue block */
            next(p);
            decl = parse_function(p);
            if (decl && decl->child_count > 0) {
                AstNode *body = decl->children[decl->child_count - 1];
                free(body->text);
                body->text = malloc(sizeof("absolue"));
                if (body->text) strcpy(body->text, "absolue");
            }
        } else {
            fprintf(stderr, "parse error at %zu:%zu: expected Class, def or import, got %s\n",
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
