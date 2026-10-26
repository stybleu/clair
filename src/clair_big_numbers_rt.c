#include "clair_big_numbers_rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
#include <math.h>

/* Objets immuables.
 *
 * Chaque nombre porte un numero croissant. Chaque appel de fonction note, en
 * entrant, le dernier numero distribue : les nombres plus recents forment sa
 * zone. Aux fins de blocs (aucun temporaire d'expression en cours), la collecte
 * garde les nombres de la zone que montrent les racines de l'appel et libere
 * les autres. En sortie d'appel, seule la valeur rendue par return est gardee.
 * Les temporaires des appelants sont plus anciens : jamais touches.
 * Un nombre range dans un champ d'objet est epingle : garde jusqu'a la fin. */
struct ClairDynInt { ClairBigInt v; };
struct ClairDynFloat { ClairBigFloat v; uint32_t precision; };
typedef struct ClairNumNode {
    struct ClairNumNode *next;
    uint64_t serial;
    unsigned char is_float, marked, pinned;
    union { struct ClairDynInt i; struct ClairDynFloat f; } u;
} ClairNumNode;
#define CLAIR_NUM_NODE(v) ((ClairNumNode *)((char *)(v) - offsetof(ClairNumNode, u)))
static ClairNumNode *clair_num_nodes;      /* du plus recent au plus ancien */
static ClairNumNode *clair_num_pinned;
static ClairNumRoot *clair_num_roots;
static ClairNumFrameGuard *clair_num_frame;
static uint64_t clair_num_serial;

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
static void clair_num_destroy_node(ClairNumNode *n) {
    if (n->is_float) clair_bf_clear(&n->u.f.v);
    else clair_big_clear(&n->u.i.v);
    free(n);
}
static void clair_num_mark(const ClairNumFrameGuard *g, const void *value) {
    if (!value) return;
    ClairNumNode *n = CLAIR_NUM_NODE(value);
    if (n->serial > g->serial && !n->pinned) n->marked = 1;
}
/* la zone de g : garde les nombres marques, epingle a part, libere le reste */
static void clair_num_sweep(ClairNumFrameGuard *g, int with_roots) {
    if (with_roots) {
        for (ClairNumRoot *root = clair_num_roots; root && root != g->roots; root = root->previous) {
            const void *value = NULL;
            memcpy(&value, root->slot, sizeof(value));   /* evite le strict-aliasing */
            clair_num_mark(g, value);
        }
    }
    clair_num_mark(g, g->keep);
    ClairNumNode **p = &clair_num_nodes;
    while (*p && (*p)->serial > g->serial) {
        ClairNumNode *n = *p;
        if (n->pinned) { *p = n->next; n->next = clair_num_pinned; clair_num_pinned = n; }
        else if (n->marked) { n->marked = 0; p = &n->next; }
        else { *p = n->next; clair_num_destroy_node(n); }
    }
    g->clean = clair_num_serial;
}
void clair_rt_num_frame_enter(ClairNumFrameGuard *guard) {
    guard->active = 1;
    guard->previous = clair_num_frame;
    guard->serial = guard->clean = clair_num_serial;
    guard->roots = clair_num_roots;
    guard->keep = NULL;
    clair_num_frame = guard;
}
/* sortie de l'appel (return, fin de fonction) : les racines locales sont deja parties */
void clair_rt_num_frame_cleanup(ClairNumFrameGuard *guard) {
    if (!guard->active) return;
    if (clair_num_frame == guard && clair_num_serial != guard->clean) clair_num_sweep(guard, 0);
    clair_num_frame = guard->previous;
    guard->active = 0;
}
/* Appeler seulement entre deux instructions Clair : aucun temporaire suspendu
 * dans l'appel en cours. Rien a faire si aucun nombre n'a ete cree depuis. */
void clair_rt_num_collect(void) {
    ClairNumFrameGuard *g = clair_num_frame;
    if (!g || clair_num_serial == g->clean) return;
    clair_num_sweep(g, 1);
}
void *clair_rt_num_keep(ClairNumFrameGuard *guard, const void *value) {
    guard->keep = value;
    return (void *)value;
}
void *clair_rt_num_pin(const void *value) {
    if (value) CLAIR_NUM_NODE(value)->pinned = 1;
    return (void *)value;
}
static int clair_num_registered;

