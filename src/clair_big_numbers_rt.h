#ifndef CLAIR_BIG_NUMBERS_RT_H
#define CLAIR_BIG_NUMBERS_RT_H
#include "clair_bigfloat.h"
/* V0.40 : objets immuables, gestion dynamique et collecte a points surs.
   Ne jamais modifier les champs par du C genere. */
typedef struct ClairDynInt ClairDynInt;
typedef struct ClairDynFloat ClairDynFloat;
/* Racinement explicite des valeurs vivantes ; nettoyage automatique Clang.
 * Les adresses inscrites restent valides jusqu'a la sortie de leur bloc C. */
typedef struct ClairNumRoot {
    struct ClairNumRoot *previous;
    const void *slot;
} ClairNumRoot;
/* V0.42 : chaque appel de fonction a sa zone, les nombres crees pendant l'appel.
 * Une collecte ne touche que la zone de l'appel en cours : les temporaires des
 * appelants (a * b dans a * b + f(c)) sont plus anciens et ne sont jamais vus. */
typedef struct ClairNumFrameGuard {
    int active;
    struct ClairNumFrameGuard *previous;
    uint64_t serial;          /* les nombres plus recents appartiennent a l'appel */
    uint64_t clean;           /* numero du dernier nombre a la derniere collecte */
    ClairNumRoot *roots;      /* racines d'avant l'appel */
    const void *keep;         /* valeur rendue par return, gardee a la sortie */
} ClairNumFrameGuard;
void clair_rt_num_root_enter(ClairNumRoot *root, const void *slot);
void clair_rt_num_root_cleanup(ClairNumRoot *root);
void clair_rt_num_frame_enter(ClairNumFrameGuard *guard);
void clair_rt_num_frame_cleanup(ClairNumFrameGuard *guard);
void clair_rt_num_collect(void);
void *clair_rt_num_keep(ClairNumFrameGuard *guard, const void *value);   /* return v */
void *clair_rt_num_pin(const void *value);                                /* objet.champ = v */
ClairDynInt *clair_rt_bigint_parse(const char *s);
ClairDynInt *clair_rt_bigint_from_int(int64_t x);                 /* bigint(k) */
char *clair_rt_bigint_str(const ClairDynInt *v);                  /* str(b) : malloc */
char *clair_rt_bigfloat_str(const ClairDynFloat *v);
ClairDynInt *clair_rt_bigint_neg(const ClairDynInt *v);
ClairDynInt *clair_rt_bigint_add(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_sub(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_mul(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_div(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_mod(const ClairDynInt *a, const ClairDynInt *b);
int clair_rt_bigint_cmp(const ClairDynInt *a, const ClairDynInt *b);
/* V0.44 : un bigint avec un int, sans creer de bigint pour l'int (num * k, acc + 1) */
ClairDynInt *clair_rt_bigint_add_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_sub_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_int_sub(int64_t k, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_mul_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_div_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_mod_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_int_div(int64_t k, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_int_mod(int64_t k, const ClairDynInt *b);
int clair_rt_bigint_cmp_int(const ClairDynInt *a, int64_t k);
ClairDynInt *clair_rt_bigint_addmul_int(const ClairDynInt *a, const ClairDynInt *b, int64_t k);   /* a + b * k */
void clair_rt_bigint_print(const ClairDynInt *v);

ClairDynFloat *clair_rt_bigfloat_parse(const char *s, int64_t precision);
ClairDynFloat *clair_rt_bigfloat_neg(const ClairDynFloat *v);
ClairDynFloat *clair_rt_bigfloat_add(const ClairDynFloat *a, const ClairDynFloat *b);
ClairDynFloat *clair_rt_bigfloat_sub(const ClairDynFloat *a, const ClairDynFloat *b);
ClairDynFloat *clair_rt_bigfloat_mul(const ClairDynFloat *a, const ClairDynFloat *b);
ClairDynFloat *clair_rt_bigfloat_div(const ClairDynFloat *a, const ClairDynFloat *b);
int clair_rt_bigfloat_cmp(const ClairDynFloat *a, const ClairDynFloat *b);
void clair_rt_bigfloat_print(const ClairDynFloat *v);
#endif
