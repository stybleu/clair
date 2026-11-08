#include "ad_reverse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define AD_MAX_NODES 128
#define AD_MAX_PARAMS 16

typedef struct {
    const AstNode *expr;
    int left, right;
} TapeNode;

typedef struct {
    TapeNode nodes[AD_MAX_NODES];
    int count;
    const char *params[AD_MAX_PARAMS];
    int nparams;
} Tape;

static int supported_call(const char *s) {
    return s && (!strcmp(s,"sin") || !strcmp(s,"cos") ||
        !strcmp(s,"exp") || !strcmp(s,"log") || !strcmp(s,"sqrt"));
}


/* V4.10 : partage structurel des expressions pures. */
static int ad_same(
    const AstNode *a, const AstNode *b, int depth
) {
    if (a == b) return 1;
    if (!a || !b || depth > 32 ||
        a->kind != b->kind ||
        a->child_count != b->child_count ||
        a->unit || b->unit) return 0;

    if ((a->text == NULL) != (b->text == NULL))
        return 0;

    if (a->text && strcmp(a->text,b->text))
        return 0;

    for (size_t i=0;i<a->child_count;i++)
        if (!ad_same(a->children[i],
                     b->children[i],depth+1))
            return 0;

    return 1;
}

/* On refuse les expressions à effets de bord et les unités en V4. */
static int flatten(Tape *t, const AstNode *e, int depth) {

    if (!e || e->unit || depth > 32) return -1;

    for (int i=0;i<t->count;i++)
        if (ad_same(e,t->nodes[i].expr,0))
            return i;

    if (t->count >= AD_MAX_NODES) return -1;
    int a = -1, b = -1;
    switch (e->kind) {
        case AST_FLOAT: case AST_INTEGER: break;
        case AST_IDENTIFIER: {
            int found = 0;
            for (int k=0;k<t->nparams;k++)
                if (e->text && !strcmp(e->text,t->params[k]))
                    found = 1;
            if (!found) return -1;
            break;
        }
        case AST_UNARY:
            if (!e->text || strcmp(e->text,"-") || e->child_count != 1) return -1;
            a = flatten(t,e->children[0],depth+1);
            if (a < 0) return -1;
            break;
        case AST_CALL:
            if (!supported_call(e->text) || e->child_count != 1) return -1;
            a = flatten(t,e->children[0],depth+1);
            if (a < 0) return -1;
            break;
        case AST_BINARY:
            if (!e->text || e->child_count != 2 ||
                (strcmp(e->text,"+") && strcmp(e->text,"-") &&
                 strcmp(e->text,"*") && strcmp(e->text,"/"))) return -1;
            a = flatten(t,e->children[0],depth+1);
            if (a < 0) return -1;
            b = flatten(t,e->children[1],depth+1);
            if (b < 0) return -1;
            break;
        default: return -1;
    }
    if (t->count >= AD_MAX_NODES) return -1;
    int id = t->count++;
    t->nodes[id] = (TapeNode){e,a,b};
    return id;
}

static AstNode *node(AstKind kind, const char *name) {
    return ast_new(kind, name, name ? strlen(name) : 0);
}
static AstNode *join(AstKind kind, const char *name, AstNode *a, AstNode *b) {
    if (!a || (b == NULL && kind == AST_BINARY)) {
        ast_free(a); ast_free(b); return NULL;
    }
    AstNode *n = node(kind,name);
    if (!n) { ast_free(a); ast_free(b); return NULL; }
    if (!ast_add_child(n,a)) { ast_free(a); ast_free(b); ast_free(n); return NULL; }
    if (b && !ast_add_child(n,b)) { ast_free(b); ast_free(n); return NULL; }
    return n;
}
static AstNode *bin(const char *op, AstNode *a, AstNode *b) {
    return join(AST_BINARY,op,a,b);
}
static AstNode *unary(const char *op, AstNode *a) {
    return join(AST_UNARY,op,a,NULL);
}
static AstNode *call(const char *name, AstNode *a) {
    return join(AST_CALL,name,a,NULL);
}
static AstNode *num(const char *s) {return node(AST_FLOAT,s);}
static AstNode *named(const char *prefix,int i,AstKind kind) {
    char name[48];
    snprintf(name,sizeof(name),"@ad%s%d",prefix,i);
    return node(kind,name);
}
static AstNode *refp(int i) {return named("p",i,AST_IDENTIFIER);}
static AstNode *refa(int i) {return named("a",i,AST_IDENTIFIER);}
static AstNode *refg(int i) {return named("g",i,AST_IDENTIFIER);}

