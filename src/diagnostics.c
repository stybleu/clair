#include "diagnostics.h"
#include <stdio.h>
#include <string.h>
static const char *source, *filename;
static int enabled;

/* CLAIR_V14_TYPE_DIAGNOSTIC */
static const char *clair_v14_source = NULL;

/* CLAIR_V191_PLUS_AST */
static size_t clair_v19_plus_column = 0;

void clair_v19_set_plus_column(size_t column) {
    clair_v19_plus_column = column;
}

static size_t clair_v14_plus_column(size_t line) {
    if (!clair_v14_source || !line) return 0;
    const char *p = clair_v14_source;

    for (size_t i = 1; i < line; ++i) {
        p = strchr(p, '\n');
        if (!p) return 0;
        ++p;
    }

    int quote = 0, escaped = 0;
    size_t found = 0;

    for (size_t col = 1;
         *p && *p != '\n' && *p != '\r';
         ++p, ++col) {

        const unsigned char ch = (unsigned char)*p;

        if (quote) {
            if (escaped) escaped = 0;
            else if (ch == '\\') escaped = 1;
            else if (ch == (unsigned char)quote) quote = 0;
        } else if (ch == '"' || ch == '\'') {
            quote = ch;
        } else if (ch == '/' && p[1] == '/') {
            break;
        } else if (ch == '+') {
            if (found) return 0;
            found = col;
        }
    }
    return found;
}

/* CLAIR_V15_CODES */
static const char *clair_v15_code(char type, const char *msg) {
    if (type != 'S' || !msg) return "001";

    if (strstr(msg, "incompatible types for '+'"))
        return "002";

    if (strstr(msg, "cannot initialize ") ||
        strstr(msg, "cannot assign "))
        return "003";

    if (strstr(msg, "function '") &&
        strstr(msg, " expects ") &&
        strstr(msg, "argument(s), got "))
        return "004";

    if (strstr(msg, "return expects ") &&
        strstr(msg, ", got "))
        return "005";

    if (strstr(msg, "arithmetic operators require int or float operands") ||
        strstr(msg, "arithmetic operators '-', '*' and '/' require int or float operands") ||
        strstr(msg, "operator '%' requires int or float operands"))
        return "006";

    return "001";
}

static const char *clair_v15_hint(
    char type, const char *msg, const char *default_hint
) {
    const char *code = clair_v15_code(type, msg);

    if (!strcmp(code, "002"))
        return "Pour +, utilise deux nombres ou deux chaines de caracteres.";

    if (!strcmp(code, "003")) {
        if (strstr(msg, "cannot initialize int ") &&
            strstr(msg, " with float"))
            return "float vers int n'est pas implicite : adapte le type ou convertis explicitement la valeur.";
        if (strstr(msg, "cannot initialize "))
            return "Le type declare doit accepter la valeur initiale; verifie les deux types.";
        if (strstr(msg, "cannot assign "))
            return "La variable doit accepter la valeur affectee; adapte ou convertis celle-ci.";
        return "Verifie le type declare et le type de la valeur affectee.";
    }

    if (!strcmp(code, "004"))
        return "Verifie le nombre de parametres de la fonction appelee.";

    if (!strcmp(code, "005"))
        return "La valeur retournee doit correspondre au type apres ->.";

    if (!strcmp(code, "006"))
        return "Cette operation exige des valeurs numeriques compatibles.";

    return default_hint;
}


/* CLAIR_V17_LAYOUT */
static int clair_v17_active = 0;
static char clair_v17_pending[100] = {0};

void clair_v17_set_suggestion(const char *name) {
    if (!name) {
        clair_v17_pending[0] = '\0';
        return;
    }
    snprintf(clair_v17_pending,
             sizeof(clair_v17_pending), "%s", name);
}

int clair_v17_diag_enabled(void) {
    return clair_v17_active;
}


/* CLAIR_V18_SPANS */
static const char *clair_v18_source = NULL;

