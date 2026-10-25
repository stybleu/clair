#include "lexer.h"
#include <ctype.h>
#include <string.h>

static char peek(const Lexer *l) { return l->source[l->pos]; }
static char peek_next(const Lexer *l) { return l->source[l->pos + 1]; }

static char advance(Lexer *l) {
    char c = l->source[l->pos++];
    if (c == '\n') { l->line++; l->column = 1; }
    else { l->column++; }
    return c;
}

static Token make_token(TokenKind kind, const char *start, size_t length, size_t line, size_t column) {
    Token t = { kind, start, length, line, column };
    return t;
}

static int is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_ident_part(char c) { return isalnum((unsigned char)c) || c == '_'; }

static TokenKind keyword_kind(const char *s, size_t n) {
    if (n == 5 && strncmp(s, "Class", 5) == 0) return TOKEN_CLASS;
    if (n == 3 && strncmp(s, "def", 3) == 0) return TOKEN_DEF;
    if (n == 7 && strncmp(s, "absolue", 7) == 0) return TOKEN_ABSOLUE;
    if (n == 6 && strncmp(s, "return", 6) == 0) return TOKEN_RETURN;
    if (n == 3 && strncmp(s, "let", 3) == 0) return TOKEN_LET;
    if (n == 3 && strncmp(s, "int", 3) == 0) return TOKEN_INT;
    if (n == 5 && strncmp(s, "float", 5) == 0) return TOKEN_FLOAT;
    if (n == 4 && strncmp(s, "bool", 4) == 0) return TOKEN_BOOL;
    if (n == 6 && strncmp(s, "string", 6) == 0) return TOKEN_STRING_TYPE;
    if (n == 4 && strncmp(s, "list", 4) == 0) return TOKEN_LIST;
    if (n == 4 && strncmp(s, "dict", 4) == 0) return TOKEN_DICT;
    if (n == 4 && strncmp(s, "true", 4) == 0) return TOKEN_TRUE;
    if (n == 5 && strncmp(s, "false", 5) == 0) return TOKEN_FALSE;
    if (n == 3 && strncmp(s, "and", 3) == 0) return TOKEN_AND;
    if (n == 2 && strncmp(s, "or", 2) == 0) return TOKEN_OR;
    if (n == 3 && strncmp(s, "not", 3) == 0) return TOKEN_NOT;
    if (n == 5 && strncmp(s, "print", 5) == 0) return TOKEN_PRINT;
    if (n == 2 && strncmp(s, "if", 2) == 0) return TOKEN_IF;
    if (n == 4 && strncmp(s, "else", 4) == 0) return TOKEN_ELSE;
    if (n == 5 && strncmp(s, "while", 5) == 0) return TOKEN_WHILE;
    if (n == 3 && strncmp(s, "for", 3) == 0) return TOKEN_FOR;
    if (n == 2 && strncmp(s, "in", 2) == 0) return TOKEN_IN;
    if (n == 5 && strncmp(s, "range", 5) == 0) return TOKEN_RANGE;
    if (n == 5 && strncmp(s, "break", 5) == 0) return TOKEN_BREAK;
    if (n == 8 && strncmp(s, "continue", 8) == 0) return TOKEN_CONTINUE;
    return TOKEN_IDENTIFIER;
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
}