static AstNode *statement(AstKind kind,const char *prefix,int i,AstNode *expr) {
    char name[48];
    snprintf(name,sizeof(name),"@ad%s%d",prefix,i);
    AstNode *n = join(kind,name,expr,NULL);
    if (n && kind == AST_LET && !ast_set_type(n,"float",5)) {
        ast_free(n);return NULL;
    }
    return n;
}
static int append(AstNode *body,AstNode *s) {
    if (!s) return 0;
    if (!ast_add_child(body,s)) {ast_free(s);return 0;}
    return 1;
}
static int let(AstNode *body,const char *p,int i,AstNode *e) {
    return append(body,statement(AST_LET,p,i,e));
}
static int set(AstNode *body,const char *p,int i,AstNode *e) {
    return append(body,statement(AST_ASSIGN,p,i,e));
}
static int inc(AstNode *body,const char *p,int i,AstNode *delta) {
    return set(body,p,i,bin("+",named(p,i,AST_IDENTIFIER),delta));
}

static AstNode *primal(const Tape *t,int i) {
    const TapeNode *n = &t->nodes[i];
    const AstNode *e = n->expr;
    switch(e->kind) {
        case AST_FLOAT: case AST_INTEGER: case AST_IDENTIFIER:
            return ast_clone(e);
        case AST_UNARY: return unary(e->text,refp(n->left));
        case AST_CALL: return call(e->text,refp(n->left));
        case AST_BINARY: return bin(e->text,refp(n->left),refp(n->right));
        default: return NULL;
    }
}

static int reverse_step(AstNode *body,const Tape *t,int i) {
    const TapeNode *n=&t->nodes[i];
    const AstNode *e=n->expr;
    int a=n->left,b=n->right;
    if (e->kind == AST_FLOAT || e->kind == AST_INTEGER) return 1;
    if (e->kind == AST_IDENTIFIER) {
        for (int k=0;k<t->nparams;k++)
            if (!strcmp(e->text,t->params[k]))
                return inc(body,"g",k,refa(i));
        return 0;
    }
    if (e->kind == AST_UNARY)
        return inc(body,"a",a,unary("-",refa(i)));
    if (e->kind == AST_CALL) {
        AstNode *delta=NULL;
        if (!strcmp(e->text,"sin")) delta=bin("*",refa(i),call("cos",refp(a)));
        else if (!strcmp(e->text,"cos")) delta=unary("-",bin("*",refa(i),call("sin",refp(a))));
        else if (!strcmp(e->text,"exp")) delta=bin("*",refa(i),refp(i));
        else if (!strcmp(e->text,"log")) delta=bin("/",refa(i),refp(a));
        else if (!strcmp(e->text,"sqrt")) delta=bin("/",refa(i),bin("*",num("2.0"),refp(i)));
        return inc(body,"a",a,delta);
    }
    if (!strcmp(e->text,"+"))
        return inc(body,"a",a,refa(i)) && inc(body,"a",b,refa(i));
    if (!strcmp(e->text,"-"))
        return inc(body,"a",a,refa(i)) && inc(body,"a",b,unary("-",refa(i)));
    if (!strcmp(e->text,"*"))
        return inc(body,"a",a,bin("*",refa(i),refp(b))) &&
               inc(body,"a",b,bin("*",refa(i),refp(a)));
    if (!strcmp(e->text,"/"))
        return inc(body,"a",a,bin("/",refa(i),refp(b))) &&
               inc(body,"a",b,unary("-",bin("/",
                   bin("*",refa(i),refp(a)),
                   bin("*",refp(b),refp(b)))));
    return 0;
}

static AstNode *gradient(const AstNode *src,const Tape *t) {
    char fname[256];
    if (strlen(src->text)+10 >= sizeof(fname)) return NULL;
    snprintf(fname,sizeof(fname),"gradient_%s",src->text);
    AstNode *f=node(AST_FUNCTION,fname),*body=node(AST_BLOCK,NULL);
    if (!f || !body) {ast_free(f);ast_free(body);return NULL;}
    if (!ast_set_type(f,"list[float]",11)) goto fail;
    for (int k=0;k<t->nparams;k++) {
        AstNode *p=ast_clone(src->children[k]);
        if (!p) goto fail;
        if (!ast_add_child(f,p)) {ast_free(p);goto fail;}
    }
    if (!ast_add_child(f,body)) goto fail;
    /* Désormais, le bloc appartient à la fonction. */
    for (int i=0;i<t->count;i++)
        if (!let(body,"p",i,primal(t,i))) goto fail;
    for (int i=0;i<t->count;i++)
        if (!let(body,"a",i,num("0.0"))) goto fail;
    for (int i=0;i<t->nparams;i++)
        if (!let(body,"g",i,num("0.0"))) goto fail;
    if (!set(body,"a",t->count-1,num("1.0"))) goto fail;
    for (int i=t->count-1;i>=0;i--)
        if (!reverse_step(body,t,i)) goto fail;
    AstNode *list=node(AST_LIST,NULL);
    if (!list) goto fail;
    for (int i=0;i<t->nparams;i++) {
        AstNode *v=refg(i);
        if (!v) {ast_free(list);goto fail;}
        if (!ast_add_child(list,v)) {
            ast_free(v);ast_free(list);goto fail;
        }
    }
    if (!append(body,join(AST_RETURN,NULL,list,NULL))) goto fail;
    return f;
fail:
    ast_free(f);
    return NULL;
}