static const char *clair_v18_markers(
    const char *msg, size_t line, size_t col
) {
    static char markers[66];

    markers[0] = '^';
    markers[1] = '\0';

    if (!msg || !clair_v18_source ||
        !line || !col || col > 8192)
        return markers;

    const char *tag = "undefined variable '";
    const char *name = strstr(msg, tag);

    if (!name) return markers;

    name += strlen(tag);

    const char *stop = strchr(name, '\'');
    if (!stop) return markers;

    size_t n = (size_t)(stop - name);

    if (n < 2 || n > 64)
        return markers;

    const char *p = clair_v18_source;

    for (size_t k = 1; k < line; ++k) {
        p = strchr(p, '\n');
        if (!p) return markers;
        ++p;
    }

    const char *end = strchr(p, '\n');
    if (!end) end = p + strlen(p);

    size_t width = (size_t)(end - p);

    if (col - 1 > width ||
        n > width - (col - 1))
        return markers;

    if (strncmp(p + col - 1, name, n) != 0)
        return markers;

    if (col > 1) {
        char c = p[col - 2];

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_')
            return markers;
    }

    if (col - 1 + n < width) {
        char c = p[col - 1 + n];

        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_')
            return markers;
    }

    for (size_t k = 1; k < n; ++k)
        markers[k] = '~';

    markers[n] = '\0';
    return markers;
}
void clair_diag_init(const char *name, const char *src, int on) {
    clair_v18_source = src;
    clair_v17_active = !!on;
    clair_v17_pending[0] = '\0';
    clair_v14_source = src;
    filename = name; source = src; enabled = on;
}
void clair_diag_show(char type, const char *file, size_t line, size_t col, const char *msg) {
    if (!enabled || !line) return;
    const char *name = file ? file : filename;
    const char *base = filename ? strrchr(filename, 47) : NULL;
    base = base ? base + 1 : filename;
    if (source && (!file || (base && strcmp(file, base) == 0))) {
        const char *p = source;
        for (size_t i = 1; i < line && p; ++i) {
            const char *next = strchr(p, 10); p = next ? next + 1 : NULL;
        }
        if (p) {
            const char *end = strchr(p, 10);
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (len > 140) len = 140;
            const char *id = msg ? strstr(msg, "undefined variable") : NULL;
            if (id) { const char *a = strchr(id, 39); if (a) { ++a; const char *b = strchr(a, 39); if (b) { size_t n = (size_t)(b-a); for (size_t j=0; n && j+n<=len; ++j) if (!memcmp(p+j,a,n)) { col=j+1; break; } } } }
            if (col < 1) col = 1;
            if (col > len + 1) col = len + 1;
    int clair_v14_s002 = (type == 'S' &&
        strstr(msg, "incompatible types for '+'") != NULL);
    if (clair_v14_s002) {
        size_t exact = (clair_v19_plus_column ? clair_v19_plus_column : clair_v14_plus_column(line));
        if (exact) col = exact;
    }
            fprintf(stderr, "  --> %s:%zu:%zu\n", name ? name : "<source>", line, col);
            fprintf(stderr, "  %zu | %.*s\n", line, (int)len, p);
            fprintf(stderr, "     | %*s%s\n", (int)(col - 1), "", clair_v18_markers(msg, line, col));
        }
    }
    if (!source || (file && (!base || strcmp(file, base) != 0))) fprintf(stderr, "  --> %s:%zu:%zu\n", name ? name : "<source>", line, col ? col : 1);
    const char *hint = "Verifiez la syntaxe, les types et les declarations.";
    if (msg && strstr(msg, "undefined variable")) hint = "Declare cette variable ou verifie son orthographe.";
    else if (msg && strstr(msg, "expected expression")) hint = "Une expression manque apres un operateur ou un signe egal.";
    else if (msg && strstr(msg, "expected Class")) hint = "Place let et print dans une fonction def main().";
    else if (msg && strstr(msg, "already declared")) hint = "Utilise un autre nom ou une affectation.";
    if (type == 'S' &&
        strstr(msg, "undefined variable '") &&
        clair_v17_pending[0]) {
        fprintf(stderr,
                " suggestion : Vouliez-vous dire '%s' ?\n",
                clair_v17_pending);
    }
    clair_v17_pending[0] = '\0';
    fprintf(stderr, " aide [CLAIR-%c%s] : %s\n", type,
        clair_v15_code(type, msg),
        clair_v15_hint(type, msg, hint));
}
