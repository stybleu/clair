#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t ast_current_line = 0;
const char *ast_current_file = NULL;

AstNode *ast_new(AstKind kind, const char *text, size_t text_len) {
    AstNode *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->kind = kind;
    n->line = ast_current_line;
    n->file = ast_current_file;
    if (text) {
        n->text = malloc(text_len + 1);
        if (!n->text) { free(n); return NULL; }
        memcpy(n->text, text, text_len);
        n->text[text_len] = '\0';
    }
    return n;
}

int ast_add_child(AstNode *parent, AstNode *child) {
    if (parent->child_count == parent->child_capacity) {
        size_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        AstNode **new_children = realloc(parent->children, new_cap * sizeof(*new_children));
        if (!new_children) return 0;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return 1;
}

int ast_set_type(AstNode *node, const char *type_name, size_t type_len) {
    if (!node) return 0;
    char *copy = NULL;
    if (type_name) {
        copy = malloc(type_len + 1);
        if (!copy) return 0;
        memcpy(copy, type_name, type_len);
        copy[type_len] = '\0';
    }
    free(node->type_name);
    node->type_name = copy;
    return 1;
}

AstNode *ast_clone(const AstNode *node) {
    if (!node) return NULL;
    AstNode *copy = ast_new(node->kind, node->text, node->text ? strlen(node->text) : 0);
    if (!copy) return NULL;
    copy->line = node->line;
    copy->file = node->file;
    if (node->type_name && !ast_set_type(copy, node->type_name, strlen(node->type_name))) {
        ast_free(copy);
        return NULL;
    }
    if (node->unit) {
        copy->unit = malloc(strlen(node->unit) + 1);
        if (!copy->unit) { ast_free(copy); return NULL; }
        strcpy(copy->unit, node->unit);
    }
    for (size_t i = 0; i < node->child_count; i++) {
        AstNode *child = ast_clone(node->children[i]);
        if ((node->children[i] && !child) || !ast_add_child(copy, child)) {
            ast_free(child);
            ast_free(copy);
            return NULL;
        }
    }
    return copy;
}

void ast_free(AstNode *node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) ast_free(node->children[i]);
    free(node->children);
    free(node->text);
    free(node->type_name);
    free(node->unit);
    free(node);
}

int clair_decimal_to_words(const char *text, uint64_t *out, int words, int is_unsigned) {
    /* 32-bit halves keep the arithmetic portable (no 128-bit type needed) */
    uint32_t half[2 * 16 + 1] = { 0 };
    int halves = 2 * words;
    int negative = 0;
    if (!text || words < 1 || words > 16) return 0;
    if (*text == '-') { negative = 1; text++; }
    if (!*text) return 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        uint64_t carry = (uint64_t)(*p - '0');
        for (int i = 0; i < halves; i++) {
            uint64_t v = (uint64_t)half[i] * 10u + carry;
            half[i] = (uint32_t)v;
            carry = v >> 32;
        }
        if (carry) return 0;                      /* does not fit even unsigned */
    }
    for (int i = 0; i < words; i++) out[i] = (uint64_t)half[2 * i] | ((uint64_t)half[2 * i + 1] << 32);

    if (is_unsigned) {                            /* 0 .. 2^(64w) - 1 */
        if (!negative) return 1;
        for (int i = 0; i < words; i++) if (out[i]) return 0;
        return 1;                                 /* -0 */
    }
    int top_bit = (int)(out[words - 1] >> 63);
    if (!negative) return !top_bit;               /* at most 2^(64w-1) - 1 */
    if (top_bit) {                                /* only 2^(64w-1) itself */
        if (out[words - 1] != (UINT64_C(1) << 63)) return 0;
        for (int i = 0; i < words - 1; i++) if (out[i]) return 0;
    }
    uint64_t carry = 1;                           /* two's complement negation */
    for (int i = 0; i < words; i++) {
        out[i] = ~out[i] + carry;
        carry = carry && out[i] == 0;
    }
    return 1;
}

int clair_integer_to_decimal(const char *text, size_t len, char *out, size_t out_size) {
    /* the number as 32-bit halves (1056 bits: anything Clair can hold, int1024 included) */
    uint32_t half[33] = { 0 };
    const int halves = 33;
    unsigned base = 10;
    size_t i = 0;
    if (len > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { base = 16; i = 2; }
    else if (len > 2 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) { base = 2; i = 2; }
    else if (len > 2 && text[0] == '0' && (text[1] == 'o' || text[1] == 'O')) { base = 8; i = 2; }
    int digits = 0;
    /* like Python: 1_000 and 0x_FF, but not 1__0 nor 1_; no 007 (in C it would be octal) */
    int after_digit = base != 10;
    if (base == 10 && len > 1 && text[0] == '0') {
        for (size_t k = 1; k < len; k++) if (text[k] != '0' && text[k] != '_') return 0;
    }
    for (; i < len; i++) {
        char c = text[i];
        if (c == '_') {
            if (!after_digit) return 0;
            after_digit = 0;
            continue;
        }
        after_digit = 1;
        unsigned d;
        if (c >= '0' && c <= '9') d = (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (unsigned)(c - 'A' + 10);
        else return 0;
        if (d >= base) return 0;
        uint64_t carry = d;
        for (int k = 0; k < halves; k++) {
            uint64_t v = (uint64_t)half[k] * base + carry;
            half[k] = (uint32_t)v;
            carry = v >> 32;
        }
        if (carry) return 0;                          /* far too large */
        digits++;
    }
    if (!digits || !after_digit) return 0;
    /* to decimal: divide by 10^9 until nothing is left */
    char groups[40][10];
    int count = 0;
    for (;;) {
        uint64_t rem = 0;
        int more = 0;
        for (int k = halves - 1; k >= 0; k--) {
            uint64_t cur = (rem << 32) | half[k];
            half[k] = (uint32_t)(cur / 1000000000u);
            rem = cur % 1000000000u;
            if (half[k]) more = 1;
        }
        snprintf(groups[count], sizeof(groups[0]), "%09u", (unsigned)rem);
        count++;
        if (!more) break;
    }
    size_t used = 0;
    for (int k = count - 1; k >= 0; k--) {
        const char *g = groups[k];
        if (k == count - 1) {               /* the leading group without its zeros */
            while (g[0] == '0' && g[1]) g++;
        }
        size_t n = strlen(g);
        if (used + n + 1 > out_size) return 0;
        memcpy(out + used, g, n);
        used += n;
    }
    out[used] = '\0';
    return 1;
}

