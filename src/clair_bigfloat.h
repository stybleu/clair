#ifndef CLAIR_BIGFLOAT_H
#define CLAIR_BIGFLOAT_H
#include "clair_bigint.h"
#include <stdint.h>

/* Expérimental : flottant décimal signé = coefficient entier * 10^exp10.
   Les opérations arrondissent à 'precision' chiffres significatifs, au plus proche,
   les égalités exactement à mi-chemin vers le chiffre pair (ties-to-even).
   ATTENTION : ce moteur est décimal, pas encore un flottant binaire IEEE/MPFR.
   Les résultats sont des valeurs décimales, et non des approximations binaires. */
typedef struct {
    ClairBigInt coefficient;
    int64_t exp10;
} ClairBigFloat;

void clair_bf_init(ClairBigFloat *x);
void clair_bf_clear(ClairBigFloat *x);
int clair_bf_parse(ClairBigFloat *out, const char *s, uint32_t precision);
int clair_bf_format(const ClairBigFloat *x, char **out); /* résultat malloc, appelant free */
int clair_bf_add(ClairBigFloat *out, const ClairBigFloat *a, const ClairBigFloat *b, uint32_t precision);
int clair_bf_sub(ClairBigFloat *out, const ClairBigFloat *a, const ClairBigFloat *b, uint32_t precision);
int clair_bf_mul(ClairBigFloat *out, const ClairBigFloat *a, const ClairBigFloat *b, uint32_t precision);
int clair_bf_div(ClairBigFloat *out, const ClairBigFloat *a, const ClairBigFloat *b, uint32_t precision);
/* V0.45 : coefficient * 10^exp10 arrondi a 'precision' chiffres ; sticky : des chiffres non nuls
   ont ete coupes plus bas (le resultat exact n'est pas a mi-chemin). */
int clair_bf_from_scaled(ClairBigFloat *out, const ClairBigInt *coefficient, int64_t exp10,
                         uint32_t precision, int sticky);
#endif
