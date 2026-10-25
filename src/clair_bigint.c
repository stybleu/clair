#include "clair_bigint.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void clair_big_init(ClairBigInt *x) { memset(x, 0, sizeof(*x)); }
void clair_big_clear(ClairBigInt *x) { free(x->word); clair_big_init(x); }
static void swap_big(ClairBigInt *a, ClairBigInt *b) { ClairBigInt x=*a; *a=*b; *b=x; }
static void normalize(ClairBigInt *x) {
    while (x->len && !x->word[x->len-1]) x->len--;
    if (!x->len) x->sign=0;
}
static int reserve(ClairBigInt *x, size_t needed) {
    if (needed > SIZE_MAX / sizeof(uint64_t)) return CLAIR_BIG_TOO_LARGE;
    if (needed <= x->cap) return CLAIR_BIG_OK;
    size_t new_cap = needed;
    if (needed < 1000000 && x->cap) {
        size_t expanded=x->cap + x->cap/2 + 1;
        if (expanded > needed && expanded <= SIZE_MAX/sizeof(uint64_t)) new_cap=expanded;
    }
    uint64_t *p = (uint64_t *)realloc(x->word, new_cap*sizeof(uint64_t));
    if (!p) return CLAIR_BIG_NOMEM;
    memset(p+x->cap, 0, (new_cap-x->cap)*sizeof(uint64_t));
    x->word=p; x->cap=new_cap;
    return CLAIR_BIG_OK;
}
static int cmp_abs(const ClairBigInt *a, const ClairBigInt *b) {
    if (a->len != b->len) return a->len>b->len ? 1:-1;
    for (size_t i=a->len;i>0;) { i--; if(a->word[i]!=b->word[i]) return a->word[i]>b->word[i]?1:-1; }
    return 0;
}
int clair_big_cmp(const ClairBigInt *a, const ClairBigInt *b) {
    if (a->sign != b->sign) return a->sign>b->sign ? 1:-1;
    return a->sign * cmp_abs(a,b);
}
int clair_big_from_i64(ClairBigInt *out, int64_t val) {
    ClairBigInt r; clair_big_init(&r);
    if(val) {
        int rc=reserve(&r,1); if(rc) return rc;
        r.word[0]= val<0 ? (uint64_t)(-(val+1))+1u : (uint64_t)val;
        r.len=1; r.sign=val<0?-1:1;
    }
    swap_big(out,&r); clair_big_clear(&r); return 0;
}
int clair_big_copy(ClairBigInt *out, const ClairBigInt *a) {
    if(out==a) return 0;
    ClairBigInt r; clair_big_init(&r);
    int rc=reserve(&r,a->len); if(rc) return rc;
    if(a->len) memcpy(r.word,a->word,a->len*sizeof(uint64_t));
    r.len=a->len; r.sign=a->sign;
    swap_big(out,&r); clair_big_clear(&r); return 0;
}
size_t clair_big_bit_length(const ClairBigInt *x) {
    if (!x->len) return 0;
    uint64_t w=x->word[x->len-1];
    return (x->len-1)*64 + (64u-(unsigned)__builtin_clzll(w));
}
int clair_big_get_bit(const ClairBigInt *a, size_t bit) {
    size_t i=bit/64;
    return i<a->len ? (int)((a->word[i]>>(bit%64))&1u) : 0;
}
int clair_big_set_bit(ClairBigInt *x, size_t bit) {
    if(x->sign<0) return CLAIR_BIG_INVALID;
    size_t i=bit/64;
    if (i==SIZE_MAX) return CLAIR_BIG_TOO_LARGE;
    int rc=reserve(x,i+1); if(rc) return rc;
    if(x->len<=i) memset(x->word+x->len,0,(i+1-x->len)*sizeof(uint64_t));
    x->word[i] |= UINT64_C(1)<<(bit%64);
    if(x->len<=i) x->len=i+1;
    x->sign=1; return 0;
}
static int add_abs(ClairBigInt *r,const ClairBigInt*a,const ClairBigInt*b) {
    size_t n=a->len>b->len?a->len:b->len;
    if(n==SIZE_MAX) return CLAIR_BIG_TOO_LARGE;
    int rc=reserve(r,n+1); if(rc) return rc;
    uint64_t carry=0;
    for(size_t i=0;i<n;i++) {
        uint64_t x=i<a->len?a->word[i]:0,y=i<b->len?b->word[i]:0;
        unsigned __int128 t=(unsigned __int128)x+y+carry;
        r->word[i]=(uint64_t)t;
        carry=(uint64_t)(t>>64);
    }
    r->word[n]=carry; r->len=n+(carry!=0); return 0;
}
static void sub_abs(ClairBigInt *r,const ClairBigInt*a,const ClairBigInt*b) { /* |a| >= |b| */
    uint64_t borrow=0;
    for(size_t i=0;i<a->len;i++) {
        uint64_t x=a->word[i],y=i<b->len?b->word[i]:0;
        uint64_t t=x-y, under=x<y;
        r->word[i]=t-borrow;
        borrow=under || t<borrow;
    }
    r->len=a->len; normalize(r);
}
int clair_big_add(ClairBigInt *out,const ClairBigInt*a,const ClairBigInt*b) {
    ClairBigInt r; clair_big_init(&r);
    int rc=0;
    if(a->sign==b->sign) {
        rc=add_abs(&r,a,b);
        if(!rc) r.sign=a->sign;
    } else {
        int c=cmp_abs(a,b);
        const ClairBigInt *x=c>=0?a:b, *y=c>=0?b:a;
        rc=reserve(&r,x->len);
        if(!rc) { sub_abs(&r,x,y); if(r.len) r.sign=x->sign; }
    }
    if(rc) {clair_big_clear(&r);return rc;}
    normalize(&r);swap_big(out,&r);clair_big_clear(&r);return 0;
}
int clair_big_sub(ClairBigInt *out,const ClairBigInt*a,const ClairBigInt*b) {
    ClairBigInt neg=*b;
    neg.sign=-neg.sign;
    return clair_big_add(out,a,&neg);
}
/*
 * Multiplication de magnitudes : classique, Karatsuba ou Toom-Cook 3.
 * Les entrées sont normalisées ; les fonctions internes acceptent aussi
 * les blocs de taille nulle créés par le découpage récursif.
 * CLAIR_BIG_FORCE_CLASSIC / CLAIR_BIG_FORCE_KARATSUBA permettent les comparaisons.
 */
