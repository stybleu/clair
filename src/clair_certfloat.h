#ifndef CLAIR_CERTFLOAT_H
#define CLAIR_CERTFLOAT_H
#include "clair_bigint.h"
#include <stdint.h>

enum { CLAIR_CF_UNCERTIFIED = 1 };

typedef struct {
    ClairBigInt lo, hi;
    uint32_t places;
} ClairCertFloat;

void clair_cf_init(ClairCertFloat *x, uint32_t places);
void clair_cf_clear(ClairCertFloat *x);

int clair_cf_parse(ClairCertFloat *out, const char *decimal);
int clair_cf_add(ClairCertFloat *out, const ClairCertFloat *a, const ClairCertFloat *b);
int clair_cf_sub(ClairCertFloat *out, const ClairCertFloat *a, const ClairCertFloat *b);
int clair_cf_mul(ClairCertFloat *out, const ClairCertFloat *a, const ClairCertFloat *b);
int clair_cf_div(ClairCertFloat *out, const ClairCertFloat *a, const ClairCertFloat *b);
int clair_cf_sqrt(ClairCertFloat *out, const ClairCertFloat *a);
int clair_cf_guaranteed(const ClairCertFloat *x, uint32_t places, char **text);

#endif
