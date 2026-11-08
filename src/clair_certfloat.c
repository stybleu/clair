#include "clair_certfloat.h"
#include "clair_bigmath.h"
#include <stdlib.h>
#include <string.h>

#define TRY(call) do { rc = (call); if (rc) goto end; } while (0)

static int cf_pow10(ClairBigInt *out, uint32_t p) {
    if (p > 1000) return CLAIR_BIG_TOO_LARGE;
    ClairBigInt ten;
    clair_big_init(&ten);
    int rc = clair_big_from_i64(&ten, 10);
    if (!rc) rc = clair_big_pow_u64(out, &ten, p);
    clair_big_clear(&ten);
    return rc;
}

void clair_cf_init(ClairCertFloat *x, uint32_t p) {
    clair_big_init(&x->lo);
    clair_big_init(&x->hi);
    x->places = p;
}

void clair_cf_clear(ClairCertFloat *x) {
    clair_big_clear(&x->lo);
    clair_big_clear(&x->hi);
}

static int compatible(
    const ClairCertFloat *o,
    const ClairCertFloat *a,
    const ClairCertFloat *b
) {
    return o->places <= 1000 &&
           o->places == a->places &&
           (!b || o->places == b->places);
}

static void swap_cf(ClairCertFloat *a, ClairCertFloat *b) {
    ClairCertFloat t = *a;
    *a = *b;
    *b = t;
}

static int div_out(
    ClairBigInt *out,
    const ClairBigInt *n,
    const ClairBigInt *d,
    int upper
) {
    ClairBigInt q, r, one;
    clair_big_init(&q);
    clair_big_init(&r);
    clair_big_init(&one);

    int rc = CLAIR_BIG_DIVZERO;
    if (!d->sign) goto end;

    TRY(clair_big_divmod(&q, &r, n, d));

    if (r.sign) {
        TRY(clair_big_from_i64(&one, 1));
        int positive = n->sign == d->sign;

        if (upper && positive)
            TRY(clair_big_add(&q, &q, &one));

        if (!upper && !positive)
            TRY(clair_big_sub(&q, &q, &one));
    }

    TRY(clair_big_copy(out, &q));

end:
    clair_big_clear(&q);
    clair_big_clear(&r);
    clair_big_clear(&one);
    return rc;
}

int clair_cf_parse(ClairCertFloat *out, const char *decimal) {
    if (!decimal || out->places > 1000)
        return CLAIR_BIG_INVALID;

    size_t len = strlen(decimal);
    if (!len || len > 2000)
        return CLAIR_BIG_INVALID;

    char *digits = malloc(len + 2);
    if (!digits) return CLAIR_BIG_NOMEM;

    size_t n = 0, fractional = 0;
    int dot = 0, sign = 0, count = 0;
    size_t i = 0;

    if (decimal[i] == '-' || decimal[i] == '+') {
        sign = decimal[i] == '-';
        i++;
    }

    int rc = CLAIR_BIG_INVALID;

    for (; i < len; i++) {
        char c = decimal[i];

        if (c == '.' && !dot) {
            dot = 1;
            continue;
        }

        if (c < '0' || c > '9') goto cleanup;

        digits[n++] = c;
        count++;
        if (dot) fractional++;
    }

    if (!count) goto cleanup;
    digits[n] = 0;

    ClairBigInt coeff, ten, scaled;
    clair_big_init(&coeff);
    clair_big_init(&ten);
    clair_big_init(&scaled);

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    if (sign) {
        memmove(digits + 1, digits, n + 1);
        digits[0] = '-';
    }

    TRY(clair_big_from_decimal(&coeff, digits));

    if (fractional <= out->places) {
        TRY(cf_pow10(&ten, out->places - (uint32_t)fractional));
        TRY(clair_big_mul(&scaled, &coeff, &ten));
        TRY(clair_big_copy(&tmp.lo, &scaled));
        TRY(clair_big_copy(&tmp.hi, &scaled));
    } else {
        if (fractional - out->places > 1000) {
            rc = CLAIR_BIG_TOO_LARGE;
            goto end;
        }

        TRY(cf_pow10(&ten, (uint32_t)(fractional - out->places)));
        TRY(div_out(&tmp.lo, &coeff, &ten, 0));
        TRY(div_out(&tmp.hi, &coeff, &ten, 1));
    }

    swap_cf(out, &tmp);

end:
    clair_big_clear(&coeff);
    clair_big_clear(&ten);
    clair_big_clear(&scaled);
    clair_cf_clear(&tmp);

cleanup:
    free(digits);
    return rc;
}