/* V4.6 : ecrire le gradient dans une liste existante. */
static AstNode *gradient_into(
    const AstNode *src, const AstNode *base, const Tape *t
) {
    static const char outname[] = "@adout";
    char fname[256];

    if (strlen(src->text) + strlen("gradient_vers_")
        >= sizeof(fname)) return NULL;

    snprintf(fname, sizeof(fname),
             "gradient_vers_%s", src->text);

    AstNode *f = ast_clone(base);
    if (!f) return NULL;

    AstNode *p = NULL;
    char *name = malloc(strlen(fname) + 1);
    if (!name) goto fail;

    strcpy(name, fname);
    free(f->text);
    f->text = name;

    if (!ast_set_type(f, "int", 3)) goto fail;

    size_t np = (size_t)t->nparams;
    if (f->child_count != np + 1) goto fail;

    p = node(AST_PARAM, outname);
    if (!p) goto fail;

    if (!ast_set_type(p, "list[float]", 11))
        goto fail;

    if (!ast_add_child(f, p)) goto fail;
    p = NULL;

    AstNode *body = f->children[np];
    f->children[np] = f->children[np + 1];
    f->children[np + 1] = body;

    if (!body || body->kind != AST_BLOCK ||
        body->child_count == 0) goto fail;

    AstNode *old =
        body->children[body->child_count - 1];

    if (!old || old->kind != AST_RETURN)
        goto fail;

    body->child_count--;
    ast_free(old);

    for (int k = 0; k < t->nparams; ++k) {
        char index[24];
        snprintf(index, sizeof(index), "%d", k);

        AstNode *write = join(
            AST_INDEX_ASSIGN, outname,
            node(AST_INTEGER, index),
            refg(k)
        );

        if (!append(body, write)) goto fail;
    }

    if (!append(
        body,
        join(AST_RETURN, NULL,
             node(AST_INTEGER, "0"), NULL)
    )) goto fail;

    return f;

fail:
    ast_free(p);
    ast_free(f);
    return NULL;
}

int clair_ad_reverse_generate(AstNode *program) {
    if (!program || program->kind!=AST_PROGRAM) return 0;
    size_t count=program->child_count;
    for (size_t i=0;i<count;i++) {
        AstNode *f=program->children[i];
        if (!f || f->kind!=AST_FUNCTION || !f->text || !f->type_name ||
            strcmp(f->type_name,"float") || f->child_count<3 ||
            f->child_count>AD_MAX_PARAMS+1 || f->unit ||
            strstr(f->text,"__") || !strncmp(f->text,"derivee_",8) ||
            !strncmp(f->text,"gradient_",9)) continue;
        int np=(int)f->child_count-1;
        const AstNode *body=f->children[np];
        if (!body || body->kind!=AST_BLOCK || body->text || body->child_count!=1) continue;
        const AstNode *ret=body->children[0];
        if (!ret || ret->kind!=AST_RETURN || ret->child_count!=1) continue;
        int valid=1;
        for (int k=0;k<np;k++) {
            const AstNode *p=f->children[k];
            if (!p || p->kind!=AST_PARAM || !p->text || !p->type_name ||
                strcmp(p->type_name,"float") || p->unit) valid=0;
        }
        if (!valid) continue;
        Tape tape={0};
        tape.nparams=np;
        for (int k=0;k<np;k++)
            tape.params[k]=f->children[k]->text;
        if (flatten(&tape,ret->children[0],0)<0) continue;
        if (strlen(f->text)>240) continue;
        char name[256];
        snprintf(name,sizeof(name),"gradient_%s",f->text);
        for(size_t j=0;j<program->child_count;j++)
            if(program->children[j]->text && !strcmp(program->children[j]->text,name)) {
                fprintf(stderr,"AD inverse: nom deja utilise: %s\n",name);
                return 0;
            }
        AstNode *g=gradient(f,&tape);
        if (!g) {fprintf(stderr,"AD inverse: generation impossible pour %s\n",f->text);return 0;}

        char into_name[256];
        snprintf(into_name,sizeof(into_name),
                 "gradient_vers_%s",f->text);

        for (size_t j=0;j<program->child_count;j++)
            if (program->children[j]->text &&
                !strcmp(program->children[j]->text,into_name)) {
                fprintf(stderr,"AD inverse: nom deja utilise: %s\n",into_name);
                ast_free(g);
                return 0;
            }

        AstNode *into=gradient_into(f,g,&tape);
        if (!into) {ast_free(g);return 0;}

        if (!ast_add_child(program,g)) {
            ast_free(into);
            ast_free(g);
            return 0;
        }
        if (!ast_add_child(program,into)) {
            ast_free(into);
            return 0;
        }
    }
    return 1;
}
