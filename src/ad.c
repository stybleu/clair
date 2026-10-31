#include "ad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static AstNode *nombre(const char *s) {
    return ast_new(AST_FLOAT, s, strlen(s));
}

static AstNode *binaire(
    const char *op, AstNode *a, AstNode *b
) {
    if (!a || !b) {
        ast_free(a);
        ast_free(b);
        return NULL;
    }
    AstNode *n = ast_new(AST_BINARY, op, strlen(op));
    if (!n) {
        ast_free(a);
        ast_free(b);
        return NULL;
    }
    if (!ast_add_child(n, a)) {
        ast_free(a);
        ast_free(b);
        ast_free(n);
        return NULL;
    }
    if (!ast_add_child(n, b)) {
        ast_free(b);
        ast_free(n);
        return NULL;
    }
    return n;
}


static AstNode *appel(const char *nom, AstNode *arg) {
    if (!arg) return NULL;

    AstNode *n = ast_new(AST_CALL, nom, strlen(nom));
    if (!n) {
        ast_free(arg);
        return NULL;
    }

    if (!ast_add_child(n, arg)) {
        ast_free(arg);
        ast_free(n);
        return NULL;
    }
    return n;
}

static AstNode *derivee(
    const AstNode *e, const char *x, int profondeur
) {
    if (!e || profondeur > 32 || e->unit)
        return NULL;

    if (e->kind == AST_FLOAT || e->kind == AST_INTEGER)
        return nombre("0.0");

    if (e->kind == AST_IDENTIFIER) {
        if (!e->text) return NULL;
        return nombre(strcmp(e->text, x) == 0 ? "1.0" : "0.0");
    }

    if (e->kind == AST_UNARY &&
        e->text && e->child_count == 1 &&
        !strcmp(e->text, "-")) {
        return binaire(
            "-", nombre("0.0"),
            derivee(e->children[0], x, profondeur + 1)
        );
    }

    if (e->kind == AST_CALL &&
        e->text && e->child_count == 1) {

        const AstNode *a = e->children[0];
        AstNode *d = NULL;

        if (!strcmp(e->text, "sin")) {
            d = binaire("*",
                appel("cos", ast_clone(a)),
                derivee(a, x, profondeur + 1));
        }
        else if (!strcmp(e->text, "cos")) {
            d = binaire("*",
                binaire("-",
                    nombre("0.0"),
                    appel("sin", ast_clone(a))),
                derivee(a, x, profondeur + 1));
        }
        else if (!strcmp(e->text, "exp")) {
            d = binaire("*",
                appel("exp", ast_clone(a)),
                derivee(a, x, profondeur + 1));
        }
        else if (!strcmp(e->text, "log")) {
            d = binaire("/",
                derivee(a, x, profondeur + 1),
                ast_clone(a));
        }
        else if (!strcmp(e->text, "sqrt")) {
            d = binaire("/",
                derivee(a, x, profondeur + 1),
                binaire("*",
                    nombre("2.0"),
                    appel("sqrt", ast_clone(a))));
        }

        return d;
    }

    if (e->kind != AST_BINARY ||
        !e->text || e->child_count != 2)
        return NULL;

    const AstNode *a = e->children[0];
    const AstNode *b = e->children[1];

    if (!strcmp(e->text, "+") ||
        !strcmp(e->text, "-")) {
        return binaire(
            e->text,
            derivee(a, x, profondeur + 1),
            derivee(b, x, profondeur + 1)
        );
    }

    if (!strcmp(e->text, "*")) {
        return binaire("+",
            binaire("*",
                derivee(a, x, profondeur + 1),
                ast_clone(b)),
            binaire("*",
                ast_clone(a),
                derivee(b, x, profondeur + 1))
        );
    }

    if (!strcmp(e->text, "/")) {
        return binaire("/",
            binaire("-",
                binaire("*",
                    derivee(a, x, profondeur + 1),
                    ast_clone(b)),
                binaire("*",
                    ast_clone(a),
                    derivee(b, x, profondeur + 1))),
            binaire("*", ast_clone(b), ast_clone(b))
        );
    }

    return NULL;
}

int clair_ad_generate(AstNode *program) {
    if (!program || program->kind != AST_PROGRAM)
        return 0;


    /* Eviter les collisions entre fonctions utilisateur
       et fonctions mathematiques generees. */
    const char *maths[] = {
        "sin", "cos", "exp", "log", "sqrt", NULL
    };
    for (size_t i = 0; i < program->child_count; ++i) {
        const AstNode *f = program->children[i];
        if (f->kind != AST_FUNCTION || !f->text)
            continue;
        for (size_t j = 0; maths[j]; ++j) {
            if (!strcmp(f->text, maths[j])) {
                fprintf(stderr,
                    "AD V2: fonction %s masquant une fonction "
                    "mathematique non prise en charge\n", f->text);
                return 0;
            }
        }
    }

    size_t initial = program->child_count;

    for (size_t i = 0; i < initial; ++i) {
        AstNode *f = program->children[i];

        if (f->kind != AST_FUNCTION ||
            !f->text || !f->type_name ||
            strcmp(f->type_name, "float") ||
            (f->child_count < 2 || f->child_count > 17) ||
            strchr(f->text, '.') ||
            strstr(f->text, "__") ||
            !strncmp(f->text, "derivee_", 8))
            continue;

        size_t np = f->child_count - 1;
        AstNode *body = f->children[np];
        int valide = !f->unit;

        for (size_t k = 0; k < np; ++k) {
            AstNode *p = f->children[k];
            if (!p || p->kind != AST_PARAM ||
                !p->text || !p->type_name ||
                strcmp(p->type_name, "float") ||
                p->unit)
                valide = 0;
        }

        if (!valide || !body ||
            body->kind != AST_BLOCK ||
            body->text ||
            body->child_count != 1)
            continue;

        AstNode *ret = body->children[0];
        if (!ret || ret->kind != AST_RETURN ||
            ret->child_count != 1)
            continue;

        for (size_t k = 0; k < np; ++k) {
        AstNode *param = f->children[k];
        AstNode *d = derivee(
            ret->children[0], param->text, 0
        );
        if (!d) continue;

        size_t taille = strlen(f->text) + 9 +
            (np > 1 ? strlen(param->text) + 1 : 0);
        char *nom = malloc(taille);
        if (!nom) {
            ast_free(d);
            return 0;
        }
        if (np == 1)
            snprintf(nom, taille, "derivee_%s", f->text);
        else
            snprintf(nom, taille, "derivee_%s_%s",
                     f->text, param->text);

        for (size_t j = 0; j < program->child_count; ++j) {
            if (program->children[j]->text &&
                !strcmp(program->children[j]->text, nom)) {
                fprintf(stderr,
                    "AD error: nom deja utilise: %s\n", nom);
                free(nom);
                ast_free(d);
                return 0;
            }
        }

        AstNode *nouvelle = ast_clone(f);
        if (!nouvelle) {
            free(nom);
            ast_free(d);
            return 0;
        }

        free(nouvelle->text);
        nouvelle->text = nom;

        AstNode *r = nouvelle->children[np]->children[0];
        ast_free(r->children[0]);
        r->children[0] = d;

        if (!ast_add_child(program, nouvelle)) {
            ast_free(nouvelle);
            return 0;
        }
        }
    }
    return 1;
}