int clair_cf_add(
    ClairCertFloat *out,
    const ClairCertFloat *a,
    const ClairCertFloat *b
) {
    if (!compatible(out, a, b)) return CLAIR_BIG_INVALID;

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    int rc = clair_big_add(&tmp.lo, &a->lo, &b->lo);
    if (!rc) rc = clair_big_add(&tmp.hi, &a->hi, &b->hi);

    if (!rc) swap_cf(out, &tmp);
    clair_cf_clear(&tmp);
    return rc;
}

int clair_cf_sub(
    ClairCertFloat *out,
    const ClairCertFloat *a,
    const ClairCertFloat *b
) {
    if (!compatible(out, a, b)) return CLAIR_BIG_INVALID;

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    int rc = clair_big_sub(&tmp.lo, &a->lo, &b->hi);
    if (!rc) rc = clair_big_sub(&tmp.hi, &a->hi, &b->lo);

    if (!rc) swap_cf(out, &tmp);
    clair_cf_clear(&tmp);
    return rc;
}

int clair_cf_mul(
    ClairCertFloat *out,
    const ClairCertFloat *a,
    const ClairCertFloat *b
) {
    if (!compatible(out, a, b)) return CLAIR_BIG_INVALID;

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    ClairBigInt scale, prod, low, high;
    clair_big_init(&scale);
    clair_big_init(&prod);
    clair_big_init(&low);
    clair_big_init(&high);

    const ClairBigInt *u[2] = {&a->lo, &a->hi};
    const ClairBigInt *v[2] = {&b->lo, &b->hi};

    int rc;
    TRY(cf_pow10(&scale, out->places));

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            TRY(clair_big_mul(&prod, u[i], v[j]));
            TRY(div_out(&low, &prod, &scale, 0));
            TRY(div_out(&high, &prod, &scale, 1));

            if ((!i && !j) ||
                clair_big_cmp(&low, &tmp.lo) < 0)
                TRY(clair_big_copy(&tmp.lo, &low));

            if ((!i && !j) ||
                clair_big_cmp(&high, &tmp.hi) > 0)
                TRY(clair_big_copy(&tmp.hi, &high));
        }
    }

    swap_cf(out, &tmp);

end:
    clair_big_clear(&scale);
    clair_big_clear(&prod);
    clair_big_clear(&low);
    clair_big_clear(&high);
    clair_cf_clear(&tmp);
    return rc;
}

int clair_cf_div(
    ClairCertFloat *out,
    const ClairCertFloat *a,
    const ClairCertFloat *b
) {
    if (!compatible(out, a, b)) return CLAIR_BIG_INVALID;

    if (b->lo.sign <= 0 && b->hi.sign >= 0)
        return CLAIR_BIG_DIVZERO;

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    ClairBigInt scale, num, low, high;
    clair_big_init(&scale);
    clair_big_init(&num);
    clair_big_init(&low);
    clair_big_init(&high);

    const ClairBigInt *u[2] = {&a->lo, &a->hi};
    const ClairBigInt *v[2] = {&b->lo, &b->hi};

    int rc;
    TRY(cf_pow10(&scale, out->places));

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            TRY(clair_big_mul(&num, u[i], &scale));
            TRY(div_out(&low, &num, v[j], 0));
            TRY(div_out(&high, &num, v[j], 1));

            if ((!i && !j) ||
                clair_big_cmp(&low, &tmp.lo) < 0)
                TRY(clair_big_copy(&tmp.lo, &low));

            if ((!i && !j) ||
                clair_big_cmp(&high, &tmp.hi) > 0)
                TRY(clair_big_copy(&tmp.hi, &high));
        }
    }

    swap_cf(out, &tmp);

