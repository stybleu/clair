#include "clair_certfloat.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * certfloat utilise les memes objets suivis que bigint et bigfloat.
 * clair_num_alloc(2) alloue une valeur suivie par le collecteur Clair.
 * Les chaines certdigits restent gerees separement jusqu'a la sortie.
 */

typedef struct ClairCFString {
    struct ClairCFString *next;
    struct ClairCFString *bucket_next;
    char *value;
    uint64_t hash;
} ClairCFString;

#define CLAIR_CF_BUCKET_COUNT 4096u
static ClairCFString *clair_cf_buckets[CLAIR_CF_BUCKET_COUNT];

static ClairCFString *clair_cf_strings;
static int clair_cf_strings_registered;

static void clair_cf_strings_cleanup(void) {
    while (clair_cf_strings) {
        ClairCFString *n = clair_cf_strings;
        clair_cf_strings = n->next;
        free(n->value);
        free(n);
    }
}

static void clair_cf_fail(const char *message) {
    fprintf(stderr, "Clair runtime error: certfloat: %s\n", message);
    exit(EXIT_FAILURE);
}

static void clair_cf_check(int rc) {
    if (rc == CLAIR_BIG_OK) return;

    if (rc == CLAIR_CF_UNCERTIFIED)
        clair_cf_fail("precision insuffisante pour certifier les decimales demandees");
    if (rc == CLAIR_BIG_DIVZERO)
        clair_cf_fail("division par un intervalle contenant zero");
    if (rc == CLAIR_BIG_DOMAIN)
        clair_cf_fail("sqrt d'un nombre negatif");
    if (rc == CLAIR_BIG_NOMEM)
        clair_cf_fail("memoire insuffisante");

    clair_cf_fail("nombre, precision ou operation invalide");
}

static ClairCertFloat *clair_cf_new(int64_t places) {
    if (places < 0 || places > 1000)
        clair_cf_fail("precision attendue entre 0 et 1000");

    ClairCertFloat *p = &clair_num_alloc(2)->u.c.v;
    clair_cf_init(p, (uint32_t)places);
    return p;
}

ClairCertFloat *clair_rt_certfloat(const char *s, int64_t places) {
    ClairCertFloat *r = clair_cf_new(places);
    clair_cf_check(clair_cf_parse(r, s));
    return r;
}

#define CF_BINARY(name) \
ClairCertFloat *clair_rt_certfloat_##name( \
    const ClairCertFloat *a, const ClairCertFloat *b) { \
    ClairCertFloat *r = clair_cf_new(a->places); \
    clair_cf_check(clair_cf_##name(r, a, b)); \
    return r; \
}

CF_BINARY(add)
CF_BINARY(sub)
CF_BINARY(mul)
CF_BINARY(div)

ClairCertFloat *clair_rt_certsqrt(const ClairCertFloat *x) {
    ClairCertFloat *r = clair_cf_new(x->places);
    clair_cf_check(clair_cf_sqrt(r, x));
    return r;
}

static uint64_t clair_cf_hash_text(const char *text) {
    uint64_t hash = UINT64_C(14695981039346656037);

    for (const unsigned char *p =
             (const unsigned char *)text; *p; ++p) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

const char *clair_rt_certdigits(
    const ClairCertFloat *x, int64_t places
) {
    if (places < 0 || places > 1000)
        clair_cf_fail("nombre de decimales invalide");

    char *text = NULL;
    clair_cf_check(
        clair_cf_guaranteed(x, (uint32_t)places, &text)
    );

    uint64_t hash = clair_cf_hash_text(text);
    size_t bucket = (size_t)(
        hash & (CLAIR_CF_BUCKET_COUNT - 1u)
    );

    for (ClairCFString *entry = clair_cf_buckets[bucket];
         entry; entry = entry->bucket_next) {
        if (entry->hash == hash &&
            strcmp(entry->value, text) == 0) {
            free(text);
            return entry->value;
        }
    }

    if (!clair_cf_strings_registered) {
        if (atexit(clair_cf_strings_cleanup))
            clair_cf_fail("impossible d'enregistrer le nettoyage");
        clair_cf_strings_registered = 1;
    }

    ClairCFString *n = malloc(sizeof(*n));
    if (!n) {
        free(text);
        clair_cf_fail("memoire insuffisante");
    }

    n->value = text;
    n->hash = hash;
    n->bucket_next = clair_cf_buckets[bucket];
    clair_cf_buckets[bucket] = n;

    n->next = clair_cf_strings;
    clair_cf_strings = n;

    return text;
}


/* Affichage immediat : aucun pointeur ne s'echappe. */
void clair_rt_certdigits_print(
    const ClairCertFloat *x, int64_t places
) {
    if (places < 0 || places > 1000)
        clair_cf_fail("nombre de decimales invalide");

    char *text = NULL;
    clair_cf_check(
        clair_cf_guaranteed(x, (uint32_t)places, &text)
    );

    int status = puts(text);
    free(text);

    if (status == EOF)
        clair_cf_fail("echec de l'affichage");
}
