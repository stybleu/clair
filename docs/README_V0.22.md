# Clair V0.22

## Boucles `while` rapides

La boucle de comptage écrite à la main profite maintenant de la même version rapide que les `for` :

```text
let i: int = 0
while i < len(x) {
    y[i] = alpha * x[i] + y[i]
    i = i + 1
}
```

Clair reconnaît la forme `while i < fin { ... ; i = i + 1 }` quand `i` ne change que par cette
dernière ligne et que `fin` ne change pas dans la boucle (constante, variable non modifiée ou
`len(liste)` d'une liste qui ne grandit pas). Les bornes sont alors vérifiées une seule fois, à
l'entrée, à partir de la valeur de `i` à ce moment-là :

```c
int64_t clair_stop_i = (int64_t)x->len;
if (clair_rt_affine_ok(y, 0, 1, i, clair_stop_i) &&
    clair_rt_affine_ok(x, 0, 1, i, clair_stop_i)) {
    double *clair_data_y = (double *)y->data;
    double *clair_data_x = (double *)x->data;
    while (i < clair_stop_i) {
        clair_data_y[i] = alpha * clair_data_x[i] + clair_data_y[i];
        i = i + 1;
    }
} else {
    /* même boucle, vérifiée à chaque accès */
}
```

`continue` et `break` restent permis. Sur `axpy` écrit avec un `while` : 0,20 s → 0,07 s sur PC,
et la boucle est maintenant vectorisée en NEON sur ARM64 (elle ne l'était pas en V0.21). Le
chemin SIMD V4 existant garde la priorité pour la forme qu'il reconnaît.

## Rapport du code machine plus fiable

- Clair et le C écrit à la main tournent en alternance, 5 fois chacun, et le meilleur temps de
  chacun est gardé : les deux sont mesurés dans les mêmes conditions (un téléphone ralentit quand
  il chauffe) ;
- la colonne `NEON Cl/C` indique si la boucle chaude est vectorisée dans Clair et dans le C ;
- la colonne `ecart` donne directement la différence en % ;
- une ligne par programme, assez courte pour l'écran d'un téléphone ;
- `matmul` et `matrice` tournent 8 fois plus longtemps, pour que leur temps soit mesurable ;
- nouveau programme `while` : `axpy` écrit avec une boucle `while`.

## Correction dans les tests

`expect_runtime_error` dans `tests/run_tests.sh` passait la sortie attendue directement à
`printf` : une sortie qui commence par `-1` était prise pour une option. Elle passe maintenant
par `printf '%b'`.

## Limite connue

Le produit de matrices (`matmul`, `matrice`) reste 5 à 8 % derrière le C écrit à la main. Une
partie vient des vérifications de bornes refaites à chaque tour de la boucle du milieu, avant la
boucle intérieure. Même en les sortant de toute la boucle (essai à la main), il resterait environ
4 % : le C écrit à la main utilise un seul `n` pour toutes les largeurs de ligne, alors que Clair
lit la largeur de chaque matrice séparément.