static void clair_num_destroy_all(void) {
    ClairNumNode *lists[2] = { clair_num_nodes, clair_num_pinned };
    clair_num_nodes = clair_num_pinned = NULL;
    for (int k = 0; k < 2; k++) {
        while (lists[k]) {
            ClairNumNode *n = lists[k];
            lists[k] = n->next;
            clair_num_destroy_node(n);
        }
    }
}
static void clair_num_fail(const char *msg) {
    fprintf(stderr,"Clair runtime error: %s\n",msg);   /* comme les autres erreurs (V0.45) */
    exit(EXIT_FAILURE);
}
static void clair_num_check(int rc) {
    if (rc == CLAIR_BIG_OK) return;
    switch (rc) {
        case CLAIR_BIG_NOMEM: clair_num_fail("out of memory");break;
        case CLAIR_BIG_INVALID: clair_num_fail("invalid big number or precision");break;
        case CLAIR_BIG_DIVZERO: clair_num_fail("division by zero");break;
        case CLAIR_BIG_TOO_LARGE: clair_num_fail("big number or exponent too large");break;
        case CLAIR_BIG_DOMAIN: clair_num_fail("math domain error");break;
        default: clair_num_fail("big number arithmetic error");
    }
}
static ClairNumNode *clair_num_alloc(int flt) {
    ClairNumNode *n=(ClairNumNode*)calloc(1,sizeof(*n));
    if (!n) clair_num_fail("out of memory");
    if (!clair_num_registered) {
        if (atexit(clair_num_destroy_all)) {free(n);clair_num_fail("cannot register memory cleanup");}
        clair_num_registered=1;
    }
    n->is_float=(unsigned char)flt;n->serial=++clair_num_serial;n->next=clair_num_nodes;clair_num_nodes=n;
    return n;
}
static ClairDynInt *clair_new_int(void) {
    ClairDynInt *v=&clair_num_alloc(0)->u.i;
    clair_big_init(&v->v);
    return v;
}
static ClairDynFloat *clair_new_float(uint32_t precision) {
    ClairDynFloat *v=&clair_num_alloc(1)->u.f;
    clair_bf_init(&v->v);v->precision=precision;
    return v;
}
ClairDynInt *clair_rt_bigint_parse(const char *s) {
    ClairDynInt *r=clair_new_int();clair_num_check(clair_big_from_decimal(&r->v,s));return r;
}
ClairDynInt *clair_rt_bigint_from_int(int64_t x) {
    ClairDynInt *r=clair_new_int();clair_num_check(clair_big_from_i64(&r->v,x));return r;
}
char *clair_rt_bigint_str(const ClairDynInt *v) {
    char *str=NULL;clair_num_check(clair_big_to_decimal(&v->v,&str));return str;
}
char *clair_rt_bigfloat_str(const ClairDynFloat *v) {
    char *str=NULL;clair_num_check(clair_bf_format(&v->v,&str));return str;
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
/* Un int vu comme un bigint d'un mot, sur la pile : aucune allocation. */
#define CLAIR_SMALL_INT(name, k) \
    uint64_t name##_word = (k) < 0 ? (uint64_t)(-((k) + 1)) + 1u : (uint64_t)(k); \
    ClairDynInt name = { { &name##_word, (k) != 0, 1, (k) < 0 ? -1 : (k) > 0 } }
ClairDynInt *clair_rt_bigint_add_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_int_bin(a,&b,'+');}
ClairDynInt *clair_rt_bigint_sub_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_int_bin(a,&b,'-');}
ClairDynInt *clair_rt_bigint_int_sub(int64_t k,const ClairDynInt *b){CLAIR_SMALL_INT(a,k);return clair_int_bin(&a,b,'-');}
ClairDynInt *clair_rt_bigint_mul_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_int_bin(a,&b,'*');}
ClairDynInt *clair_rt_bigint_div_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_int_bin(a,&b,'/');}
ClairDynInt *clair_rt_bigint_mod_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_int_bin(a,&b,'%');}
ClairDynInt *clair_rt_bigint_int_div(int64_t k,const ClairDynInt *b){CLAIR_SMALL_INT(a,k);return clair_int_bin(&a,b,'/');}
ClairDynInt *clair_rt_bigint_int_mod(int64_t k,const ClairDynInt *b){CLAIR_SMALL_INT(a,k);return clair_int_bin(&a,b,'%');}
ClairDynInt *clair_rt_bigint_addmul_int(const ClairDynInt *a,const ClairDynInt *b,int64_t k){
    ClairDynInt *r=clair_new_int();clair_num_check(clair_big_addmul_i64(&r->v,&a->v,&b->v,k));return r;
}
int clair_rt_bigint_cmp_int(const ClairDynInt *a,int64_t k){CLAIR_SMALL_INT(b,k);return clair_big_cmp(&a->v,&b.v);}
void clair_rt_bigint_print(const ClairDynInt *v) {
    char *str=NULL;clair_num_check(clair_big_to_decimal(&v->v,&str));puts(str);free(str);
}
ClairDynFloat *clair_rt_bigfloat_parse(const char *s,int64_t precision) {
    if(precision<1 || precision>1000000)clair_num_fail("bigfloat precision must be between 1 and 1000000");
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

/* ------------------------------------------------------------------ */
/* V0.45 : fonctions mathematiques                                     */
/* ------------------------------------------------------------------ */
#define BF_FN1(NAME) ClairDynFloat *clair_rt_bigfloat_##NAME(const ClairDynFloat *x) { \
    ClairDynFloat *r = clair_new_float(x->precision); \
    clair_num_check(clair_bf_##NAME(&r->v, &x->v, x->precision)); return r; }
BF_FN1(sqrt) BF_FN1(exp) BF_FN1(log) BF_FN1(log10) BF_FN1(log2) BF_FN1(sin) BF_FN1(cos) BF_FN1(tan)
BF_FN1(asin) BF_FN1(acos) BF_FN1(atan) BF_FN1(sinh) BF_FN1(cosh) BF_FN1(tanh)
#define BF_FN2(NAME) ClairDynFloat *clair_rt_bigfloat_##NAME(const ClairDynFloat *a, const ClairDynFloat *b) { \
    uint32_t p = a->precision > b->precision ? a->precision : b->precision; \
    ClairDynFloat *r = clair_new_float(p); \
    clair_num_check(clair_bf_##NAME(&r->v, &a->v, &b->v, p)); return r; }
BF_FN2(atan2) BF_FN2(hypot) BF_FN2(pow)
ClairDynFloat *clair_rt_bigfloat_abs(const ClairDynFloat *x) {
    ClairDynFloat *r = clair_new_float(x->precision);
    clair_num_check(clair_big_copy(&r->v.coefficient, &x->v.coefficient));
    r->v.exp10 = x->v.exp10;
    if (r->v.coefficient.sign < 0) r->v.coefficient.sign = 1;
    return r;
}
static uint32_t clair_num_digits(int64_t digits) {
    if (digits < 1 || digits > 1000000) clair_num_fail("bigfloat precision must be between 1 and 1000000");
    return (uint32_t)digits;
}
ClairDynFloat *clair_rt_bigfloat_pi(int64_t digits) {
    uint32_t p = clair_num_digits(digits);
    ClairDynFloat *r = clair_new_float(p);
    clair_num_check(clair_bf_pi(&r->v, p));
    return r;
}
ClairDynInt *clair_rt_bigfloat_floor(const ClairDynFloat *x, int mode) {
    ClairDynInt *r = clair_new_int();
    clair_num_check(clair_bf_floor(&r->v, &x->v, mode));
    return r;
}
/* un nombre ordinaire dans un calcul avec un bigfloat : exact, precision 1 (celle du bigfloat l'emporte) */
ClairDynFloat *clair_rt_bigfloat_exact_int(int64_t k) {
    ClairDynFloat *r = clair_new_float(1);
    ClairBigInt c; clair_big_init(&c);
    int rc = clair_big_from_i64(&c, k);
    if (!rc) rc = clair_bf_from_scaled(&r->v, &c, 0, 1000000, 0);
    clair_big_clear(&c);
    clair_num_check(rc);
    return r;
}
ClairDynFloat *clair_rt_bigfloat_exact_float(double x) {
    if (x != x || x - x != 0) clair_num_fail("bigfloat cannot hold inf or nan");
    ClairDynFloat *r = clair_new_float(1);
    clair_num_check(clair_bf_from_double(&r->v, x));
    return r;
}
ClairDynFloat *clair_rt_bigfloat_exact_bigint(const ClairDynInt *b) {
    ClairDynFloat *r = clair_new_float(1);
    clair_num_check(clair_bf_from_bigint(&r->v, &b->v));
    return r;
}
/* bigfloat(x, chiffres) : x arrondi */
ClairDynFloat *clair_rt_bigfloat_round(const ClairDynFloat *x, int64_t digits) {
    uint32_t p = clair_num_digits(digits);
    ClairDynFloat *r = clair_new_float(p);
    clair_num_check(clair_bf_from_scaled(&r->v, &x->v.coefficient, x->v.exp10, p, 0));
    return r;
}
double clair_rt_bigfloat_to_float(const ClairDynFloat *x) {
    double d = 0;
    clair_num_check(clair_bf_to_double(&x->v, &d));
    return d;
}
int64_t clair_rt_bigfloat_to_int(const ClairDynFloat *x) {
    ClairBigInt t; clair_big_init(&t);
    int64_t v = 0;
    int rc = clair_bf_floor(&t, &x->v, 0);
    if (!rc) rc = clair_big_to_i64(&t, &v);
    clair_big_clear(&t);
    if (rc == CLAIR_BIG_TOO_LARGE) clair_num_fail("int() argument out of range");
    clair_num_check(rc);
    return v;
}
double clair_rt_bigint_to_float(const ClairDynInt *b) {
    double d = 0;
    clair_num_check(clair_big_to_double(&b->v, &d));
    return d;
}
int64_t clair_rt_bigint_to_int(const ClairDynInt *b) {
    int64_t v = 0;
    if (clair_big_to_i64(&b->v, &v)) clair_num_fail("int() argument out of range");
    return v;
}
ClairDynInt *clair_rt_bigint_isqrt(const ClairDynInt *b) {
    ClairDynInt *r = clair_new_int();
    int rc = clair_big_isqrt(&r->v, &b->v);
    if (rc == CLAIR_BIG_DOMAIN) clair_num_fail("isqrt() argument must be nonnegative");
    clair_num_check(rc);
    return r;
}
int64_t clair_rt_int_isqrt(int64_t k) {
    if (k < 0) clair_num_fail("isqrt() argument must be nonnegative");
    uint64_t n = (uint64_t)k, x = (uint64_t)sqrt((double)n);
    while (x * x > n) x--;
    while ((x + 1) * (x + 1) <= n) x++;
    return (int64_t)x;
}
ClairDynInt *clair_rt_bigint_gcd(const ClairDynInt *a, const ClairDynInt *b) {
    ClairDynInt *r = clair_new_int();
    clair_num_check(clair_big_gcd(&r->v, &a->v, &b->v));
    return r;
}
int64_t clair_rt_int_gcd(int64_t a, int64_t b) {
    uint64_t x = a < 0 ? (uint64_t)(-(a + 1)) + 1u : (uint64_t)a, y = b < 0 ? (uint64_t)(-(b + 1)) + 1u : (uint64_t)b;
    while (y) { uint64_t t = x % y; x = y; y = t; }
    if (x > (uint64_t)INT64_MAX) clair_num_fail("gcd() result out of range for int");
    return (int64_t)x;
}
ClairDynInt *clair_rt_bigint_pow(const ClairDynInt *b, int64_t e) {
    if (e < 0) clair_num_fail("pow() of a bigint: negative exponent");
    ClairDynInt *r = clair_new_int();
    clair_num_check(clair_big_pow_u64(&r->v, &b->v, (uint64_t)e));
    return r;
}
ClairDynInt *clair_rt_bigint_powmod(const ClairDynInt *b, const ClairDynInt *e, const ClairDynInt *m) {
    ClairDynInt *r = clair_new_int();
    int rc = clair_big_powmod(&r->v, &b->v, &e->v, &m->v);
    if (rc == CLAIR_BIG_DIVZERO) clair_num_fail("pow() 3rd argument cannot be 0");
    if (rc == CLAIR_BIG_DOMAIN) clair_num_fail("pow() 2nd argument cannot be negative");
    clair_num_check(rc);
    return r;
}
ClairDynInt *clair_rt_bigint_abs(const ClairDynInt *b) {
    ClairDynInt *r = clair_new_int();
    clair_num_check(clair_big_copy(&r->v, &b->v));
    if (r->v.sign < 0) r->v.sign = 1;
    return r;
}