end:
    clair_big_clear(&scale);
    clair_big_clear(&num);
    clair_big_clear(&low);
    clair_big_clear(&high);
    clair_cf_clear(&tmp);
    return rc;
}

int clair_cf_sqrt(
    ClairCertFloat *out,
    const ClairCertFloat *a
) {
    if (!compatible(out, a, NULL)) return CLAIR_BIG_INVALID;
    if (a->lo.sign < 0) return CLAIR_BIG_DOMAIN;

    ClairCertFloat tmp;
    clair_cf_init(&tmp, out->places);

    ClairBigInt scale, n, square, one;
    clair_big_init(&scale);
    clair_big_init(&n);
    clair_big_init(&square);
    clair_big_init(&one);

    int rc;
    TRY(cf_pow10(&scale, out->places));

    TRY(clair_big_mul(&n, &a->lo, &scale));
    TRY(clair_big_isqrt(&tmp.lo, &n));

    TRY(clair_big_mul(&n, &a->hi, &scale));
    TRY(clair_big_isqrt(&tmp.hi, &n));
    TRY(clair_big_mul(&square, &tmp.hi, &tmp.hi));

    if (clair_big_cmp(&square, &n) < 0) {
        TRY(clair_big_from_i64(&one, 1));
        TRY(clair_big_add(&tmp.hi, &tmp.hi, &one));
    }

    swap_cf(out, &tmp);

end:
    clair_big_clear(&scale);
    clair_big_clear(&n);
    clair_big_clear(&square);
    clair_big_clear(&one);
    clair_cf_clear(&tmp);
    return rc;
}

int clair_cf_guaranteed(
    const ClairCertFloat *x,
    uint32_t places,
    char **text
) {
    if (!text || x->places > 1000 || places > x->places)
        return CLAIR_BIG_INVALID;

    *text = NULL;

    ClairBigInt divisor, ql, qu, r;
    clair_big_init(&divisor);
    clair_big_init(&ql);
    clair_big_init(&qu);
    clair_big_init(&r);

    int rc;
    TRY(cf_pow10(&divisor, x->places - places));
    TRY(clair_big_divmod(&ql, &r, &x->lo, &divisor));
    TRY(clair_big_divmod(&qu, &r, &x->hi, &divisor));

    if (clair_big_cmp(&ql, &qu) ||
        (x->lo.sign < 0 && x->hi.sign >= 0)) {
        rc = CLAIR_CF_UNCERTIFIED;
        goto end;
    }

    char *raw = NULL;
    TRY(clair_big_to_decimal(&ql, &raw));

    int negative =
        ql.sign < 0 ||
        (x->lo.sign < 0 && x->hi.sign < 0);

    const char *digits = raw + (raw[0] == '-');
    size_t len = strlen(digits);

    size_t outlen =
        (size_t)places +
        (len > places ? len - places : 1) +
        (places ? 1 : 0) +
        negative;

    char *buf = malloc(outlen + 1);

    if (!buf) {
        free(raw);
        rc = CLAIR_BIG_NOMEM;
        goto end;
    }

    char *p = buf;

    if (negative) *p++ = '-';

    if (!places) {
        memcpy(p, digits, len);
        p += len;
    } else if (len <= places) {
        *p++ = '0';
        *p++ = '.';

        size_t zeros = places - len;
        memset(p, '0', zeros);
        p += zeros;

        memcpy(p, digits, len);
        p += len;
    } else {
        size_t integer = len - places;

        memcpy(p, digits, integer);
        p += integer;

        *p++ = '.';
        memcpy(p, digits + integer, places);
        p += places;
    }

    *p = 0;
    *text = buf;
    free(raw);

end:
    clair_big_clear(&divisor);
    clair_big_clear(&ql);
    clair_big_clear(&qu);
    clair_big_clear(&r);
    return rc;
}