#ifndef CLAIR_BIG_KARATSUBA_THRESHOLD
#define CLAIR_BIG_KARATSUBA_THRESHOLD 48u /* mots de 64 bits */
#endif

static void mul_classic_words(uint64_t *r, const uint64_t *a, size_t na,
                              const uint64_t *b, size_t nb) {
    /* r[0..na+nb) a été initialisé à zéro par l'appelant. */
    for (size_t i = 0; i < na; i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < nb; j++) {
            unsigned __int128 t = (unsigned __int128)a[i] * b[j]
                                 + r[i+j] + carry;
            r[i+j] = (uint64_t)t;
            carry = (uint64_t)(t >> 64);
        }
        r[i+nb] = carry;
    }
}

#ifndef CLAIR_BIG_FORCE_CLASSIC
/* Addition de mots non signés. r dispose de max(na,nb)+1 mots. */
static size_t add_words(uint64_t *r, const uint64_t *a, size_t na,
                        const uint64_t *b, size_t nb) {
    size_t n = na > nb ? na : nb;
    uint64_t carry = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned __int128 t = (unsigned __int128)(i < na ? a[i] : 0)
                                 + (i < nb ? b[i] : 0) + carry;
        r[i] = (uint64_t)t;
        carry = (uint64_t)(t >> 64);
    }
    if (carry) r[n++] = carry;
    while (n && !r[n-1]) n--;
    return n;
}

/* r -= b. Les retenues doivent rester dans r ; code interne exact. */
static int sub_words(uint64_t *r, size_t nr, const uint64_t *b, size_t nb) {
    uint64_t borrow = 0;
    for (size_t i = 0; i < nr; i++) {
        uint64_t x = r[i], y = i < nb ? b[i] : 0;
        uint64_t d = x - y;
        r[i] = d - borrow;
        borrow = (uint64_t)(x < y || d < borrow);
    }
    /* Un emprunt final indique une erreur dans un invariant de Karatsuba. */
    return borrow ? CLAIR_BIG_INVALID : CLAIR_BIG_OK;
}

