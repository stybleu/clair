#ifndef CLAIR_BIGMATH_H
#define CLAIR_BIGMATH_H
#include "clair_bigfloat.h"

/*
 * V0.45 : fonctions mathematiques des grands nombres.
 *
 * bigfloat : le resultat a 'precision' chiffres significatifs, avec une erreur
 * de moins d'une unite sur le dernier chiffre (calcul interne avec des chiffres
 * de garde, repris plus precis si le resultat est trop petit pour eux).
 * sqrt est arrondie exactement (au plus proche, egalite au chiffre pair).
 * Erreurs : CLAIR_BIG_DOMAIN (sqrt(-1), log(0), asin(2)), CLAIR_BIG_TOO_LARGE.
 */
int clair_bf_sqrt(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_exp(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_log(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_log10(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_log2(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_sin(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_cos(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_tan(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_atan(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_asin(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_acos(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_atan2(ClairBigFloat *out, const ClairBigFloat *y, const ClairBigFloat *x, uint32_t precision);
int clair_bf_sinh(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_cosh(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_tanh(ClairBigFloat *out, const ClairBigFloat *x, uint32_t precision);
int clair_bf_pow(ClairBigFloat *out, const ClairBigFloat *x, const ClairBigFloat *y, uint32_t precision);
int clair_bf_hypot(ClairBigFloat *out, const ClairBigFloat *x, const ClairBigFloat *y, uint32_t precision);
int clair_bf_pi(ClairBigFloat *out, uint32_t precision);

/* conversions exactes ou arrondies */
int clair_bf_from_double(ClairBigFloat *out, double x);              /* exact (jusqu'a 767 chiffres) */
int clair_bf_from_bigint(ClairBigFloat *out, const ClairBigInt *n);  /* exact */
int clair_bf_to_double(const ClairBigFloat *x, double *out);         /* au plus proche */
int clair_bf_floor(ClairBigInt *out, const ClairBigFloat *x, int mode); /* mode -1 floor, 1 ceil, 0 vers zero */

/* bigint */
int clair_big_isqrt(ClairBigInt *out, const ClairBigInt *n);
int clair_big_gcd(ClairBigInt *out, const ClairBigInt *a, const ClairBigInt *b);
int clair_big_powmod(ClairBigInt *out, const ClairBigInt *b, const ClairBigInt *e, const ClairBigInt *m);
int clair_big_to_double(const ClairBigInt *n, double *out);
int clair_big_to_i64(const ClairBigInt *n, int64_t *out);
#endif
