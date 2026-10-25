#ifndef CLAIR_BIGINT_H
#define CLAIR_BIGINT_H
#include <stddef.h>
#include <stdint.h>

/* Prototype autonome : signe -1, 0, +1 ; magnitude en mots de 64 bits, petit-boutiste. */
typedef struct {
    uint64_t *word;
    size_t len;
    size_t cap;
    int sign;
} ClairBigInt;

enum { CLAIR_BIG_OK = 0, CLAIR_BIG_NOMEM = -1, CLAIR_BIG_INVALID = -2,
       CLAIR_BIG_DIVZERO = -3, CLAIR_BIG_TOO_LARGE = -4 };
void clair_big_init(ClairBigInt *x);
void clair_big_clear(ClairBigInt *x);
int clair_big_from_i64(ClairBigInt *out, int64_t x);
int clair_big_from_decimal(ClairBigInt *out, const char *decimal);
int clair_big_to_decimal(const ClairBigInt *x, char **result); /* free(*result) */
int clair_big_copy(ClairBigInt *out, const ClairBigInt *x);
int clair_big_cmp(const ClairBigInt *a, const ClairBigInt *b);
size_t clair_big_bit_length(const ClairBigInt *a);
int clair_big_get_bit(const ClairBigInt *a, size_t bit);
int clair_big_set_bit(ClairBigInt *a, size_t bit); /* magnitude non négative */
int clair_big_shift_left(ClairBigInt *out, const ClairBigInt *a, size_t bits);
int clair_big_add(ClairBigInt *out, const ClairBigInt *a, const ClairBigInt *b);
int clair_big_sub(ClairBigInt *out, const ClairBigInt *a, const ClairBigInt *b);
int clair_big_mul(ClairBigInt *out, const ClairBigInt *a, const ClairBigInt *b);
int clair_big_divmod(ClairBigInt *quo, ClairBigInt *rem,
                     const ClairBigInt *a, const ClairBigInt *b); /* vers zero */
int clair_big_pow_u64(ClairBigInt *out, const ClairBigInt *base, uint64_t exponent);
#endif