/* Ajoute b décalé de shift mots dans r[0..nr), sans perte de bits. */
static int add_words_shifted(uint64_t *r, size_t nr, const uint64_t *b,
                             size_t nb, size_t shift) {
    if (shift > nr) return CLAIR_BIG_INVALID;
    size_t i = 0;
    uint64_t carry = 0;
    for (; i < nb && i < nr-shift; i++) {
        unsigned __int128 t = (unsigned __int128)r[shift+i] + b[i] + carry;
        r[shift+i] = (uint64_t)t;
        carry = (uint64_t)(t >> 64);
    }
    /* Les mots hors de la destination doivent impérativement valoir zéro. */
    for (; i < nb; i++) if (b[i]) return CLAIR_BIG_INVALID;
    for (size_t j = shift+i; carry && j < nr; j++) {
        unsigned __int128 t = (unsigned __int128)r[j] + carry;
        r[j] = (uint64_t)t;
        carry = (uint64_t)(t >> 64);
    }
    return carry ? CLAIR_BIG_INVALID : CLAIR_BIG_OK;
}

#endif


/* Toom-Cook 3 sur magnitudes équilibrées, sans flottants.
 * Évaluations aux points 0, 1, -1, 2, infini et interpolation exacte.
 * L'implémentation privilégie des invariants contrôlables à la réduction
 * du nombre d'allocations (prototype, non encore optimisé en mémoire).
 */
#if !defined(CLAIR_BIG_FORCE_CLASSIC) && !defined(CLAIR_BIG_FORCE_KARATSUBA)
#ifndef CLAIR_BIG_TOOM3_THRESHOLD
#define CLAIR_BIG_TOOM3_THRESHOLD 2048u /* mots de 64 bits : seuil ARM64 provisoire */
#endif

static ClairBigInt words_view(const uint64_t *w, size_t n,
                              size_t start, size_t count) {
    ClairBigInt v = {0};
    if (start >= n) return v;
    size_t available = n - start;
    if (count > available) count = available;
    while (count && w[start+count-1] == 0) count--;
    v.word = (uint64_t *)(w+start); /* vue empruntée, jamais libérée/modifiée */
    v.len = count;
    v.sign = count ? 1 : 0;
    return v;
}

static int toom_eval(const ClairBigInt *a0, const ClairBigInt *a1,
                     const ClairBigInt *a2, ClairBigInt *v1,
                     ClairBigInt *vm1, ClairBigInt *v2) {
    ClairBigInt s = {0}, d = {0}, q = {0};
    int rc = clair_big_add(&s, a0, a2);
    if (!rc) rc = clair_big_add(v1, &s, a1);
    if (!rc) rc = clair_big_sub(vm1, &s, a1);
    if (!rc) rc = clair_big_shift_left(&d, a1, 1);
    if (!rc) rc = clair_big_shift_left(&q, a2, 2);
    if (!rc) rc = clair_big_add(&s, a0, &d);
    if (!rc) rc = clair_big_add(v2, &s, &q);
    clair_big_clear(&s); clair_big_clear(&d); clair_big_clear(&q);
    return rc;
}

/* Quotient entier exact par 2 ou 3, avec contrôle obligatoire du reste. */
static int toom_exact_div(ClairBigInt *out, const ClairBigInt *x, int d) {
    ClairBigInt divisor = {0}, q = {0}, rem = {0};
    int rc = clair_big_from_i64(&divisor, d);
    if (!rc) rc = clair_big_divmod(&q, &rem, x, &divisor);
    if (!rc && rem.sign != 0) rc = CLAIR_BIG_INVALID;
    if (!rc) rc = clair_big_copy(out, &q);
    clair_big_clear(&divisor); clair_big_clear(&q); clair_big_clear(&rem);
    return rc;
}

/* r += x * 2^(64*words), avec gestion des débordements size_t. */
static int toom_accumulate(ClairBigInt *r, const ClairBigInt *x,
                           size_t words) {
    if (x->sign == 0) return CLAIR_BIG_OK;
    if (words > SIZE_MAX/64) return CLAIR_BIG_TOO_LARGE;
    ClairBigInt shifted = {0};
    int rc = clair_big_shift_left(&shifted, x, words*64);
    if (!rc) rc = clair_big_add(r, r, &shifted);
    clair_big_clear(&shifted);
    return rc;
}

enum {
    TP_A1, TP_AM1, TP_A2, TP_B1, TP_BM1, TP_B2,
    TP_R0, TP_R1, TP_RM1, TP_R2, TP_R4,
    TP_S1, TP_S2, TP_C1, TP_C2, TP_C3,
    TP_WORK, TP_TMP, TP_RESULT, TP_COUNT
};

