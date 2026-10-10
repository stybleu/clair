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