Token lexer_next(Lexer *l) {
    for (;;) {
        char c = peek(l);
        if (c == '\0') return make_token(TOKEN_EOF, l->source + l->pos, 0, l->line, l->column);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(l); continue; }
        if (c == '/' && peek_next(l) == '/') {
            while (peek(l) != '\0' && peek(l) != '\n') advance(l);
            continue;
        }
        break;
    }

    size_t line = l->line, column = l->column, start_pos = l->pos;
    char c = advance(l);

    if (is_ident_start(c)) {
        while (is_ident_part(peek(l))) advance(l);
        size_t len = l->pos - start_pos;
        return make_token(keyword_kind(l->source + start_pos, len), l->source + start_pos, len, line, column);
    }

    if (isdigit((unsigned char)c)) {
        int is_float = 0;
        char kind = (char)peek(l);
        if (c == '0' && (kind == 'x' || kind == 'X' || kind == 'b' || kind == 'B' || kind == 'o' || kind == 'O')) {
            /* 0xFF, 0b1010, 0o17 (checked and turned into decimal by the parser) */
            advance(l);
            while (isalnum((unsigned char)peek(l)) || peek(l) == '_') advance(l);
            return make_token(TOKEN_INTEGER, l->source + start_pos, l->pos - start_pos, line, column);
        }
        while (isdigit((unsigned char)peek(l)) || (peek(l) == '_' && isdigit((unsigned char)peek_next(l)))) advance(l);
        if (isalpha((unsigned char)peek(l)) && peek(l) != 'e' && peek(l) != 'E') {
            /* 12abc or 1_: kept in one token, refused by the parser as "invalid number" */
            while (isalnum((unsigned char)peek(l)) || peek(l) == '_') advance(l);
            return make_token(TOKEN_INTEGER, l->source + start_pos, l->pos - start_pos, line, column);
        }
        if (peek(l) == '_') {
            while (isalnum((unsigned char)peek(l)) || peek(l) == '_') advance(l);
            return make_token(TOKEN_INTEGER, l->source + start_pos, l->pos - start_pos, line, column);
        }
        if (peek(l) == '.' && isdigit((unsigned char)peek_next(l))) {
            is_float = 1;
            advance(l);
            while (isdigit((unsigned char)peek(l)) || (peek(l) == '_' && isdigit((unsigned char)peek_next(l)))) advance(l);
        }
        /* scientific notation: 1e-3, 6.02e23 */
        if (peek(l) == 'e' || peek(l) == 'E') {
            size_t look = l->pos + 1;
            if (l->source[look] == '+' || l->source[look] == '-') look++;
            if (isdigit((unsigned char)l->source[look])) {
                is_float = 1;
                while (l->pos < look) advance(l);
                while (isdigit((unsigned char)peek(l)) || (peek(l) == '_' && isdigit((unsigned char)peek_next(l)))) advance(l);
            }
        }
        return make_token(is_float ? TOKEN_FLOAT_LITERAL : TOKEN_INTEGER,
                          l->source + start_pos, l->pos - start_pos, line, column);
    }

    if (c == '"') {
        const char *content = l->source + l->pos;
        size_t content_line = line, content_col = column;
        while (peek(l) != '\0' && peek(l) != '"') {
            if (peek(l) == '\\' && peek_next(l) != '\0') { advance(l); advance(l); }
            else advance(l);
        }
        if (peek(l) != '"') return make_token(TOKEN_ERROR, content, l->pos - start_pos, content_line, content_col);
        size_t len = (size_t)((l->source + l->pos) - content);
        advance(l);
        return make_token(TOKEN_STRING, content, len, line, column);
    }

    switch (c) {
        case '(': return make_token(TOKEN_LPAREN, l->source + start_pos, 1, line, column);
        case ')': return make_token(TOKEN_RPAREN, l->source + start_pos, 1, line, column);
        case '{': return make_token(TOKEN_LBRACE, l->source + start_pos, 1, line, column);
        case '}': return make_token(TOKEN_RBRACE, l->source + start_pos, 1, line, column);
        case '[': return make_token(TOKEN_LBRACKET, l->source + start_pos, 1, line, column);
        case ']': return make_token(TOKEN_RBRACKET, l->source + start_pos, 1, line, column);
        case ':': return make_token(TOKEN_COLON, l->source + start_pos, 1, line, column);
        case ',': return make_token(TOKEN_COMMA, l->source + start_pos, 1, line, column);
        case '.': return make_token(TOKEN_DOT, l->source + start_pos, 1, line, column);
        case '+':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_PLUS_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_PLUS, l->source + start_pos, 1, line, column);
        case '*':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_STAR_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_STAR, l->source + start_pos, 1, line, column);
        case '/':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_SLASH_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_SLASH, l->source + start_pos, 1, line, column);
        case '%':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_PERCENT_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_PERCENT, l->source + start_pos, 1, line, column);
        case '&':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_AMP_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_AMP, l->source + start_pos, 1, line, column);
        case '|':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_PIPE_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_PIPE, l->source + start_pos, 1, line, column);
        case '^':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_CARET_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_CARET, l->source + start_pos, 1, line, column);
        case '~': return make_token(TOKEN_TILDE, l->source + start_pos, 1, line, column);
        case '=':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_EQUAL_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_EQUAL, l->source + start_pos, 1, line, column);
        case '!':
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_BANG_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_ERROR, l->source + start_pos, 1, line, column);
        case '<':
            if (peek(l) == '<') {
                advance(l);
                if (peek(l) == '=') { advance(l); return make_token(TOKEN_SHL_EQUAL, l->source + start_pos, 3, line, column); }
                return make_token(TOKEN_SHL, l->source + start_pos, 2, line, column);
            }
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_LESS_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_LESS, l->source + start_pos, 1, line, column);
        case '>':
            if (peek(l) == '>') {
                advance(l);
                if (peek(l) == '=') { advance(l); return make_token(TOKEN_SHR_EQUAL, l->source + start_pos, 3, line, column); }
                return make_token(TOKEN_SHR, l->source + start_pos, 2, line, column);
            }
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_GREATER_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_GREATER, l->source + start_pos, 1, line, column);
        case '-':
            if (peek(l) == '>') { advance(l); return make_token(TOKEN_ARROW, l->source + start_pos, 2, line, column); }
            if (peek(l) == '=') { advance(l); return make_token(TOKEN_MINUS_EQUAL, l->source + start_pos, 2, line, column); }
            return make_token(TOKEN_MINUS, l->source + start_pos, 1, line, column);
        default: return make_token(TOKEN_ERROR, l->source + start_pos, 1, line, column);
    }
}

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOKEN_EOF: return "EOF"; case TOKEN_IDENTIFIER: return "identifier"; case TOKEN_INTEGER: return "integer";
        case TOKEN_FLOAT_LITERAL: return "float literal"; case TOKEN_STRING: return "string"; case TOKEN_CLASS: return "Class"; case TOKEN_DEF: return "def"; case TOKEN_ABSOLUE: return "absolue"; case TOKEN_RETURN: return "return";
        case TOKEN_LET: return "let"; case TOKEN_INT: return "int"; case TOKEN_FLOAT: return "float"; case TOKEN_BOOL: return "bool"; case TOKEN_STRING_TYPE: return "string"; case TOKEN_LIST: return "list"; case TOKEN_DICT: return "dict";
        case TOKEN_TRUE: return "true"; case TOKEN_FALSE: return "false"; case TOKEN_AND: return "and";
        case TOKEN_OR: return "or"; case TOKEN_NOT: return "not"; case TOKEN_PRINT: return "print";
        case TOKEN_IF: return "if"; case TOKEN_ELSE: return "else"; case TOKEN_WHILE: return "while";
        case TOKEN_FOR: return "for"; case TOKEN_IN: return "in"; case TOKEN_RANGE: return "range";
        case TOKEN_BREAK: return "break"; case TOKEN_CONTINUE: return "continue";
        case TOKEN_LPAREN: return "("; case TOKEN_RPAREN: return ")"; case TOKEN_LBRACE: return "{"; case TOKEN_RBRACE: return "}"; case TOKEN_LBRACKET: return "["; case TOKEN_RBRACKET: return "]";
        case TOKEN_COLON: return ":"; case TOKEN_COMMA: return ","; case TOKEN_DOT: return "."; case TOKEN_ARROW: return "->"; case TOKEN_EQUAL: return "=";
        case TOKEN_EQUAL_EQUAL: return "=="; case TOKEN_BANG_EQUAL: return "!="; case TOKEN_LESS: return "<";
        case TOKEN_LESS_EQUAL: return "<="; case TOKEN_GREATER: return ">"; case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_PLUS: return "+"; case TOKEN_MINUS: return "-"; case TOKEN_STAR: return "*"; case TOKEN_SLASH: return "/"; case TOKEN_PERCENT: return "%";
        case TOKEN_PLUS_EQUAL: return "+="; case TOKEN_MINUS_EQUAL: return "-="; case TOKEN_STAR_EQUAL: return "*=";
        case TOKEN_SLASH_EQUAL: return "/="; case TOKEN_PERCENT_EQUAL: return "%=";
        case TOKEN_AMP: return "&"; case TOKEN_PIPE: return "|"; case TOKEN_CARET: return "^"; case TOKEN_TILDE: return "~";
        case TOKEN_SHL: return "<<"; case TOKEN_SHR: return ">>"; case TOKEN_AMP_EQUAL: return "&=";
        case TOKEN_PIPE_EQUAL: return "|="; case TOKEN_CARET_EQUAL: return "^="; case TOKEN_SHL_EQUAL: return "<<=";
        case TOKEN_SHR_EQUAL: return ">>=";
        case TOKEN_ERROR: return "error";
    }
    return "unknown";
}