static int mul_toom3_words(uint64_t *out, const uint64_t *a, size_t na,
                            const uint64_t *b, size_t nb) {
    const size_t large = na > nb ? na : nb;
    const size_t k = large / 3 + (large % 3 != 0);
    if (k > SIZE_MAX/4 || na > SIZE_MAX-nb) return CLAIR_BIG_TOO_LARGE;
    const ClairBigInt a0 = words_view(a,na,0,k);
    const ClairBigInt a1 = words_view(a,na,k,k);
    const ClairBigInt a2 = words_view(a,na,2*k,k);
    const ClairBigInt b0 = words_view(b,nb,0,k);
    const ClairBigInt b1 = words_view(b,nb,k,k);
    const ClairBigInt b2 = words_view(b,nb,2*k,k);
    ClairBigInt t[TP_COUNT] = {{0}};
    int rc = toom_eval(&a0,&a1,&a2,&t[TP_A1],&t[TP_AM1],&t[TP_A2]);
    if (!rc) rc = toom_eval(&b0,&b1,&b2,&t[TP_B1],&t[TP_BM1],&t[TP_B2]);
    if (!rc) rc = clair_big_mul(&t[TP_R0], &a0, &b0);
    if (!rc) rc = clair_big_mul(&t[TP_R4], &a2, &b2);
    if (!rc) rc = clair_big_mul(&t[TP_R1], &t[TP_A1], &t[TP_B1]);
    if (!rc) rc = clair_big_mul(&t[TP_RM1], &t[TP_AM1], &t[TP_BM1]);
    if (!rc) rc = clair_big_mul(&t[TP_R2], &t[TP_A2], &t[TP_B2]);

    /* s1 = (r(1)+r(-1))/2 = c0+c2+c4
     * s2 = (r(1)-r(-1))/2 = c1+c3
     */
    if (!rc) rc = clair_big_add(&t[TP_S1], &t[TP_R1], &t[TP_RM1]);
    if (!rc) rc = toom_exact_div(&t[TP_S1], &t[TP_S1], 2);
    if (!rc) rc = clair_big_sub(&t[TP_S2], &t[TP_R1], &t[TP_RM1]);
    if (!rc) rc = toom_exact_div(&t[TP_S2], &t[TP_S2], 2);
    if (!rc) rc = clair_big_sub(&t[TP_C2], &t[TP_S1], &t[TP_R0]);
    if (!rc) rc = clair_big_sub(&t[TP_C2], &t[TP_C2], &t[TP_R4]);

    /* work = (r(2)-c0-16*c4)/2 - 2*c2 = c1+4*c3
     * c3 = (work-s2)/3 ; c1=s2-c3.
     */
    if (!rc) rc = clair_big_shift_left(&t[TP_TMP], &t[TP_R4], 4);
    if (!rc) rc = clair_big_sub(&t[TP_WORK], &t[TP_R2], &t[TP_R0]);
    if (!rc) rc = clair_big_sub(&t[TP_WORK], &t[TP_WORK], &t[TP_TMP]);
    if (!rc) rc = toom_exact_div(&t[TP_WORK], &t[TP_WORK], 2);
    if (!rc) rc = clair_big_shift_left(&t[TP_TMP], &t[TP_C2], 1);
    if (!rc) rc = clair_big_sub(&t[TP_WORK], &t[TP_WORK], &t[TP_TMP]);
    if (!rc) rc = clair_big_sub(&t[TP_C3], &t[TP_WORK], &t[TP_S2]);
    if (!rc) rc = toom_exact_div(&t[TP_C3], &t[TP_C3], 3);
    if (!rc) rc = clair_big_sub(&t[TP_C1], &t[TP_S2], &t[TP_C3]);

    /* Les coefficients de A*B doivent être des magnitudes non négatives. */
    if (!rc && (t[TP_C1].sign < 0 || t[TP_C2].sign < 0 ||
                t[TP_C3].sign < 0)) rc = CLAIR_BIG_INVALID;
    if (!rc) rc = clair_big_copy(&t[TP_RESULT], &t[TP_R0]);
    if (!rc) rc = toom_accumulate(&t[TP_RESULT], &t[TP_C1], k);
    if (!rc) rc = toom_accumulate(&t[TP_RESULT], &t[TP_C2], 2*k);
    if (!rc) rc = toom_accumulate(&t[TP_RESULT], &t[TP_C3], 3*k);
    if (!rc) rc = toom_accumulate(&t[TP_RESULT], &t[TP_R4], 4*k);
    if (!rc && (t[TP_RESULT].sign < 0 || t[TP_RESULT].len > na+nb))
        rc = CLAIR_BIG_INVALID;
    if (!rc && t[TP_RESULT].len)
        memcpy(out, t[TP_RESULT].word, t[TP_RESULT].len*sizeof(uint64_t));
    for (size_t i=0; i<TP_COUNT; i++) clair_big_clear(&t[i]);
    return rc;
}
#endif

