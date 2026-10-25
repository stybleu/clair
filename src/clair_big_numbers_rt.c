#include "clair_big_numbers_rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Objets immuables : une racine est l'adresse d'une variable Clair vivante.
 * Balayage uniquement aux fins de blocs du code genere, jamais pendant une
 * expression C. Les appels de fonctions imbriques suspendent la collecte. */
struct ClairDynInt { ClairBigInt v; };
struct ClairDynFloat { ClairBigFloat v; uint32_t precision; };
typedef struct ClairNumNode { struct ClairNumNode *next; void *value; int is_float; int marked; } ClairNumNode;
static ClairNumNode *clair_num_nodes;
static ClairNumRoot *clair_num_roots;
static unsigned clair_num_frame_depth;

void clair_rt_num_root_enter(ClairNumRoot *root, const void *slot) {
    root->previous = clair_num_roots;
    root->slot = slot;
    clair_num_roots = root;
}
void clair_rt_num_root_cleanup(ClairNumRoot *root) {
    if (clair_num_roots != root) {
        fprintf(stderr, "Clair runtime error: ordre des racines invalide\n");
        abort();
    }
    clair_num_roots = root->previous;
    root->previous = NULL;
    root->slot = NULL;
}
void clair_rt_num_frame_enter(ClairNumFrameGuard *guard) {
    guard->active = 1;
    ++clair_num_frame_depth;
}
void clair_rt_num_frame_cleanup(ClairNumFrameGuard *guard) {
    if (guard->active && clair_num_frame_depth) --clair_num_frame_depth;
    guard->active = 0;
}
static void clair_num_destroy_node(ClairNumNode *n) {
    if (n->is_float) {
        ClairDynFloat *x = (ClairDynFloat *)n->value;
        clair_bf_clear(&x->v);
    } else {
        ClairDynInt *x = (ClairDynInt *)n->value;
        clair_big_clear(&x->v);
    }
    free(n->value);
    free(n);
}

/* Appeler seulement a une frontiere entre instructions Clair et dans main :
 * la pile d'expressions est vide, aucun resultat temporaire n'est suspendu
 * pendant l'execution d'un appel imbrique. */
void clair_rt_num_collect(void) {
    if (clair_num_frame_depth != 1) return;
    for (ClairNumNode *n = clair_num_nodes; n; n = n->next) n->marked = 0;
    for (ClairNumRoot *root = clair_num_roots; root; root = root->previous) {
        const void *value = NULL;
        /* memcpy evite le strict-aliasing entre pointeur typé et void*. */
        memcpy(&value, root->slot, sizeof(value));
        for (ClairNumNode *n = clair_num_nodes; n; n = n->next)
            if (n->value == value) { n->marked = 1; break; }
    }
    ClairNumNode **p = &clair_num_nodes;
    while (*p) {
        ClairNumNode *n = *p;
        if (n->marked) p = &n->next;
        else { *p = n->next; clair_num_destroy_node(n); }
    }
}
static int clair_num_registered;

