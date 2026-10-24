#include "ast.h"
#include <stdlib.h>
#include <string.h>

AstNode *ast_new(AstKind kind, const char *text, size_t text_len) {
    AstNode *n = calloc(1, sizeof(*n));
    if (!n) return NULL;
    n->kind = kind;
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

void ast_free(AstNode *node) {
    if (!node) return;
    for (size_t i = 0; i < node->child_count; i++) ast_free(node->children[i]);
    free(node->children);
    free(node->text);
    free(node->type_name);
    free(node);
}

int clair_decimal_to_words(const char *text, uint64_t *out, int words) {
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