static int mul_words(uint64_t *r, const uint64_t *a, size_t na,
                     const uint64_t *b, size_t nb) {
    if (!na || !nb) return CLAIR_BIG_OK;
#ifdef CLAIR_BIG_FORCE_CLASSIC
    mul_classic_words(r, a, na, b, nb);
    return CLAIR_BIG_OK;
#else
    /* Déséquilibre important : la méthode classique évite les copies inutiles. */
    size_t small = na < nb ? na : nb;
    size_t large = na > nb ? na : nb;
    if (small <= CLAIR_BIG_KARATSUBA_THRESHOLD || large / small >= 2) {
        mul_classic_words(r, a, na, b, nb);
        return CLAIR_BIG_OK;
    }
#ifndef CLAIR_BIG_FORCE_KARATSUBA
    /* Eviter Toom pour les tailles déséquilibrées : trois blocs incomplets. */
    if (small > CLAIR_BIG_TOOM3_THRESHOLD && large-small <= large/8)
        return mul_toom3_words(r, a, na, b, nb);
#endif
    size_t k = large / 2;
    size_t a0 = na < k ? na : k, b0 = nb < k ? nb : k;
    size_t a1 = na > k ? na-k : 0, b1 = nb > k ? nb-k : 0;
    if (!a1 || !b1) {
        mul_classic_words(r, a, na, b, nb);
        return CLAIR_BIG_OK;
    }
    /* z0=a0*b0 ; z2=a1*b1 ; milieu=(a0+a1)*(b0+b1)-z0-z2. */
    int rc = mul_words(r, a, a0, b, b0);
    if (rc) return rc;
    rc = mul_words(r+2*k, a+k, a1, b+k, b1);
    if (rc) return rc;

    size_t sa = a0 > a1 ? a0 : a1;
    size_t sb = b0 > b1 ? b0 : b1;
    /* Tous les tableaux temporaires sont bornés avant allocation. */
    if (sa == SIZE_MAX || sb == SIZE_MAX || sa+1 > SIZE_MAX/sizeof(uint64_t)
        || sb+1 > SIZE_MAX/sizeof(uint64_t)
        || sa+1 > SIZE_MAX-(sb+1)
        || sa+sb+2 > SIZE_MAX/sizeof(uint64_t)) return CLAIR_BIG_TOO_LARGE;
    uint64_t *sum_a = calloc(sa+1, sizeof(uint64_t));
    uint64_t *sum_b = calloc(sb+1, sizeof(uint64_t));
    uint64_t *middle = calloc(sa+sb+2, sizeof(uint64_t));
    if (!sum_a || !sum_b || !middle) {
        free(sum_a); free(sum_b); free(middle);
        return CLAIR_BIG_NOMEM;
    }
    size_t len_a = add_words(sum_a, a, a0, a+k, a1);
    size_t len_b = add_words(sum_b, b, b0, b+k, b1);
    size_t len_middle = len_a + len_b;
    rc = mul_words(middle, sum_a, len_a, sum_b, len_b);
    if (!rc) rc = sub_words(middle, len_middle, r, a0+b0);
    if (!rc) rc = sub_words(middle, len_middle, r+2*k, a1+b1);
    if (!rc) rc = add_words_shifted(r, na+nb, middle, len_middle, k);
    free(sum_a); free(sum_b); free(middle);
    return rc;
#endif
}

