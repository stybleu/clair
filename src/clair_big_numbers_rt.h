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
typedef struct ClairNumFrameGuard { int active; } ClairNumFrameGuard;
void clair_rt_num_root_enter(ClairNumRoot *root, const void *slot);
void clair_rt_num_root_cleanup(ClairNumRoot *root);
void clair_rt_num_frame_enter(ClairNumFrameGuard *guard);
void clair_rt_num_frame_cleanup(ClairNumFrameGuard *guard);
void clair_rt_num_collect(void);
ClairDynInt *clair_rt_bigint_parse(const char *s);
ClairDynInt *clair_rt_bigint_neg(const ClairDynInt *v);
ClairDynInt *clair_rt_bigint_add(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_sub(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_mul(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_div(const ClairDynInt *a, const ClairDynInt *b);
ClairDynInt *clair_rt_bigint_mod(const ClairDynInt *a, const ClairDynInt *b);
int clair_rt_bigint_cmp(const ClairDynInt *a, const ClairDynInt *b);
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