static void clair_num_destroy_all(void) {
    while (clair_num_nodes) {
        ClairNumNode *n = clair_num_nodes;
        clair_num_nodes = n->next;
        clair_num_destroy_node(n);
    }
}
static void clair_num_fail(const char *msg) {
    fprintf(stderr,"Clair V0.40 big numbers: %s\n",msg);
    exit(EXIT_FAILURE);
}
static void clair_num_check(int rc) {
    if (rc == CLAIR_BIG_OK) return;
    switch (rc) {
        case CLAIR_BIG_NOMEM: clair_num_fail("memoire insuffisante");break;
        case CLAIR_BIG_INVALID: clair_num_fail("valeur ou precision invalide");break;
        case CLAIR_BIG_DIVZERO: clair_num_fail("division par zero");break;
        case CLAIR_BIG_TOO_LARGE: clair_num_fail("nombre ou exposant trop grand");break;
        default: clair_num_fail("erreur arithmetique");
    }
}
static void *clair_num_alloc(size_t size, int flt) {
    void *value=calloc(1,size);
    ClairNumNode *n=(ClairNumNode*)malloc(sizeof(*n));
    if (!value || !n) {free(value);free(n);clair_num_fail("memoire insuffisante");}
    if (!clair_num_registered) {
        if (atexit(clair_num_destroy_all)) {free(value);free(n);clair_num_fail("atexit a echoue");}
        clair_num_registered=1;
    }
    n->value=value;n->is_float=flt;n->next=clair_num_nodes;clair_num_nodes=n;
    return value;
}
static ClairDynInt *clair_new_int(void) {
    ClairDynInt *v=(ClairDynInt*)clair_num_alloc(sizeof(*v),0);
    clair_big_init(&v->v);
    return v;
}
static ClairDynFloat *clair_new_float(uint32_t precision) {
    ClairDynFloat *v=(ClairDynFloat*)clair_num_alloc(sizeof(*v),1);
    clair_bf_init(&v->v);v->precision=precision;
    return v;
}
ClairDynInt *clair_rt_bigint_parse(const char *s) {
    ClairDynInt *r=clair_new_int();clair_num_check(clair_big_from_decimal(&r->v,s));return r;
}
ClairDynInt *clair_rt_bigint_neg(const ClairDynInt *v) {
    ClairDynInt *r=clair_new_int();clair_num_check(clair_big_copy(&r->v,&v->v));r->v.sign=-r->v.sign;return r;
}
static ClairDynInt *clair_int_bin(const ClairDynInt *a,const ClairDynInt *b,char op) {
    ClairDynInt *r=clair_new_int();int rc=CLAIR_BIG_INVALID;
    switch(op) {
        case '+':rc=clair_big_add(&r->v,&a->v,&b->v);break;
        case '-':rc=clair_big_sub(&r->v,&a->v,&b->v);break;
        case '*':rc=clair_big_mul(&r->v,&a->v,&b->v);break;
        case '/':{ClairBigInt rem;clair_big_init(&rem);rc=clair_big_divmod(&r->v,&rem,&a->v,&b->v);clair_big_clear(&rem);break;}
        case '%':{ClairBigInt quo;clair_big_init(&quo);rc=clair_big_divmod(&quo,&r->v,&a->v,&b->v);clair_big_clear(&quo);break;}
    }
    clair_num_check(rc);return r;
}
#define INT_BIN(NAME, OP) ClairDynInt *clair_rt_bigint_##NAME(const ClairDynInt *a,const ClairDynInt *b){return clair_int_bin(a,b,OP);}
INT_BIN(add,'+') INT_BIN(sub,'-') INT_BIN(mul,'*') INT_BIN(div,'/') INT_BIN(mod,'%')
int clair_rt_bigint_cmp(const ClairDynInt *a,const ClairDynInt *b) {return clair_big_cmp(&a->v,&b->v);}
void clair_rt_bigint_print(const ClairDynInt *v) {
    char *str=NULL;clair_num_check(clair_big_to_decimal(&v->v,&str));puts(str);free(str);
}
ClairDynFloat *clair_rt_bigfloat_parse(const char *s,int64_t precision) {
    if(precision<1 || precision>1000000)clair_num_fail("precision bigfloat hors de [1, 1000000]");
    ClairDynFloat *r=clair_new_float((uint32_t)precision);
    clair_num_check(clair_bf_parse(&r->v,s,r->precision));return r;
}
ClairDynFloat *clair_rt_bigfloat_neg(const ClairDynFloat *v) {
    ClairDynFloat *r=clair_new_float(v->precision);
    clair_num_check(clair_big_copy(&r->v.coefficient,&v->v.coefficient));
    r->v.exp10=v->v.exp10;r->v.coefficient.sign=-r->v.coefficient.sign;return r;
}
static ClairDynFloat *clair_float_bin(const ClairDynFloat *a,const ClairDynFloat *b,char op) {
    uint32_t p=a->precision>b->precision?a->precision:b->precision;
    ClairDynFloat *r=clair_new_float(p);int rc=CLAIR_BIG_INVALID;
    switch(op) {
        case '+':rc=clair_bf_add(&r->v,&a->v,&b->v,p);break;
        case '-':rc=clair_bf_sub(&r->v,&a->v,&b->v,p);break;
        case '*':rc=clair_bf_mul(&r->v,&a->v,&b->v,p);break;
        case '/':rc=clair_bf_div(&r->v,&a->v,&b->v,p);break;
    }
    clair_num_check(rc);return r;
}
#define FLT_BIN(NAME, OP) ClairDynFloat *clair_rt_bigfloat_##NAME(const ClairDynFloat *a,const ClairDynFloat *b){return clair_float_bin(a,b,OP);}
FLT_BIN(add,'+') FLT_BIN(sub,'-') FLT_BIN(mul,'*') FLT_BIN(div,'/')
/* Comparaison exacte coefficient*10^exp sans creer 10^1e9 en memoire. */
int clair_rt_bigfloat_cmp(const ClairDynFloat *a,const ClairDynFloat *b) {
    int sa=a->v.coefficient.sign,sb=b->v.coefficient.sign;
    if(sa!=sb)return sa<sb?-1:1;
    if(!sa)return 0;
    char *da=NULL,*db=NULL;
    clair_num_check(clair_big_to_decimal(&a->v.coefficient,&da));
    clair_num_check(clair_big_to_decimal(&b->v.coefficient,&db));
    const char *as=da+(da[0]=='-'),*bs=db+(db[0]=='-');
    size_t la=strlen(as),lb=strlen(bs);
    __int128 ea=(__int128)a->v.exp10+(__int128)la-1;
    __int128 eb=(__int128)b->v.exp10+(__int128)lb-1;
    int cmp=0;
    if(ea<eb)cmp=-1;
    else if(ea>eb)cmp=1;
    else {
        size_t len=la>lb?la:lb;
        for(size_t i=0;i<len;i++) {
            char ca=i<la?as[i]:'0',cb=i<lb?bs[i]:'0';
            if(ca!=cb){cmp=ca<cb?-1:1;break;}
        }
    }
    free(da);free(db);return sa*cmp;
}
void clair_rt_bigfloat_print(const ClairDynFloat *v) {
    char *str=NULL;clair_num_check(clair_bf_format(&v->v,&str));puts(str);free(str);
}