int clair_big_mul(ClairBigInt *out, const ClairBigInt *a, const ClairBigInt *b) {
    ClairBigInt r; clair_big_init(&r);
    if (!a->len || !b->len) {
        swap_big(out, &r); clair_big_clear(&r); return CLAIR_BIG_OK;
    }
    if (a->len > SIZE_MAX-b->len) return CLAIR_BIG_TOO_LARGE;
    size_t n = a->len+b->len;
    int rc = reserve(&r,n);
    if (rc) return rc;
    rc = mul_words(r.word,a->word,a->len,b->word,b->len);
    if (rc) { clair_big_clear(&r); return rc; }
    r.len=n; r.sign=a->sign*b->sign; normalize(&r);
    swap_big(out,&r); clair_big_clear(&r); return CLAIR_BIG_OK;
}
int clair_big_shift_left(ClairBigInt *out,const ClairBigInt*a,size_t bits) {
    ClairBigInt r;clair_big_init(&r);
    if (!a->len) {swap_big(out,&r);clair_big_clear(&r);return 0;}
    size_t wordshift=bits/64; unsigned rest=(unsigned)(bits%64);
    if(wordshift>SIZE_MAX-a->len-1) return CLAIR_BIG_TOO_LARGE;
    size_t n=wordshift+a->len+1;
    int rc=reserve(&r,n); if(rc) return rc;
    for(size_t i=0;i<a->len;i++) {
        r.word[i+wordshift]|=a->word[i]<<rest;
        if(rest) r.word[i+wordshift+1]|=a->word[i]>>(64-rest);
    }
    r.len=n;r.sign=a->sign;normalize(&r);
    swap_big(out,&r);clair_big_clear(&r);return 0;
}
/* Division multi-mots normalisee, base 2^64 (variante de Knuth, algorithme D).
 * La multiplication d'essai utilise uint128 pour ne perdre aucun bit.
 * Allocations bornées ; aucun changement des sorties en cas d'erreur.
 * Le diviseur mono-mot est traité séparément dans clair_big_divmod().
 */
