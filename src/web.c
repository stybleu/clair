/*
 * V0.55: the routes of a web program.
 *
 *   import web
 *
 *   @web.route("/bonjour/<nom>")
 *   def bonjour(nom: string) -> string {
 *       return "<h1>Bonjour " + web.escape(nom) + "</h1>"
 *   }
 *
 * Before the modules are read, every @web.route is checked against the
 * function under it, here, at compile time:
 *   - each <name> of the path is a parameter of that name, of type string,
 *     int or float (/carre/abc does not reach carre(n: int): it is a 404);
 *   - every other parameter has the type Request (the request);
 *   - the function gives a string (an HTML page) or a Response;
 *   - the methods are among GET POST PUT DELETE PATCH OPTIONS, and no method
 *     and path are taken twice.
 * Then the function the server calls for every request, __web_handle(), is
 * written in Clair and joins the program.  The routes are tried from the most
 * precise to the least: /a/b before /a/<x>, an int before a float before a
 * string; a path known with another method answers 405; anything else is a
 * file of static/ (or a PHP page), or 404.
 */
#include "web.h"
#include "parser.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEB_ROUTES_MAX 1024
#define WEB_SEGS_MAX 32

static const char *const method_names[] = { "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS" };
#define METHOD_COUNT 6

typedef struct {
    const AstNode *fn;
    size_t line;
    size_t order;
    unsigned methods;              /* bit i: method_names[i] */
    size_t nseg;
    char *seg[WEB_SEGS_MAX];       /* a literal segment, or the name of a <variable> */
    int kind[WEB_SEGS_MAX];        /* 0 literal, 1 int, 2 float, 3 string */
} Route;

static int failed = 0;

