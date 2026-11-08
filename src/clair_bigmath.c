#include "clair_bigmath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * V0.45 : fonctions mathematiques des grands nombres.
 *
 * Les calculs se font en virgule fixe binaire : un bigint X represente X / 2^B.
 * Multiplier, c'est (A * C) >> B ; diviser par un petit entier ne coute qu'un
 * passage.  Les series (exp, sin, atan...) ne manipulent que ces entiers.  Le
 * resultat est ensuite converti en decimal et arrondi a la precision voulue.
 *
 * B contient la precision demandee (en bits) plus des bits de garde.  Quand le
 * resultat est petit (sin d'un nombre proche de pi, log d'un nombre proche de
 * 1), ses premiers bits sont des zeros : la fonction recommence avec plus de
 * bits, jusqu'a avoir assez de chiffres significatifs.
 */

typedef ClairBigInt BmInt;
#define BM_TRY(call) do { rc = (call); if (rc) goto end; } while (0)
/* un int64 vu comme un bigint d'un mot, sur la pile */
#define BM_SMALL(name, k) \
    uint64_t name##_word = (k) < 0 ? (uint64_t)(-((k) + 1)) + 1u : (uint64_t)(k); \
    BmInt name = { &name##_word, (k) != 0, 1, (k) < 0 ? -1 : (k) > 0 }

static void bm_trim(BmInt *x) {
    while (x->len && !x->word[x->len - 1]) x->len--;
    if (!x->len) x->sign = 0;
}
static int bm_is_zero(const BmInt *x) { return x->sign == 0; }

/* a >> bits, magnitude (vers zero) */
static int bm_shr(BmInt *out, const BmInt *a, size_t bits) {
    int rc = clair_big_copy(out, a);
    if (rc || !bits || !out->len) return rc;
    size_t ws = bits / 64;
    unsigned rs = (unsigned)(bits % 64);
    if (ws >= out->len) { out->len = 0; out->sign = 0; return 0; }
    size_t n = out->len - ws;
    for (size_t i = 0; i < n; i++) {
        uint64_t lo = out->word[i + ws] >> rs;
        uint64_t hi = (rs && i + ws + 1 < out->len) ? out->word[i + ws + 1] << (64 - rs) : 0;
        out->word[i] = lo | hi;
    }
    out->len = n;
    bm_trim(out);
    return 0;
}
static int bm_shl(BmInt *out, const BmInt *a, size_t bits) {
    return bits ? clair_big_shift_left(out, a, bits) : clair_big_copy(out, a);
}
static int bm_shift(BmInt *out, const BmInt *a, int64_t bits) {
    return bits >= 0 ? bm_shl(out, a, (size_t)bits) : bm_shr(out, a, (size_t)(-bits));
}
/* (a * c) >> B */
static int bm_mul(BmInt *out, const BmInt *a, const BmInt *c, size_t B) {
    BmInt t; clair_big_init(&t);
    int rc = clair_big_mul(&t, a, c);
    if (!rc) rc = bm_shr(out, &t, B);
    clair_big_clear(&t);
    return rc;
}
/* (a << B) / c */
static int bm_div(BmInt *out, const BmInt *a, const BmInt *c, size_t B) {
    BmInt t, q, r; clair_big_init(&t); clair_big_init(&q); clair_big_init(&r);
    int rc = bm_shl(&t, a, B);
    if (!rc) rc = clair_big_divmod(&q, &r, &t, c);
    if (!rc) rc = clair_big_copy(out, &q);
    clair_big_clear(&t); clair_big_clear(&q); clair_big_clear(&r);
    return rc;
}
static int bm_divi(BmInt *out, const BmInt *a, int64_t k) {
    BM_SMALL(d, k);
    BmInt q, r; clair_big_init(&q); clair_big_init(&r);
    int rc = clair_big_divmod(&q, &r, a, &d);
    if (!rc) rc = clair_big_copy(out, &q);
    clair_big_clear(&q); clair_big_clear(&r);
    return rc;
}
static int bm_muli(BmInt *out, const BmInt *a, int64_t k) {
    BM_SMALL(m, k);
    return clair_big_mul(out, a, &m);
}
static int bm_one(BmInt *out, size_t B) {
    BmInt one; clair_big_init(&one);
    int rc = clair_big_from_i64(&one, 1);
    if (!rc) rc = bm_shl(out, &one, B);
    clair_big_clear(&one);
    return rc;
}
static int bm_pow10(BmInt *out, uint64_t k) {
    BM_SMALL(ten, 10);
    return clair_big_pow_u64(out, &ten, k);
}
static size_t bm_digits(const BmInt *x) {         /* chiffres decimaux de |x| (0 pour 0) */
    if (!x->sign) return 0;
    char *s = NULL;
    if (clair_big_to_decimal(x, &s)) return 0;
    size_t n = strlen(s + (s[0] == '-'));
    free(s);
    return n;
}
/* ordre decimal : x = c * 10^e avec 10^ord <= |x| < 10^(ord+1) */
static int64_t bm_order10(const ClairBigFloat *x) {
    return x->exp10 + (int64_t)bm_digits(&x->coefficient) - 1;
}
static size_t bm_bits_for(uint32_t precision) {
    return (size_t)ceil(((double)precision + 4.0) * 3.3219280948873623) + 24;
}

/* ------------------------------------------------------------------ */
/* Racine entiere, pgcd, puissance modulaire                           */
/* ------------------------------------------------------------------ */

int clair_big_isqrt(BmInt *out, const BmInt *n) {
    if (n->sign < 0) return CLAIR_BIG_DOMAIN;
    if (!n->sign) return clair_big_from_i64(out, 0);
    BmInt x, y, q, r, sum; clair_big_init(&x); clair_big_init(&y); clair_big_init(&q);
    clair_big_init(&r); clair_big_init(&sum);
    int rc;
    size_t bits = clair_big_bit_length(n);
    BM_TRY(bm_one(&x, (bits + 1) / 2));                   /* 2^ceil(bits/2) >= racine */
    for (;;) {
        BM_TRY(clair_big_divmod(&q, &r, n, &x));
        BM_TRY(clair_big_add(&sum, &x, &q));
        BM_TRY(bm_shr(&y, &sum, 1));
        if (clair_big_cmp(&y, &x) >= 0) break;            /* Newton par le haut : arret au minimum */
        BM_TRY(clair_big_copy(&x, &y));
    }
    BM_TRY(clair_big_copy(out, &x));
end:
    clair_big_clear(&x); clair_big_clear(&y); clair_big_clear(&q); clair_big_clear(&r); clair_big_clear(&sum);
    return rc;
}

int clair_big_gcd(BmInt *out, const BmInt *a, const BmInt *b) {
    BmInt x, y, q, r; clair_big_init(&x); clair_big_init(&y); clair_big_init(&q); clair_big_init(&r);
    int rc;
    BM_TRY(clair_big_copy(&x, a)); BM_TRY(clair_big_copy(&y, b));
    if (x.sign < 0) x.sign = 1;
    if (y.sign < 0) y.sign = 1;
    while (y.sign) {
        BM_TRY(clair_big_divmod(&q, &r, &x, &y));
        BM_TRY(clair_big_copy(&x, &y));
        BM_TRY(clair_big_copy(&y, &r));
    }
    BM_TRY(clair_big_copy(out, &x));
end:
    clair_big_clear(&x); clair_big_clear(&y); clair_big_clear(&q); clair_big_clear(&r);
    return rc;
}

/* r = a mod m (m > 0), entre 0 et m - 1 */
static int bm_mod_pos(BmInt *r, const BmInt *a, const BmInt *m) {
    BmInt q; clair_big_init(&q);
    int rc = clair_big_divmod(&q, r, a, m);
    if (!rc && r->sign < 0) rc = clair_big_add(r, r, m);
    clair_big_clear(&q);
    return rc;
}

/* pow(b, e, m) comme Python : resultat du signe de m */
int clair_big_powmod(BmInt *out, const BmInt *b, const BmInt *e, const BmInt *m) {
    if (!m->sign) return CLAIR_BIG_DIVZERO;
    if (e->sign < 0) return CLAIR_BIG_DOMAIN;
    BmInt am, base, result, t, q; clair_big_init(&am); clair_big_init(&base); clair_big_init(&result);
    clair_big_init(&t); clair_big_init(&q);
    int rc;
    BM_TRY(clair_big_copy(&am, m));
    am.sign = 1;
    BM_TRY(bm_mod_pos(&base, b, &am));
    BM_TRY(clair_big_from_i64(&t, 1));
    BM_TRY(bm_mod_pos(&result, &t, &am));                 /* 1 mod 1 = 0 */
    size_t bits = clair_big_bit_length(e);
    for (size_t i = bits; i > 0; i--) {
        BM_TRY(clair_big_mul(&t, &result, &result));
        BM_TRY(bm_mod_pos(&result, &t, &am));
        if (clair_big_get_bit(e, i - 1)) {
            BM_TRY(clair_big_mul(&t, &result, &base));
            BM_TRY(bm_mod_pos(&result, &t, &am));
        }
    }
    if (m->sign < 0 && result.sign) BM_TRY(clair_big_sub(&result, &result, &am));
    BM_TRY(clair_big_copy(out, &result));
end:
    clair_big_clear(&am); clair_big_clear(&base); clair_big_clear(&result); clair_big_clear(&t); clair_big_clear(&q);
    return rc;
}

int clair_big_to_double(const BmInt *n, double *out) {
    char *s = NULL;
    int rc = clair_big_to_decimal(n, &s);
    if (rc) return rc;
    *out = strtod(s, NULL);                               /* au plus proche ; trop grand : inf */
    free(s);
    return 0;
}

int clair_big_to_i64(const BmInt *n, int64_t *out) {
    if (!n->sign) { *out = 0; return 0; }
    if (n->len > 1) return CLAIR_BIG_TOO_LARGE;
    uint64_t w = n->word[0];
    if (n->sign > 0) { if (w > (uint64_t)INT64_MAX) return CLAIR_BIG_TOO_LARGE; *out = (int64_t)w; }
    else { if (w > (uint64_t)INT64_MAX + 1u) return CLAIR_BIG_TOO_LARGE; *out = w == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)w; }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Passage decimal <-> virgule fixe binaire                           */
/* ------------------------------------------------------------------ */

/* X = x * 2^B (vers zero) */
static int bm_from_bf(BmInt *X, const ClairBigFloat *x, size_t B) {
    if (!x->coefficient.sign) return clair_big_from_i64(X, 0);
    int64_t e = x->exp10;
    BmInt p, t; clair_big_init(&p); clair_big_init(&t);
    int rc;
    if (e >= 0) {
        if (e > 2000000) { rc = CLAIR_BIG_TOO_LARGE; goto end; }
        BM_TRY(bm_pow10(&p, (uint64_t)e));
        BM_TRY(clair_big_mul(&t, &x->coefficient, &p));
        BM_TRY(bm_shl(X, &t, B));
    } else {
        /* trop petit pour B bits : 0 */
        if ((double)(-e) > (double)B * 0.30103 + (double)bm_digits(&x->coefficient) + 10.0) {
            rc = clair_big_from_i64(X, 0);
            goto end;
        }
        BM_TRY(bm_pow10(&p, (uint64_t)(-e)));
        BM_TRY(bm_shl(&t, &x->coefficient, B));
        BM_TRY(clair_big_divmod(X, &p, &t, &p));
    }
    rc = 0;
end:
    clair_big_clear(&p); clair_big_clear(&t);
    return rc;
}

/* out = X * 2^e2, arrondi a 'precision' chiffres (le calcul n'est pas exact : sticky) */
static int bm_to_bf(ClairBigFloat *out, const BmInt *X, int64_t e2, uint32_t precision) {
    if (!X->sign) {
        BmInt zero; clair_big_init(&zero);
        int rc = clair_bf_from_scaled(out, &zero, 0, precision, 0);
        clair_big_clear(&zero);
        return rc;
    }
    size_t bits = clair_big_bit_length(X);
    int64_t D = (int64_t)floor(((double)bits - 1.0 + (double)e2) * 0.30102999566398120);
    int64_t m = (int64_t)precision + 6 - 1 - D;          /* T = valeur * 10^m : precision + 6 chiffres */
    BmInt T, p, d; clair_big_init(&T); clair_big_init(&p); clair_big_init(&d);
    int rc;
    if (m >= 0) {
        BM_TRY(bm_pow10(&p, (uint64_t)m));
        BM_TRY(clair_big_mul(&d, X, &p));
        BM_TRY(bm_shift(&T, &d, e2));
    } else {
        BM_TRY(bm_pow10(&p, (uint64_t)(-m)));
        if (e2 >= 0) {
            BM_TRY(bm_shl(&d, X, (size_t)e2));
            BM_TRY(clair_big_divmod(&T, &p, &d, &p));
        } else {
            BM_TRY(bm_shl(&d, &p, (size_t)(-e2)));
            BM_TRY(clair_big_divmod(&T, &p, X, &d));
        }
    }
    BM_TRY(clair_bf_from_scaled(out, &T, -m, precision, 1));
end:
    clair_big_clear(&T); clair_big_clear(&p); clair_big_clear(&d);
    return rc;
}

/* chiffres significatifs disponibles dans un resultat en virgule fixe, moins les bits douteux */
static int bm_enough(const BmInt *R, size_t want_bits, size_t doubtful) {
    size_t bits = clair_big_bit_length(R);
    return bits > doubtful && bits - doubtful >= want_bits;
}

/* ------------------------------------------------------------------ */
/* Constantes : pi et ln 2, gardees pour le plus grand B deja calcule  */
/* ------------------------------------------------------------------ */

/* atan(1/q) ou atanh(1/q) (hyperbolic) a l'echelle 2^B */
static int bm_atan_inv(BmInt *out, int64_t q, size_t B, int hyperbolic) {
    BmInt t, term, sum; clair_big_init(&t); clair_big_init(&term); clair_big_init(&sum);
    int rc;
    BM_TRY(bm_one(&t, B));
    BM_TRY(bm_divi(&t, &t, q));
    BM_TRY(clair_big_copy(&sum, &t));
    for (int64_t k = 1;; k++) {
        BM_TRY(bm_divi(&t, &t, q * q));
        if (bm_is_zero(&t)) break;
        BM_TRY(bm_divi(&term, &t, 2 * k + 1));
        if (!hyperbolic && (k & 1)) BM_TRY(clair_big_sub(&sum, &sum, &term));
        else BM_TRY(clair_big_add(&sum, &sum, &term));
    }
    BM_TRY(clair_big_copy(out, &sum));
end:
    clair_big_clear(&t); clair_big_clear(&term); clair_big_clear(&sum);
    return rc;
}

static int bm_compute_pi(BmInt *out, size_t B) {           /* Machin : 16 atan(1/5) - 4 atan(1/239) */
    BmInt a, b; clair_big_init(&a); clair_big_init(&b);
    int rc;
    BM_TRY(bm_atan_inv(&a, 5, B + 16, 0));
    BM_TRY(bm_atan_inv(&b, 239, B + 16, 0));
    BM_TRY(bm_muli(&a, &a, 16));
    BM_TRY(bm_muli(&b, &b, 4));
    BM_TRY(clair_big_sub(&a, &a, &b));
    BM_TRY(bm_shr(out, &a, 16));
end:
    clair_big_clear(&a); clair_big_clear(&b);
    return rc;
}
static int bm_compute_ln2(BmInt *out, size_t B) {          /* 2 atanh(1/3) */
    BmInt a; clair_big_init(&a);
    int rc;
    BM_TRY(bm_atan_inv(&a, 3, B + 16, 1));
    BM_TRY(bm_shr(out, &a, 15));
end:
    clair_big_clear(&a);
    return rc;
}
static BmInt bm_pi_cache, bm_ln2_cache;
static size_t bm_pi_bits, bm_ln2_bits;
static int bm_constant(BmInt *out, size_t B, BmInt *cache, size_t *cache_bits, int (*compute)(BmInt *, size_t)) {
    if (*cache_bits < B) {
        BmInt t; clair_big_init(&t);
        int rc = compute(&t, B + 64);
        if (rc) { clair_big_clear(&t); return rc; }
        clair_big_clear(cache);
        *cache = t;
        *cache_bits = B + 64;
    }
    return bm_shr(out, cache, *cache_bits - B);
}
static int bm_pi(BmInt *out, size_t B) { return bm_constant(out, B, &bm_pi_cache, &bm_pi_bits, bm_compute_pi); }
static int bm_ln10(BmInt *out, size_t B);
static int bm_ln2(BmInt *out, size_t B) { return bm_constant(out, B, &bm_ln2_cache, &bm_ln2_bits, bm_compute_ln2); }

/* ------------------------------------------------------------------ */
/* Series en virgule fixe                                             */
/* ------------------------------------------------------------------ */

/* exp(R), |R| <= 1 : R / 2^s, serie de Taylor, puis s mises au carre */
static int bm_exp_core(BmInt *out, const BmInt *R, size_t B) {
    size_t s = 8 + (size_t)sqrt((double)B) / 2;
    size_t W = B + s + 24;
    BmInt r, sum, term, one; clair_big_init(&r); clair_big_init(&sum); clair_big_init(&term); clair_big_init(&one);
    int rc;
    BM_TRY(bm_shift(&r, R, (int64_t)(W - B) - (int64_t)s));
    BM_TRY(bm_one(&one, W));
    BM_TRY(clair_big_copy(&sum, &one));
    BM_TRY(clair_big_copy(&term, &one));
    for (int64_t k = 1;; k++) {
        BM_TRY(bm_mul(&term, &term, &r, W));
        BM_TRY(bm_divi(&term, &term, k));
        if (bm_is_zero(&term)) break;
        BM_TRY(clair_big_add(&sum, &sum, &term));
    }
    for (size_t i = 0; i < s; i++) BM_TRY(bm_mul(&sum, &sum, &sum, W));
    BM_TRY(bm_shr(out, &sum, W - B));
end:
    clair_big_clear(&r); clair_big_clear(&sum); clair_big_clear(&term); clair_big_clear(&one);
    return rc;
}

/* log(X / 2^B) pour X > 0, a l'echelle 2^B : X = m 2^k, m dans [0.75, 1.5), 2 atanh((m-1)/(m+1)) */
static int bm_log_core(BmInt *out, const BmInt *X, size_t B) {
    BmInt m, one, z, z2, term, sum, t, l2; clair_big_init(&m); clair_big_init(&one); clair_big_init(&z);
    clair_big_init(&z2); clair_big_init(&term); clair_big_init(&sum); clair_big_init(&t); clair_big_init(&l2);
    int rc;
    int64_t k = (int64_t)clair_big_bit_length(X) - (int64_t)B - 1;
    BM_TRY(bm_shift(&m, X, -k));                          /* m dans [1, 2) */
    BM_TRY(bm_one(&one, B));
    BM_TRY(bm_muli(&t, &one, 3));
    BM_TRY(bm_shr(&t, &t, 1));                            /* 1.5 */
    if (clair_big_cmp(&m, &t) > 0) { BM_TRY(bm_shr(&m, &m, 1)); k++; }
    BM_TRY(clair_big_sub(&t, &m, &one));
    BM_TRY(clair_big_add(&term, &m, &one));
    BM_TRY(bm_div(&z, &t, &term, B));
    BM_TRY(bm_mul(&z2, &z, &z, B));
    BM_TRY(clair_big_copy(&term, &z));
    BM_TRY(clair_big_copy(&sum, &z));
    for (int64_t j = 1;; j++) {
        BM_TRY(bm_mul(&term, &term, &z2, B));
        if (bm_is_zero(&term)) break;
        BM_TRY(bm_divi(&t, &term, 2 * j + 1));
        BM_TRY(clair_big_add(&sum, &sum, &t));
    }
    BM_TRY(bm_muli(&sum, &sum, 2));
    if (k) {
        BM_TRY(bm_ln2(&l2, B));
        BM_TRY(bm_muli(&t, &l2, k));
        BM_TRY(clair_big_add(&sum, &sum, &t));
    }
    BM_TRY(clair_big_copy(out, &sum));
end:
    clair_big_clear(&m); clair_big_clear(&one); clair_big_clear(&z); clair_big_clear(&z2); clair_big_clear(&term);
    clair_big_clear(&sum); clair_big_clear(&t); clair_big_clear(&l2);
    return rc;
}

static BmInt bm_ln10_cache;
static size_t bm_ln10_bits;
static int bm_compute_ln10(BmInt *out, size_t B) {
    BmInt t; clair_big_init(&t);
    int rc = clair_big_from_i64(&t, 10);
    if (!rc) rc = bm_shl(&t, &t, B);
    if (!rc) rc = bm_log_core(out, &t, B);
    clair_big_clear(&t);
    return rc;
}
static int bm_ln10(BmInt *out, size_t B) { return bm_constant(out, B, &bm_ln10_cache, &bm_ln10_bits, bm_compute_ln10); }

/* sin et cos de R, |R| <= 1 : R / 2^s, series, puis s doublements */
static int bm_sincos_core(BmInt *S, BmInt *C, const BmInt *R, size_t B) {
    size_t s = 6 + (size_t)sqrt((double)B) / 3;
    size_t W = B + 2 * s + 24;
    BmInt r, r2, st, ct, ss, cc, t, one; clair_big_init(&r); clair_big_init(&r2); clair_big_init(&st);
    clair_big_init(&ct); clair_big_init(&ss); clair_big_init(&cc); clair_big_init(&t); clair_big_init(&one);
    int rc;
    BM_TRY(bm_shift(&r, R, (int64_t)(W - B) - (int64_t)s));
    BM_TRY(bm_one(&one, W));
    BM_TRY(bm_mul(&r2, &r, &r, W));
    BM_TRY(clair_big_copy(&st, &r)); BM_TRY(clair_big_copy(&ss, &r));
    BM_TRY(clair_big_copy(&ct, &one)); BM_TRY(clair_big_copy(&cc, &one));
    for (int64_t k = 1;; k++) {
        BM_TRY(bm_mul(&st, &st, &r2, W));
        BM_TRY(bm_divi(&st, &st, -(2 * k) * (2 * k + 1)));
        BM_TRY(bm_mul(&ct, &ct, &r2, W));
        BM_TRY(bm_divi(&ct, &ct, -(2 * k - 1) * (2 * k)));
        if (bm_is_zero(&st) && bm_is_zero(&ct)) break;
        BM_TRY(clair_big_add(&ss, &ss, &st));
        BM_TRY(clair_big_add(&cc, &cc, &ct));
    }
    for (size_t i = 0; i < s; i++) {                      /* sin 2a = 2 sin a cos a ; cos 2a = cos^2 - sin^2 */
        BM_TRY(bm_mul(&t, &ss, &cc, W));
        BM_TRY(bm_muli(&t, &t, 2));
        BM_TRY(bm_mul(&r2, &cc, &cc, W));
        BM_TRY(bm_mul(&r, &ss, &ss, W));
        BM_TRY(clair_big_sub(&cc, &r2, &r));
        BM_TRY(clair_big_copy(&ss, &t));
    }
    if (S) BM_TRY(bm_shr(S, &ss, W - B));
    if (C) BM_TRY(bm_shr(C, &cc, W - B));
end:
    clair_big_clear(&r); clair_big_clear(&r2); clair_big_clear(&st); clair_big_clear(&ct);
    clair_big_clear(&ss); clair_big_clear(&cc); clair_big_clear(&t); clair_big_clear(&one);
    return rc;
}

/* sin et cos de X / 2^B, X quelconque : X - q pi/2, puis le quart de tour */
static int bm_sincos(BmInt *S, BmInt *C, const BmInt *X, size_t B) {
    BmInt half_pi, q, r, t, s0, c0; clair_big_init(&half_pi); clair_big_init(&q); clair_big_init(&r);
    clair_big_init(&t); clair_big_init(&s0); clair_big_init(&c0);
    int rc;
    BM_TRY(bm_pi(&half_pi, B - 1));                       /* pi a l'echelle B - 1 : pi / 2 a l'echelle B */
    BM_TRY(bm_shr(&t, &half_pi, 1));                      /* pi / 4 */
    if (X->sign >= 0) BM_TRY(clair_big_add(&r, X, &t)); else BM_TRY(clair_big_sub(&r, X, &t));
    BM_TRY(clair_big_divmod(&q, &t, &r, &half_pi));       /* q = arrondi de X / (pi / 2) */
    BM_TRY(clair_big_mul(&t, &q, &half_pi));
    BM_TRY(clair_big_sub(&r, X, &t));
    int quarter = q.sign ? (int)(q.word[0] & 3u) : 0;
    if (q.sign < 0) quarter = (4 - quarter) & 3;
    BM_TRY(bm_sincos_core(&s0, &c0, &r, B));
    /* sin(r + q pi/2), cos(r + q pi/2) */
    BmInt *sin_src = (quarter & 1) ? &c0 : &s0, *cos_src = (quarter & 1) ? &s0 : &c0;
    int sin_neg = quarter >= 2, cos_neg = quarter == 1 || quarter == 2;
    if (S) { BM_TRY(clair_big_copy(S, sin_src)); if (sin_neg) S->sign = -S->sign; }
    if (C) { BM_TRY(clair_big_copy(C, cos_src)); if (cos_neg) C->sign = -C->sign; }
end:
    clair_big_clear(&half_pi); clair_big_clear(&q); clair_big_clear(&r); clair_big_clear(&t);
    clair_big_clear(&s0); clair_big_clear(&c0);
    return rc;
}

/* atan(T / 2^B), |T| <= 1 : demi-angles jusqu'a |T| < 2^-8, puis serie */
static int bm_atan_core(BmInt *out, const BmInt *T, size_t B) {
    size_t W = B + 32;
    BmInt t, one, t2, root, d, term, sum; clair_big_init(&t); clair_big_init(&one); clair_big_init(&t2);
    clair_big_init(&root); clair_big_init(&d); clair_big_init(&term); clair_big_init(&sum);
    int rc;
    BM_TRY(bm_shl(&t, T, W - B));
    BM_TRY(bm_one(&one, W));
    size_t halvings = 0;
    while (t.sign && clair_big_bit_length(&t) > W - 8) {  /* atan t = 2 atan(t / (1 + sqrt(1 + t^2))) */
        BM_TRY(bm_mul(&t2, &t, &t, W));
        BM_TRY(clair_big_add(&t2, &t2, &one));
        BM_TRY(bm_shl(&d, &t2, W));
        BM_TRY(clair_big_isqrt(&root, &d));
        BM_TRY(clair_big_add(&root, &root, &one));
        BM_TRY(bm_div(&t, &t, &root, W));
        halvings++;
    }
    BM_TRY(bm_mul(&t2, &t, &t, W));
    BM_TRY(clair_big_copy(&term, &t));
    BM_TRY(clair_big_copy(&sum, &t));
    for (int64_t j = 1;; j++) {
        BM_TRY(bm_mul(&term, &term, &t2, W));
        if (bm_is_zero(&term)) break;
        BM_TRY(bm_divi(&d, &term, 2 * j + 1));
        if (j & 1) BM_TRY(clair_big_sub(&sum, &sum, &d)); else BM_TRY(clair_big_add(&sum, &sum, &d));
    }
    BM_TRY(bm_shift(out, &sum, (int64_t)halvings - (int64_t)(W - B)));
end:
    clair_big_clear(&t); clair_big_clear(&one); clair_big_clear(&t2); clair_big_clear(&root);
    clair_big_clear(&d); clair_big_clear(&term); clair_big_clear(&sum);
    return rc;
}

/* atan(X / 2^B), X quelconque */
static int bm_atan(BmInt *out, const BmInt *X, size_t B) {
    BmInt one, inv, a, half_pi; clair_big_init(&one); clair_big_init(&inv); clair_big_init(&a); clair_big_init(&half_pi);
    int rc;
    BM_TRY(bm_one(&one, B));
    BmInt ax = *X;
    ax.sign = ax.sign ? 1 : 0;
    if (clair_big_cmp(&ax, &one) <= 0) {
        BM_TRY(bm_atan_core(out, X, B));
    } else {                                              /* atan x = +-pi/2 - atan(1/x) */
        BM_TRY(bm_div(&inv, &one, X, B));
        BM_TRY(bm_atan_core(&a, &inv, B));
        BM_TRY(bm_pi(&half_pi, B - 1));                   /* pi / 2 */
        if (X->sign < 0) half_pi.sign = -half_pi.sign;
        BM_TRY(clair_big_sub(out, &half_pi, &a));
    }
end:
    clair_big_clear(&one); clair_big_clear(&inv); clair_big_clear(&a); clair_big_clear(&half_pi);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Fonctions bigfloat                                                 */
/* ------------------------------------------------------------------ */

static int bm_zero(ClairBigFloat *out) {
    BmInt z; clair_big_init(&z);
    int rc = clair_bf_from_scaled(out, &z, 0, 1, 0);
    clair_big_clear(&z);
    return rc;
}
static int bm_int_bf(ClairBigFloat *out, int64_t v, uint32_t precision) {
    BmInt z; clair_big_init(&z);
    int rc = clair_big_from_i64(&z, v);
    if (!rc) rc = clair_bf_from_scaled(out, &z, 0, precision, 0);
    clair_big_clear(&z);
    return rc;
}
/* bits de plus pour un argument petit (f(x) ~ x) : ses zeros de tete */
static size_t bm_small_bits(const ClairBigFloat *x) {
    int64_t o = bm_order10(x);
    return o < 0 ? (size_t)((double)(-o) * 3.33) + 8 : 0;
}
static size_t bm_big_bits(const ClairBigFloat *x) {
    int64_t o = bm_order10(x);
    return o > 0 ? (size_t)((double)o * 3.33) + 8 : 0;
}
#define BM_MAX_RETRY_BITS(B0) ((B0) * 8 + 200000)

int clair_bf_sqrt(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision) {
    if (x->coefficient.sign < 0) return CLAIR_BIG_DOMAIN;
    if (!x->coefficient.sign) return bm_zero(out);
    /* c 10^e = (c 10^k) 10^(e-k), e - k pair, c 10^k avec au moins 2 (precision + 3) chiffres */
    int64_t d = (int64_t)bm_digits(&x->coefficient);
    int64_t k = 2 * ((int64_t)precision + 3) - d;
    if (k < 0) k = 0;
    if ((x->exp10 - k) % 2 != 0) k++;
    BmInt n, p, s, sq; clair_big_init(&n); clair_big_init(&p); clair_big_init(&s); clair_big_init(&sq);
    int rc;
    BM_TRY(bm_pow10(&p, (uint64_t)k));
    BM_TRY(clair_big_mul(&n, &x->coefficient, &p));
    BM_TRY(clair_big_isqrt(&s, &n));
    BM_TRY(clair_big_mul(&sq, &s, &s));
    BM_TRY(clair_bf_from_scaled(out, &s, (x->exp10 - k) / 2, precision, clair_big_cmp(&sq, &n) != 0));
end:
    clair_big_clear(&n); clair_big_clear(&p); clair_big_clear(&s); clair_big_clear(&sq);
    return rc;
}

int clair_bf_exp(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision) {
    if (!x->coefficient.sign) return bm_int_bf(out, 1, precision);
    if (bm_order10(x) > 15) return CLAIR_BIG_TOO_LARGE;
    /* exp x = 10^K exp(x - K ln 10), K = arrondi de x / ln 10 : aucune grande puissance a calculer */
    size_t B = bm_bits_for(precision) + bm_big_bits(x) + 32;
    BmInt X, l10, n, r, t, E; clair_big_init(&X); clair_big_init(&l10); clair_big_init(&n);
    clair_big_init(&r); clair_big_init(&t); clair_big_init(&E);
    int rc;
    BM_TRY(bm_from_bf(&X, x, B));
    BM_TRY(bm_ln10(&l10, B));
    BM_TRY(bm_shr(&t, &l10, 1));
    if (X.sign >= 0) BM_TRY(clair_big_add(&r, &X, &t)); else BM_TRY(clair_big_sub(&r, &X, &t));
    BM_TRY(clair_big_divmod(&n, &t, &r, &l10));
    int64_t K;
    BM_TRY(clair_big_to_i64(&n, &K));
    BM_TRY(clair_big_mul(&t, &n, &l10));
    BM_TRY(clair_big_sub(&r, &X, &t));                    /* |r| <= ln 10 / 2 */
    BM_TRY(bm_exp_core(&E, &r, B));
    BM_TRY(bm_to_bf(out, &E, -(int64_t)B, precision));
    if (out->coefficient.sign && __builtin_add_overflow(out->exp10, K, &out->exp10)) { rc = CLAIR_BIG_TOO_LARGE; goto end; }
end:
    clair_big_clear(&X); clair_big_clear(&l10); clair_big_clear(&n); clair_big_clear(&r);
    clair_big_clear(&t); clair_big_clear(&E);
    return rc;
}

/* log(x) a l'echelle 2^B : x = m 10^E, m dans [1, 10) ; log m + E log 10 */
static int bm_log_fixed(BmInt *out, const ClairBigFloat *x, size_t B) {
    BmInt X, L, t, ten, l10; clair_big_init(&X); clair_big_init(&L); clair_big_init(&t);
    clair_big_init(&ten); clair_big_init(&l10);
    int rc;
    int64_t d = (int64_t)bm_digits(&x->coefficient);
    int64_t E = x->exp10 + d - 1;
    ClairBigFloat m = *x;
    m.exp10 = -(d - 1);                                    /* meme coefficient : m dans [1, 10) */
    BM_TRY(bm_from_bf(&X, &m, B));
    BM_TRY(bm_log_core(&L, &X, B));
    if (E) {
        BM_TRY(bm_ln10(&l10, B));
        BM_TRY(bm_muli(&t, &l10, E));
        BM_TRY(clair_big_add(&L, &L, &t));
    }
    BM_TRY(clair_big_copy(out, &L));
end:
    clair_big_clear(&X); clair_big_clear(&L); clair_big_clear(&t); clair_big_clear(&ten); clair_big_clear(&l10);
    return rc;
}

/* log, log10 (base 10), log2 (base 2) */
static int bm_log_base(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision, int base) {
    if (x->coefficient.sign <= 0) return CLAIR_BIG_DOMAIN;
    if (x->exp10 == 0 && x->coefficient.len == 1 && x->coefficient.word[0] == 1) return bm_zero(out);  /* log 1 = 0 */
    size_t want = bm_bits_for(precision);
    size_t B0 = want + 64 + (size_t)(log2((double)labs((long)bm_order10(x)) + 2.0)) + 8;
    BmInt L, den, t; clair_big_init(&L); clair_big_init(&den); clair_big_init(&t);
    int rc = 0;
    for (size_t B = B0;; ) {
        BM_TRY(bm_log_fixed(&L, x, B));
        if (base == 10) {
            BM_TRY(bm_ln10(&den, B));
            BM_TRY(bm_div(&L, &L, &den, B));
        } else if (base == 2) {
            BM_TRY(bm_ln2(&den, B));
            BM_TRY(bm_div(&L, &L, &den, B));
        }
        if (bm_enough(&L, want + 8, 24) || B > BM_MAX_RETRY_BITS(B0)) {
            BM_TRY(bm_to_bf(out, &L, -(int64_t)B, precision));
            break;
        }
        B += want + 64 + (L.sign ? 0 : B);
    }
end:
    clair_big_clear(&L); clair_big_clear(&den); clair_big_clear(&t);
    return rc;
}
int clair_bf_log(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_log_base(o, x, p, 0); }
int clair_bf_log10(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_log_base(o, x, p, 10); }
int clair_bf_log2(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_log_base(o, x, p, 2); }

/* sin (which 0), cos (1), tan (2) */
static int bm_trig(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision, int which) {
    if (!x->coefficient.sign) return which == 1 ? bm_int_bf(out, 1, precision) : bm_zero(out);
    if (bm_order10(x) > 100000) return CLAIR_BIG_TOO_LARGE;
    size_t want = bm_bits_for(precision);
    size_t B0 = want + 40 + bm_small_bits(x) + bm_big_bits(x);
    BmInt X, S, C, R; clair_big_init(&X); clair_big_init(&S); clair_big_init(&C); clair_big_init(&R);
    int rc = 0;
    for (size_t B = B0;; ) {
        BM_TRY(bm_from_bf(&X, x, B));
        BM_TRY(bm_sincos(&S, &C, &X, B));
        int ok;
        if (which == 0) { BM_TRY(clair_big_copy(&R, &S)); ok = bm_enough(&S, want + 8, 24); }
        else if (which == 1) { BM_TRY(clair_big_copy(&R, &C)); ok = bm_enough(&C, want + 8, 24); }
        else {
            ok = bm_enough(&S, want + 8, 24) && bm_enough(&C, want + 8, 24);
            if (C.sign) BM_TRY(bm_div(&R, &S, &C, B));
        }
        if (ok || B > BM_MAX_RETRY_BITS(B0)) {
            if (which == 2 && !C.sign) { rc = CLAIR_BIG_TOO_LARGE; goto end; }
            BM_TRY(bm_to_bf(out, &R, -(int64_t)B, precision));
            break;
        }
        B += want + 64;
    }
end:
    clair_big_clear(&X); clair_big_clear(&S); clair_big_clear(&C); clair_big_clear(&R);
    return rc;
}
int clair_bf_sin(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_trig(o, x, p, 0); }
int clair_bf_cos(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_trig(o, x, p, 1); }
int clair_bf_tan(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_trig(o, x, p, 2); }

int clair_bf_atan(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision) {
    if (!x->coefficient.sign) return bm_zero(out);
    if (bm_order10(x) > 1000000) {                         /* atan x = +-pi/2 a toute precision raisonnable */
        BmInt P; clair_big_init(&P);
        size_t B = bm_bits_for(precision) + 40;
        int rc = bm_pi(&P, B - 1);                        /* pi / 2 */
        if (!rc) { if (x->coefficient.sign < 0) P.sign = -P.sign; rc = bm_to_bf(out, &P, -(int64_t)B, precision); }
        clair_big_clear(&P);
        return rc;
    }
    size_t want = bm_bits_for(precision);
    size_t B0 = want + 40 + bm_small_bits(x) + bm_big_bits(x);
    BmInt X, A; clair_big_init(&X); clair_big_init(&A);
    int rc = 0;
    for (size_t B = B0;; ) {
        BM_TRY(bm_from_bf(&X, x, B));
        BM_TRY(bm_atan(&A, &X, B));
        if (bm_enough(&A, want + 8, 24) || B > BM_MAX_RETRY_BITS(B0)) {
            BM_TRY(bm_to_bf(out, &A, -(int64_t)B, precision));
            break;
        }
        B += want + 40;
    }
end:
    clair_big_clear(&X); clair_big_clear(&A);
    return rc;
}

int clair_bf_pi(ClairBigFloat *out, uint32_t precision) {
    if (!precision || precision > 1000000) return CLAIR_BIG_INVALID;
    size_t B = bm_bits_for(precision) + 40;
    BmInt P; clair_big_init(&P);
    int rc = bm_pi(&P, B);
    if (!rc) rc = bm_to_bf(out, &P, -(int64_t)B, precision);
    clair_big_clear(&P);
    return rc;
}

/* precision exacte pour une somme ou un produit : tous les chiffres de a et b */
static uint32_t bm_exact_precision(const ClairBigFloat *a, const ClairBigFloat *b, int product) {
    int64_t da = (int64_t)bm_digits(&a->coefficient), db = (int64_t)bm_digits(&b->coefficient);
    int64_t p;
    if (product) p = da + db + 2;
    else {
        int64_t hi_a = a->exp10 + da, hi_b = b->exp10 + db;
        int64_t hi = hi_a > hi_b ? hi_a : hi_b;
        int64_t lo = a->exp10 < b->exp10 ? a->exp10 : b->exp10;
        if (!a->coefficient.sign) lo = b->exp10, hi = hi_b;
        if (!b->coefficient.sign) lo = a->exp10, hi = hi_a;
        p = hi - lo + 2;
    }
    if (p < 1) p = 1;
    if (p > 1000000) p = 1000000;
    return (uint32_t)p;
}

int clair_bf_atan2(ClairBigFloat *out, const ClairBigFloat *y, const ClairBigFloat *x, uint32_t precision) {
    int sy = y->coefficient.sign, sx = x->coefficient.sign;
    ClairBigFloat q, a, pi; clair_bf_init(&q); clair_bf_init(&a); clair_bf_init(&pi);
    int rc;
    uint32_t p2 = precision + 20 > 1000000 ? 1000000 : precision + 20;
    if (!sx) {
        if (!sy) { rc = bm_zero(out); goto end; }
        BM_TRY(clair_bf_pi(&pi, p2));
        BmInt two; clair_big_init(&two);
        clair_big_from_i64(&two, sy > 0 ? 5 : -5);
        ClairBigFloat half; clair_bf_init(&half);
        rc = clair_bf_from_scaled(&half, &two, -1, 1, 0);  /* +-0.5 */
        if (!rc) rc = clair_bf_mul(out, &pi, &half, precision);
        clair_bf_clear(&half); clair_big_clear(&two);
        goto end;
    }
    BM_TRY(clair_bf_div(&q, y, x, p2));
    if (sx > 0) { BM_TRY(clair_bf_atan(out, &q, precision)); goto end; }
    BM_TRY(clair_bf_atan(&a, &q, p2));
    BM_TRY(clair_bf_pi(&pi, p2));
    if (sy >= 0) BM_TRY(clair_bf_add(out, &a, &pi, precision));
    else BM_TRY(clair_bf_sub(out, &a, &pi, precision));
end:
    clair_bf_clear(&q); clair_bf_clear(&a); clair_bf_clear(&pi);
    return rc;
}

/* 1 et une copie exacte de x : pour 1 - x, 1 + x, 1 - x^2 sans perte */
static int bm_one_bf(ClairBigFloat *out) { return bm_int_bf(out, 1, 1); }

int clair_bf_asin(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision) {
    if (!x->coefficient.sign) return bm_zero(out);
    ClairBigFloat one, x2, t, r, q; clair_bf_init(&one); clair_bf_init(&x2); clair_bf_init(&t);
    clair_bf_init(&r); clair_bf_init(&q);
    int rc;
    uint32_t p2 = precision + 20 > 1000000 ? 1000000 : precision + 20;
    BM_TRY(bm_one_bf(&one));
    BM_TRY(clair_bf_mul(&x2, x, x, bm_exact_precision(x, x, 1)));
    BM_TRY(clair_bf_sub(&t, &one, &x2, bm_exact_precision(&one, &x2, 0)));
    if (t.coefficient.sign < 0) { rc = CLAIR_BIG_DOMAIN; goto end; }
    if (!t.coefficient.sign) {                             /* asin(+-1) = +-pi/2 */
        BM_TRY(clair_bf_atan2(out, x, &t, precision));
        goto end;
    }
    BM_TRY(clair_bf_sqrt(&r, &t, p2));
    BM_TRY(clair_bf_div(&q, x, &r, p2));
    BM_TRY(clair_bf_atan(out, &q, precision));
end:
    clair_bf_clear(&one); clair_bf_clear(&x2); clair_bf_clear(&t); clair_bf_clear(&r); clair_bf_clear(&q);
    return rc;
}

int clair_bf_acos(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision) {
    ClairBigFloat one, a, b, q, r, t, two; clair_bf_init(&one); clair_bf_init(&a); clair_bf_init(&b);
    clair_bf_init(&q); clair_bf_init(&r); clair_bf_init(&t); clair_bf_init(&two);
    int rc;
    uint32_t p2 = precision + 20 > 1000000 ? 1000000 : precision + 20;
    BM_TRY(bm_one_bf(&one));
    BM_TRY(clair_bf_sub(&a, &one, x, bm_exact_precision(&one, x, 0)));   /* 1 - x */
    BM_TRY(clair_bf_add(&b, &one, x, bm_exact_precision(&one, x, 0)));   /* 1 + x */
    if (a.coefficient.sign < 0 || b.coefficient.sign < 0) { rc = CLAIR_BIG_DOMAIN; goto end; }
    if (!b.coefficient.sign) { BM_TRY(clair_bf_pi(out, precision)); goto end; }   /* acos(-1) = pi */
    if (!a.coefficient.sign) { BM_TRY(bm_zero(out)); goto end; }                  /* acos(1) = 0 */
    /* acos x = 2 atan(sqrt((1 - x) / (1 + x))) : pas de soustraction pres de 1 */
    BM_TRY(clair_bf_div(&q, &a, &b, p2));
    BM_TRY(clair_bf_sqrt(&r, &q, p2));
    BM_TRY(clair_bf_atan(&t, &r, p2));
    BM_TRY(bm_int_bf(&two, 2, 1));
    BM_TRY(clair_bf_mul(out, &t, &two, precision));
end:
    clair_bf_clear(&one); clair_bf_clear(&a); clair_bf_clear(&b); clair_bf_clear(&q);
    clair_bf_clear(&r); clair_bf_clear(&t); clair_bf_clear(&two);
    return rc;
}

/* sinh (0), cosh (1), tanh (2) par exp, avec des chiffres de plus pour un petit x */
static int bm_hyper(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision, int which) {
    if (!x->coefficient.sign) return which == 1 ? bm_int_bf(out, 1, precision) : bm_zero(out);
    int64_t o = bm_order10(x);
    if (which == 2 && o >= 1 && (double)o > log10((double)precision * 1.2 + 10.0)) {
        return bm_int_bf(out, x->coefficient.sign, precision);       /* tanh = +-1 a cette precision */
    }
    uint32_t extra = o < 0 ? (uint32_t)(-o) : 0;
    uint64_t p2l = (uint64_t)precision + 12 + extra;
    uint32_t p2 = p2l > 1000000 ? 1000000 : (uint32_t)p2l;
    ClairBigFloat e, inv, num, den, one, half; clair_bf_init(&e); clair_bf_init(&inv); clair_bf_init(&num);
    clair_bf_init(&den); clair_bf_init(&one); clair_bf_init(&half);
    int rc;
    BM_TRY(bm_one_bf(&one));
    BM_TRY(clair_bf_exp(&e, x, p2));
    BM_TRY(clair_bf_div(&inv, &one, &e, p2));
    if (which == 2) {
        BM_TRY(clair_bf_sub(&num, &e, &inv, p2));
        BM_TRY(clair_bf_add(&den, &e, &inv, p2));
        BM_TRY(clair_bf_div(out, &num, &den, precision));
    } else {
        if (which == 0) BM_TRY(clair_bf_sub(&num, &e, &inv, p2));
        else BM_TRY(clair_bf_add(&num, &e, &inv, p2));
        BmInt five; clair_big_init(&five);
        clair_big_from_i64(&five, 5);
        rc = clair_bf_from_scaled(&half, &five, -1, 1, 0);
        clair_big_clear(&five);
        if (rc) goto end;
        BM_TRY(clair_bf_mul(out, &num, &half, precision));
    }
end:
    clair_bf_clear(&e); clair_bf_clear(&inv); clair_bf_clear(&num); clair_bf_clear(&den);
    clair_bf_clear(&one); clair_bf_clear(&half);
    return rc;
}
int clair_bf_sinh(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_hyper(o, x, p, 0); }
int clair_bf_cosh(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_hyper(o, x, p, 1); }
int clair_bf_tanh(ClairBigFloat *o, const ClairBigFloat *x, uint32_t p) { return bm_hyper(o, x, p, 2); }

/* y entier (exposant decimal >= 0) et assez petit : sa valeur */
static int bm_integer_value(const ClairBigFloat *y, int64_t *out) {
    if (!y->coefficient.sign) { *out = 0; return 1; }
    if (y->exp10 < 0 || y->exp10 > 18) return 0;
    BmInt p, v; clair_big_init(&p); clair_big_init(&v);
    int ok = bm_pow10(&p, (uint64_t)y->exp10) == 0 && clair_big_mul(&v, &y->coefficient, &p) == 0 &&
             clair_big_to_i64(&v, out) == 0 && *out > -((int64_t)1 << 62) && *out < ((int64_t)1 << 62);
    clair_big_clear(&p); clair_big_clear(&v);
    return ok;
}

int clair_bf_pow(ClairBigFloat *out, const ClairBigFloat *x, const ClairBigFloat *y, uint32_t precision) {
    int64_t n;
    ClairBigFloat r, base, t, l; clair_bf_init(&r); clair_bf_init(&base); clair_bf_init(&t); clair_bf_init(&l);
    int rc;
    if (bm_integer_value(y, &n)) {
        /* puissance entiere : carres successifs, avec des chiffres de garde */
        if (!x->coefficient.sign) {
            if (n < 0) { rc = CLAIR_BIG_DIVZERO; goto end; }
            rc = n == 0 ? bm_int_bf(out, 1, precision) : bm_zero(out);
            goto end;
        }
        uint64_t m = n < 0 ? (uint64_t)(-(n + 1)) + 1u : (uint64_t)n;
        uint32_t guard = 4 + (uint32_t)log10((double)m + 1.0) * 2 + 4;
        uint32_t p2 = precision + guard > 1000000 ? 1000000 : precision + guard;
        BM_TRY(bm_int_bf(&r, 1, 1));
        BM_TRY(clair_bf_mul(&base, x, &r, p2));
        while (m) {
            if (m & 1) BM_TRY(clair_bf_mul(&r, &r, &base, p2));
            m >>= 1;
            if (m) BM_TRY(clair_bf_mul(&base, &base, &base, p2));
        }
        BM_TRY(bm_one_bf(&t));
        if (n < 0) BM_TRY(clair_bf_div(out, &t, &r, precision));
        else BM_TRY(clair_bf_mul(out, &r, &t, precision));
        goto end;
    }
    if (y->exp10 >= 0) {
        /* exposant entier trop grand pour les carres successifs : +-1 garde son signe, le reste deborde */
        int unit = x->exp10 == 0 && x->coefficient.len == 1 && x->coefficient.word[0] == 1;
        if (unit) {
            int odd = y->exp10 == 0 && (y->coefficient.word[0] & 1);
            rc = bm_int_bf(out, x->coefficient.sign < 0 && odd ? -1 : 1, precision);
            goto end;
        }
        if (x->coefficient.sign) { rc = CLAIR_BIG_TOO_LARGE; goto end; }
    }
    if (x->coefficient.sign < 0) { rc = CLAIR_BIG_DOMAIN; goto end; }
    if (!x->coefficient.sign) {
        if (y->coefficient.sign < 0) { rc = CLAIR_BIG_DIVZERO; goto end; }
        rc = bm_zero(out);
        goto end;
    }
    /* x^y = exp(y log x) ; log x avec assez de chiffres pour l'ordre de y log x */
    int64_t oy = bm_order10(y), ox = bm_order10(x);
    double mag = (double)(oy > 0 ? oy : 0) + log10(fabs((double)ox) * 2.31 + 1.0) + 2.0;
    uint64_t p2l = (uint64_t)precision + 12 + (uint64_t)mag;
    uint32_t p2 = p2l > 1000000 ? 1000000 : (uint32_t)p2l;
    BM_TRY(clair_bf_log(&l, x, p2));
    BM_TRY(clair_bf_mul(&t, &l, y, p2));
    BM_TRY(clair_bf_exp(out, &t, precision));
end:
    clair_bf_clear(&r); clair_bf_clear(&base); clair_bf_clear(&t); clair_bf_clear(&l);
    return rc;
}

int clair_bf_hypot(ClairBigFloat *out, const ClairBigFloat *x, const ClairBigFloat *y, uint32_t precision) {
    ClairBigFloat a, b, s; clair_bf_init(&a); clair_bf_init(&b); clair_bf_init(&s);
    int rc;
    BM_TRY(clair_bf_mul(&a, x, x, bm_exact_precision(x, x, 1)));
    BM_TRY(clair_bf_mul(&b, y, y, bm_exact_precision(y, y, 1)));
    BM_TRY(clair_bf_add(&s, &a, &b, bm_exact_precision(&a, &b, 0)));
    BM_TRY(clair_bf_sqrt(out, &s, precision));
end:
    clair_bf_clear(&a); clair_bf_clear(&b); clair_bf_clear(&s);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Conversions                                                        */
/* ------------------------------------------------------------------ */

int clair_bf_from_double(ClairBigFloat *out, double x) {
    if (x != x || x - x != 0) return CLAIR_BIG_INVALID;   /* nan, inf */
    if (x == 0) return bm_zero(out);
    int e2;
    double m = frexp(fabs(x), &e2);                       /* |x| = m 2^e2, m dans [0.5, 1) */
    uint64_t mant = (uint64_t)ldexp(m, 53);
    e2 -= 53;
    BmInt c, p; clair_big_init(&c); clair_big_init(&p);
    int rc;
    BM_TRY(clair_big_from_i64(&c, (int64_t)mant));
    int64_t exp10 = 0;
    if (e2 >= 0) BM_TRY(bm_shl(&c, &c, (size_t)e2));
    else {                                                 /* 2^-k = 5^k / 10^k : exact */
        BM_SMALL(five, 5);
        BM_TRY(clair_big_pow_u64(&p, &five, (uint64_t)(-e2)));
        BM_TRY(clair_big_mul(&c, &c, &p));
        exp10 = e2;
    }
    if (x < 0) c.sign = -c.sign;
    BM_TRY(clair_bf_from_scaled(out, &c, exp10, 1000000, 0));
end:
    clair_big_clear(&c); clair_big_clear(&p);
    return rc;
}

int clair_bf_from_bigint(ClairBigFloat *out, const BmInt *n) {
    return clair_bf_from_scaled(out, n, 0, 1000000, 0);
}

int clair_bf_to_double(const ClairBigFloat *x, double *out) {
    char *s = NULL;
    int rc = clair_bf_format(x, &s);
    if (rc) return rc;
    *out = strtod(s, NULL);
    free(s);
    return 0;
}

int clair_bf_floor(BmInt *out, const ClairBigFloat *x, int mode) {
    BmInt p, q, r; clair_big_init(&p); clair_big_init(&q); clair_big_init(&r);
    int rc;
    if (x->exp10 >= 0) {
        if (x->exp10 > 2000000) { rc = CLAIR_BIG_TOO_LARGE; goto end; }
        BM_TRY(bm_pow10(&p, (uint64_t)x->exp10));
        BM_TRY(clair_big_mul(out, &x->coefficient, &p));
        goto end;
    }
    if ((double)(-x->exp10) > (double)bm_digits(&x->coefficient) + 1.0) {
        BM_TRY(clair_big_from_i64(&q, 0));                 /* |x| < 1 */
        BM_TRY(clair_big_copy(&r, &x->coefficient));
    } else {
        BM_TRY(bm_pow10(&p, (uint64_t)(-x->exp10)));
        BM_TRY(clair_big_divmod(&q, &r, &x->coefficient, &p));   /* vers zero */
    }
    if (r.sign && mode < 0 && x->coefficient.sign < 0) { BM_SMALL(one, 1); BM_TRY(clair_big_sub(&q, &q, &one)); }
    if (r.sign && mode > 0 && x->coefficient.sign > 0) { BM_SMALL(one, 1); BM_TRY(clair_big_add(&q, &q, &one)); }
    BM_TRY(clair_big_copy(out, &q));
end:
    clair_big_clear(&p); clair_big_clear(&q); clair_big_clear(&r);
    return rc;
}