#ifndef CLAIR_BIG_FORCE_BIT_DIVISION
static int div_abs_normalized(ClairBigInt *q, ClairBigInt *r,
                              const ClairBigInt *a, const ClairBigInt *b) {
    const size_t n=b->len, na=a->len;
    const size_t m=na-n;
    if (n<2 || na<n || na==SIZE_MAX || n>SIZE_MAX/sizeof(uint64_t)
        || na+1>SIZE_MAX/sizeof(uint64_t)) return CLAIR_BIG_TOO_LARGE;
    uint64_t *u=(uint64_t*)calloc(na+1,sizeof(uint64_t));
    uint64_t *v=(uint64_t*)malloc(n*sizeof(uint64_t));
    if (!u || !v) {free(u);free(v);return CLAIR_BIG_NOMEM;}
    const unsigned sh=(unsigned)__builtin_clzll(b->word[n-1]);
    if(sh==0) {
        memcpy(v,b->word,n*sizeof(uint64_t));
        memcpy(u,a->word,na*sizeof(uint64_t));
    } else {
        for(size_t i=0;i<n;i++) {
            v[i]=(b->word[i]<<sh) | (i ? b->word[i-1]>>(64-sh) : 0);
        }
        for(size_t i=0;i<na;i++) {
            u[i]=(a->word[i]<<sh) | (i ? a->word[i-1]>>(64-sh) : 0);
        }
        u[na]=a->word[na-1]>>(64-sh);
    }
    int rc=reserve(q,m+1);
    if (!rc) rc=reserve(r,n);
    if (rc) {free(u);free(v);return rc;}
    memset(q->word,0,(m+1)*sizeof(uint64_t));
    q->len=m+1;
    const unsigned __int128 BASE=((unsigned __int128)1)<<64;
    for(size_t jj=m+1;jj>0;) {
        const size_t j=--jj;
        unsigned __int128 top=((unsigned __int128)u[j+n]<<64)|u[j+n-1];
        unsigned __int128 est=top/v[n-1];
        uint64_t guess=est>=BASE ? UINT64_MAX : (uint64_t)est;
        unsigned __int128 residue=top-(unsigned __int128)guess*v[n-1];
        while(residue<BASE &&
              (unsigned __int128)guess*v[n-2] >
                 (residue<<64) + u[j+n-2]) {
            guess--;
            residue+=v[n-1];
        }
        uint64_t carry=0,borrow=0;
        for(size_t i=0;i<n;i++) {
            unsigned __int128 prod=(unsigned __int128)guess*v[i]+carry;
            carry=(uint64_t)(prod>>64);
            uint64_t lo=(uint64_t)prod;
            uint64_t cur=u[j+i];
            uint64_t diff=cur-lo;
            u[j+i]=diff-borrow;
            borrow=(uint64_t)(cur<lo || diff<borrow);
        }
        unsigned __int128 subtop=(unsigned __int128)carry+borrow;
        int negative=((unsigned __int128)u[j+n]<subtop);
        u[j+n]=(uint64_t)((unsigned __int128)u[j+n]-subtop);
        if(negative) {
            /* L'estimation est trop grande d'une unité : ajouter v. */
            guess--;
            uint64_t c=0;
            for(size_t i=0;i<n;i++) {
                unsigned __int128 sum=(unsigned __int128)u[j+i]+v[i]+c;
                u[j+i]=(uint64_t)sum;
                c=(uint64_t)(sum>>64);
            }
            u[j+n]+=c; /* débordement sur 64 bits intentionnel */
        }
        q->word[j]=guess;
    }
    for(size_t i=0;i<n;i++) {
        r->word[i]=sh==0 ? u[i] :
                (u[i]>>sh) | (i+1<n ? u[i+1]<<(64-sh) : 0);
    }
    r->len=n;
    free(u);free(v);
    return CLAIR_BIG_OK;
}
#else
static int shift1_add_bit(ClairBigInt *x,int bit) {
    size_t n=x->len;
    if(n==SIZE_MAX) return CLAIR_BIG_TOO_LARGE;
    int rc=reserve(x,n+1);if(rc)return rc;
    uint64_t carry=(uint64_t)bit;
    for(size_t i=0;i<n;i++) {uint64_t next=x->word[i]>>63;x->word[i]=(x->word[i]<<1)|carry;carry=next;}
    if(carry) {x->word[n]=carry;x->len=n+1;}
    else if(!n && bit) {x->word[0]=1;x->len=1;}
    x->sign=x->len?1:0;return 0;
}
static void sub_abs_inplace(ClairBigInt *a,const ClairBigInt*b) {
    uint64_t borrow=0;
    for(size_t i=0;i<a->len;i++) {
        uint64_t x=a->word[i],y=i<b->len?b->word[i]:0;
        uint64_t t=x-y;
        a->word[i]=t-borrow;
        borrow=(x<y)||(t<borrow);
    }
    normalize(a);
}
#endif
int clair_big_divmod(ClairBigInt *quo,ClairBigInt *rem,const ClairBigInt*a,const ClairBigInt*b) {
    if (quo==rem) return CLAIR_BIG_INVALID;
    if (!b->len) return CLAIR_BIG_DIVZERO;
    ClairBigInt q,r;clair_big_init(&q);clair_big_init(&r);
    int rc=0;
    if(a->len && cmp_abs(a,b)<0) {
        rc=clair_big_copy(&r,a);
        if(rc)goto fail;
        r.sign=1;
    } else if(b->len==1 && a->len) { /* diviseur mono-mot : lineaire */
        rc=reserve(&q,a->len);if(rc)goto fail;
        q.len=a->len;
        uint64_t remainder=0;
        for(size_t i=a->len;i>0;) {
            i--;
            unsigned __int128 numerator=((unsigned __int128)remainder<<64)|a->word[i];
            q.word[i]=(uint64_t)(numerator/b->word[0]);
            remainder=(uint64_t)(numerator%b->word[0]);
        }
        if(remainder) {
            rc=reserve(&r,1);if(rc)goto fail;
            r.word[0]=remainder;r.len=1;
        }
    } else if(a->len) {
#ifdef CLAIR_BIG_FORCE_BIT_DIVISION
        size_t bits=clair_big_bit_length(a);
        rc=reserve(&q,(bits+63)/64);if(rc)goto fail;
        q.len=(bits+63)/64;
        for(size_t bit=bits;bit>0;) {
            bit--;
            rc=shift1_add_bit(&r,clair_big_get_bit(a,bit));if(rc)goto fail;
            if(cmp_abs(&r,b)>=0) {
                sub_abs_inplace(&r,b);
                q.word[bit/64] |= UINT64_C(1)<<(bit%64);
            }
        }
#else
        rc=div_abs_normalized(&q,&r,a,b);if(rc)goto fail;
#endif
    }
    q.sign=a->sign*b->sign;r.sign=a->sign;
    normalize(&q);normalize(&r);
    swap_big(quo,&q);swap_big(rem,&r);
fail:
    clair_big_clear(&q);clair_big_clear(&r);return rc;
}
int clair_big_pow_u64(ClairBigInt *out,const ClairBigInt *base,uint64_t exponent) {
    ClairBigInt result,b,tmp;clair_big_init(&result);clair_big_init(&b);clair_big_init(&tmp);
    int rc=clair_big_from_i64(&result,1);if(rc)goto end;
    rc=clair_big_copy(&b,base);if(rc)goto end;
    while(exponent) {
        if(exponent&1) {rc=clair_big_mul(&tmp,&result,&b);if(rc)goto end;swap_big(&tmp,&result);}
        exponent>>=1;
        if(exponent) {rc=clair_big_mul(&tmp,&b,&b);if(rc)goto end;swap_big(&tmp,&b);}
    }
    swap_big(out,&result);
end:
    clair_big_clear(&result);clair_big_clear(&b);clair_big_clear(&tmp);return rc;
}
int clair_big_from_decimal(ClairBigInt *out,const char *text) {
    if(!text || !*text) return CLAIR_BIG_INVALID;
    int sign=1;
    if(*text=='-' || *text=='+') {if(*text=='-')sign=-1;text++;}
    if(!*text)return CLAIR_BIG_INVALID;
    ClairBigInt r;clair_big_init(&r);
    for(const char *p=text;*p;p++) {
        if(*p<'0' || *p>'9') {clair_big_clear(&r);return CLAIR_BIG_INVALID;}
        uint64_t carry=(uint64_t)(*p-'0');
        for(size_t i=0;i<r.len;i++) {
            unsigned __int128 v=(unsigned __int128)r.word[i]*10u+carry;
            r.word[i]=(uint64_t)v;carry=(uint64_t)(v>>64);
        }
        if(carry) {
            if(r.len==SIZE_MAX) {clair_big_clear(&r);return CLAIR_BIG_TOO_LARGE;}
            int rc=reserve(&r,r.len+1);if(rc){clair_big_clear(&r);return rc;}
            r.word[r.len++]=carry;
        }
    }
    r.sign=r.len?sign:0;
    swap_big(out,&r);clair_big_clear(&r);return 0;
}
int clair_big_to_decimal(const ClairBigInt*x,char **result) {
    if(!result)return CLAIR_BIG_INVALID;
    *result=NULL;
    if(!x->len){*result=(char*)malloc(2);if(!*result)return CLAIR_BIG_NOMEM;strcpy(*result,"0");return 0;}
    uint64_t *scratch=(uint64_t*)malloc(x->len*sizeof(uint64_t));
    if(!scratch)return CLAIR_BIG_NOMEM;
    memcpy(scratch,x->word,x->len*sizeof(uint64_t));
    uint64_t *parts=NULL;size_t psize=0,pn=0,n=x->len;
    while(n) {
        unsigned __int128 rem=0;
        for(size_t i=n;i>0;) {
            i--; unsigned __int128 cur=(rem<<64)|scratch[i];
            scratch[i]=(uint64_t)(cur/UINT64_C(10000000000000000000));
            rem=cur%UINT64_C(10000000000000000000);
        }
        if(pn==psize) {
            size_t next=psize?psize*2:8;
            if(next<=psize || next>SIZE_MAX/sizeof(uint64_t)) {free(scratch);free(parts);return CLAIR_BIG_TOO_LARGE;}
            uint64_t *p=(uint64_t*)realloc(parts,next*sizeof(uint64_t));
            if(!p){free(scratch);free(parts);return CLAIR_BIG_NOMEM;}
            parts=p;psize=next;
        }
        parts[pn++]=(uint64_t)rem;
        while(n && scratch[n-1]==0)n--;
    }
    free(scratch);
    if(pn>(SIZE_MAX-3)/19){free(parts);return CLAIR_BIG_TOO_LARGE;}
    size_t alloc=pn*19+3;
    char *buf=(char*)malloc(alloc);if(!buf){free(parts);return CLAIR_BIG_NOMEM;}
    char *write=buf;
    if(x->sign<0)*write++='-';
    int used=sprintf(write,"%" PRIu64,parts[pn-1]);write+=(size_t)used;
    for(size_t i=pn-1;i>0;) {i--;used=sprintf(write,"%019" PRIu64,parts[i]);write+=(size_t)used;}
    free(parts);*result=buf;return 0;
}