static void web_error(size_t line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("web error: ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (line) fprintf(stderr, " (line %zu)", line);
    fputc('\n', stderr);
    failed = 1;
}

/* ---- a growing text ---- */

typedef struct { char *p; size_t len, cap; } Text;

static void put(Text *t, const char *s) {
    size_t n = strlen(s);
    if (t->len + n + 1 > t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 1024;
        while (cap < t->len + n + 1) cap *= 2;
        char *p = realloc(t->p, cap);
        if (!p) { failed = 1; return; }
        t->p = p;
        t->cap = cap;
    }
    memcpy(t->p + t->len, s, n + 1);
    t->len += n;
}

static void putf(Text *t, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    put(t, buf);
}

/* a Clair string literal: "a\"b" */
static void put_literal(Text *t, const char *s) {
    put(t, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char c[8];
        if (*p == '"') put(t, "\\\"");
        else if (*p == '\\') put(t, "\\\\");
        else if (*p == '\n') put(t, "\\n");
        else if (*p == '\r') put(t, "\\r");
        else if (*p == '\t') put(t, "\\t");
        else if (*p < 0x20) { snprintf(c, sizeof(c), "\\x%02x", *p); put(t, c); }
        else { c[0] = (char)*p; c[1] = '\0'; put(t, c); }
    }
    put(t, "\"");
}

static char *dup_n(const char *s, size_t n) {
    char *d = malloc(n + 1);
    if (!d) { failed = 1; return NULL; }
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static int is_name(const char *s, size_t n) {
    if (n == 0 || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_')) return 0;
    for (size_t i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static size_t param_count(const AstNode *fn) { return fn->child_count ? fn->child_count - 1 : 0; }

static const AstNode *param_named(const AstNode *fn, const char *name) {
    for (size_t i = 0; i < param_count(fn); i++) {
        const AstNode *p = fn->children[i];
        if (p->kind == AST_PARAM && p->text && strcmp(p->text, name) == 0) return p;
    }
    return NULL;
}

/* "GET POST", "get,post": the bits of the methods, 0 after an error */
static unsigned parse_methods(const char *s, size_t line) {
    unsigned bits = 0;
    const char *p = s;
    while (*p) {
        while (*p == ' ' || *p == ',' || *p == '|' || *p == '+') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ' ' && *p != ',' && *p != '|' && *p != '+') p++;
        size_t n = (size_t)(p - start);
        char word[16];
        if (n >= sizeof(word)) n = sizeof(word) - 1;
        for (size_t i = 0; i < n; i++) word[i] = (char)(start[i] >= 'a' && start[i] <= 'z' ? start[i] - 32 : start[i]);
        word[n] = '\0';
        int found = -1;
        for (int i = 0; i < METHOD_COUNT; i++) if (strcmp(word, method_names[i]) == 0) found = i;
        if (strcmp(word, "HEAD") == 0) found = 0;
        if (found < 0) {
            web_error(line, "unknown method '%s' in @web.route: GET, POST, PUT, DELETE, PATCH or OPTIONS", word);
            return 0;
        }
        bits |= 1u << found;
    }
    if (!bits) web_error(line, "@web.route needs at least one method, like \"GET POST\"");
    return bits;
}

/* one @web.route(path [, methods]) on fn */
static int read_route(const AstNode *fn, const AstNode *call, int is_method, Route *r) {
    size_t line = call->line ? call->line : fn->line;
    size_t first = is_method ? 1 : 0, argc = call->child_count - first;
    if (argc < 1 || argc > 2) {
        web_error(line, "@web.route takes the path and maybe the methods: @web.route(\"/path\") or @web.route(\"/path\", \"GET POST\")");
        return 0;
    }
    const AstNode *path = call->children[first];
    if (path->kind != AST_STRING || !path->text) {
        web_error(line, "the path of @web.route is written in quotes: @web.route(\"/path\")");
        return 0;
    }
    memset(r, 0, sizeof(*r));
    r->fn = fn;
    r->line = line;
    r->methods = 1;                                     /* GET */
    if (argc == 2) {
        const AstNode *m = call->children[first + 1];
        if (m->kind != AST_STRING || !m->text) {
            web_error(line, "the methods of @web.route are written in quotes: \"GET POST\"");
            return 0;
        }
        r->methods = parse_methods(m->text, line);
        if (!r->methods) return 0;
    }
    const char *s = path->text;
    if (s[0] != '/') {
        web_error(line, "a path starts with /, got \"%s\"", s);
        return 0;
    }
    while (*s) {
        while (*s == '/') s++;
        if (!*s) break;
        const char *start = s;
        while (*s && *s != '/') s++;
        size_t n = (size_t)(s - start);
        if (r->nseg >= WEB_SEGS_MAX) {
            web_error(line, "a path has at most %d parts", WEB_SEGS_MAX);
            return 0;
        }
        if (start[0] == '<') {
            if (n < 3 || start[n - 1] != '>' || !is_name(start + 1, n - 2)) {
                web_error(line, "write a part of the path that changes as <name>, got \"%.*s\"", (int)n, start);
                return 0;
            }
            char *name = dup_n(start + 1, n - 2);
            if (!name) return 0;
            for (size_t i = 0; i < r->nseg; i++) {
                if (r->kind[i] && strcmp(r->seg[i], name) == 0) {
                    web_error(line, "<%s> is twice in the path \"%s\"", name, path->text);
                    free(name);
                    return 0;
                }
            }
            const AstNode *p = param_named(fn, name);
            if (!p) {
                web_error(line, "%s() has no parameter %s for <%s> of \"%s\"", fn->text, name, name, path->text);
                free(name);
                return 0;
            }
            const char *t = p->type_name ? p->type_name : "?";
            int kind = strcmp(t, "int") == 0 ? 1 : strcmp(t, "float") == 0 ? 2 : strcmp(t, "string") == 0 ? 3 : 0;
            if (!kind || p->unit) {
                web_error(line, "the parameter %s of %s() comes from the path: it is a string, an int or a float%s, not %s",
                          name, fn->text, p->unit ? " without unit" : "", t);
                free(name);
                return 0;
            }
            r->seg[r->nseg] = name;
            r->kind[r->nseg++] = kind;
        } else {
            if (memchr(start, '<', n) || memchr(start, '>', n)) {
                web_error(line, "write a part of the path that changes as <name>, got \"%.*s\"", (int)n, start);
                return 0;
            }
            r->seg[r->nseg] = dup_n(start, n);
            if (!r->seg[r->nseg]) return 0;
            r->kind[r->nseg++] = 0;
        }
    }
    /* every parameter comes from the path, or is the request */
    int requests = 0;
    for (size_t i = 0; i < param_count(fn); i++) {
        const AstNode *p = fn->children[i];
        if (p->type_name && strcmp(p->type_name, "Request") == 0) {
            if (++requests > 1) {
                web_error(line, "%s() takes the request once", fn->text);
                return 0;
            }
            continue;
        }
        int in_path = 0;
        for (size_t k = 0; k < r->nseg; k++) in_path |= r->kind[k] && strcmp(r->seg[k], p->text) == 0;
        if (!in_path) {
            web_error(line, "the parameter %s of %s() is not in the path \"%s\": add <%s>, or give it the type Request",
                      p->text, fn->text, path->text, p->text);
            return 0;
        }
    }
    const char *ret = fn->type_name ? fn->type_name : "?";
    if (strcmp(ret, "string") != 0 && strcmp(ret, "Response") != 0) {
        web_error(line, "%s() answers a request: it gives a string (a page) or a Response, not %s", fn->text, ret);
        return 0;
    }
    if (strcmp(fn->text, "main") == 0) {
        web_error(line, "main() cannot be a route");
        return 0;
    }
    return 1;
}

/* the most precise route first: a literal before a variable, int before float before string */
static int route_cmp(const void *a, const void *b) {
    const Route *x = a, *y = b;
    if (x->nseg != y->nseg) return x->nseg < y->nseg ? -1 : 1;
    for (size_t i = 0; i < x->nseg; i++) {
        if (x->kind[i] != y->kind[i]) return x->kind[i] < y->kind[i] ? -1 : 1;
        if (x->kind[i] == 0) {
            int c = strcmp(x->seg[i], y->seg[i]);
            if (c) return c;
        }
    }
    return x->order < y->order ? -1 : x->order > y->order;
}

static int same_shape(const Route *x, const Route *y) {
    if (x->nseg != y->nseg) return 0;
    for (size_t i = 0; i < x->nseg; i++) {
        if (x->kind[i] != y->kind[i]) return 0;
        if (x->kind[i] == 0 && strcmp(x->seg[i], y->seg[i]) != 0) return 0;
    }
    return 1;
}

static void shape_text(const Route *r, char *out, size_t n) {
    size_t w = 0;
    out[0] = '\0';
    if (r->nseg == 0) { snprintf(out, n, "/"); return; }
    for (size_t i = 0; i < r->nseg && w + 2 < n; i++) {
        int k = snprintf(out + w, n - w, r->kind[i] ? "/<%s>" : "/%s", r->seg[i]);
        if (k < 0) break;
        w += (size_t)k;
    }
}

static void write_route(Text *t, const Route *r) {
    putf(t, "    if __n == %zu", r->nseg);
    for (size_t i = 0; i < r->nseg; i++) {
        if (r->kind[i] == 0) {
            putf(t, " and __p[%zu] == ", i);
            put_literal(t, r->seg[i]);
        } else if (r->kind[i] != 3) {
            putf(t, " and __web.__number(__p[%zu], %d)", i, r->kind[i] - 1);
        }
    }
    put(t, " {\n        __allow = __web.__allow(__allow, \"");
    int any = 0;
    for (int m = 0; m < METHOD_COUNT; m++) {
        if (!(r->methods & (1u << m))) continue;
        putf(t, "%s%s%s", any ? ", " : "", method_names[m], m == 0 ? ", HEAD" : "");
        any = 1;
    }
    put(t, "\")\n        if ");
    int first = 1;
    for (int m = 0; m < METHOD_COUNT; m++) {
        if (!(r->methods & (1u << m))) continue;
        if (!first) put(t, " or ");
        putf(t, "__m == \"%s\"", method_names[m]);
        if (m == 0) put(t, " or __m == \"HEAD\"");
        first = 0;
    }
    put(t, " {\n            return __web.");
    put(t, strcmp(r->fn->type_name, "string") == 0 ? "__page(" : "__reply(");
    put(t, r->fn->text);
    put(t, "(");
    for (size_t i = 0; i < param_count(r->fn); i++) {
        const AstNode *p = r->fn->children[i];
        if (i) put(t, ", ");
        if (strcmp(p->type_name, "Request") == 0) { put(t, "__r"); continue; }
        for (size_t k = 0; k < r->nseg; k++) {
            if (!r->kind[k] || strcmp(r->seg[k], p->text) != 0) continue;
            if (r->kind[k] == 1) putf(t, "int(__p[%zu])", k);
            else if (r->kind[k] == 2) putf(t, "float(__p[%zu])", k);
            else putf(t, "__p[%zu]", k);
        }
    }
    put(t, "))\n        }\n    }\n");
}

/* nodes made by the compiler have no line */
static void forget_lines(AstNode *n) {
    if (!n) return;
    n->line = 0;
    n->file = NULL;
    for (size_t i = 0; i < n->child_count; i++) forget_lines(n->children[i]);
}

int web_prepare(AstNode *program) {
    failed = 0;
    const char *alias = NULL, *route_name = NULL;
    int uses = 0;
    for (size_t i = 0; i < program->child_count; i++) {
        const AstNode *c = program->children[i];
        if (c->kind != AST_IMPORT || !c->text || strcmp(c->text, "web") != 0) continue;
        uses = 1;
        if (c->type_name && strcmp(c->type_name, "from") == 0) {
            for (size_t k = 0; k < c->child_count; k++) {
                const AstNode *x = c->children[k];
                if (x->text && strcmp(x->text, "route") == 0) route_name = x->child_count ? x->children[0]->text : x->text;
            }
        } else {
            alias = c->child_count ? c->children[0]->text : c->text;
        }
    }
    Route *routes = calloc(WEB_ROUTES_MAX, sizeof(Route));
    if (!routes) return 0;
    /* from web import route: route is no function of the module, only the name of the decorator */
    char *route_kept = route_name ? dup_n(route_name, strlen(route_name)) : NULL;
    route_name = route_kept;
    for (size_t i = 0; i < program->child_count; i++) {
        AstNode *c = program->children[i];
        if (c->kind != AST_IMPORT || !c->text || strcmp(c->text, "web") != 0 || !c->type_name ||
            strcmp(c->type_name, "from") != 0) continue;
        size_t out = 0;
        for (size_t k = 0; k < c->child_count; k++) {
            AstNode *x = c->children[k];
            if (x->text && strcmp(x->text, "route") == 0) { ast_free(x); continue; }
            c->children[out++] = x;
        }
        c->child_count = out;
        if (out == 0) {                                  /* from web import route: nothing else to import */
            ast_free(c);
            for (size_t k = i + 1; k < program->child_count; k++) program->children[k - 1] = program->children[k];
            program->child_count--;
            i--;
        }
    }
    size_t count = 0;
    for (size_t i = 0; i < program->child_count && !failed; i++) {
        AstNode *fn = program->children[i];
        if (fn->kind != AST_FUNCTION || !fn->decorators) continue;
        for (size_t k = 0; k < fn->decorators->child_count && !failed; k++) {
            const AstNode *d = fn->decorators->children[k];
            size_t line = d->line ? d->line : fn->line;
            int is_method = d->kind == AST_METHOD_CALL && d->child_count > 0 && d->children[0]->kind == AST_IDENTIFIER &&
                            d->children[0]->text && alias && strcmp(d->children[0]->text, alias) == 0 &&
                            d->text && strcmp(d->text, "route") == 0;
            int is_call = d->kind == AST_CALL && d->text && route_name && strcmp(d->text, route_name) == 0;
            if (!uses) {
                web_error(line, "@%s(...) on %s(): the only decorator is @web.route, after import web",
                          d->text ? d->text : "", fn->text);
                break;
            }
            if (!is_method && !is_call) {
                web_error(line, "@%s%s%s(...) on %s(): the only decorator is @%s.route(...)",
                          d->kind == AST_METHOD_CALL && d->children[0]->text ? d->children[0]->text : "",
                          d->kind == AST_METHOD_CALL ? "." : "", d->text ? d->text : "", fn->text, alias ? alias : "web");
                break;
            }
            if (count >= WEB_ROUTES_MAX) {
                web_error(line, "more than %d routes", WEB_ROUTES_MAX);
                break;
            }
            if (!read_route(fn, d, is_method, &routes[count])) break;
            routes[count].order = count;
            count++;
        }
    }
    /* a method and a path taken twice */
    for (size_t i = 0; i < count && !failed; i++) {
        for (size_t j = 0; j < i && !failed; j++) {
            if (!same_shape(&routes[i], &routes[j]) || !(routes[i].methods & routes[j].methods)) continue;
            char shape[256];
            shape_text(&routes[i], shape, sizeof(shape));
            int m = 0;
            while (!((routes[i].methods & routes[j].methods) & (1u << m))) m++;
            web_error(routes[i].line, "%s %s is already answered by %s() of line %zu", method_names[m], shape,
                      routes[j].fn->text, routes[j].line);
        }
    }
    int ok = !failed;
    if (ok && uses) {
        qsort(routes, count, sizeof(Route), route_cmp);
        Text t = { 0 };
        put(&t, "def __web_handle() -> int {\n    let __r: Request = __web.__request()\n");
        if (count) {
            put(&t, "    let __p: list[string] = __web.__parts(__r.path)\n    let __n: int = len(__p)\n"
                    "    let __m: string = __r.method\n    let __allow: string = \"\"\n");
            for (size_t i = 0; i < count; i++) write_route(&t, &routes[i]);
            /* the path is known, with other methods: 405, and they are told */
            put(&t, "    if __allow != \"\" {\n        return __web.__reply(__web.with_header(__web.error(405, \"Method Not Allowed\"), "
                    "\"Allow\", __allow))\n    }\n");
        }
        put(&t, "    return __web.__static(__r.path)\n}\n");
        if (failed || !t.p) ok = 0;
        if (ok) {
            size_t saved_line = ast_current_line;
            const char *saved_file = ast_current_file;
            ast_current_file = NULL;
            Parser parser;
            parser_init(&parser, t.p);
            AstNode *gen = parser_parse_program(&parser);
            ast_current_line = saved_line;
            ast_current_file = saved_file;
            if (!gen || parser.had_error || gen->child_count != 1) {
                fprintf(stderr, "web error: the routes could not be written (compiler bug), here they are:\n%s", t.p);
                ok = 0;
            } else {
                AstNode *fn = gen->children[0];
                gen->child_count = 0;
                forget_lines(fn);
                AstNode *imp = ast_new(AST_IMPORT, "web", 3);
                AstNode *as = ast_new(AST_IDENTIFIER, "__web", 5);
                if (!imp || !as || !ast_add_child(imp, as) || !ast_add_child(program, imp) || !ast_add_child(program, fn)) {
                    ok = 0;
                }
                if (imp) imp->line = 0;
            }
            ast_free(gen);
        }
        free(t.p);
    }
    for (size_t i = 0; i < count; i++)
        for (size_t k = 0; k < routes[i].nseg; k++) free(routes[i].seg[k]);
    free(routes);
    free(route_kept);
    /* the decorators are done: nothing after this pass sees them */
    for (size_t i = 0; i < program->child_count; i++) {
        AstNode *fn = program->children[i];
        if (fn->kind == AST_FUNCTION && fn->decorators) {
            ast_free(fn->decorators);
            fn->decorators = NULL;
        }
    }
    return ok;
}
