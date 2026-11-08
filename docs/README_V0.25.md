# Clair V0.25

## Produit de matrices en blocs

Clair reconnaît la triple boucle du produit de matrices :

```text
for i in range(rows(a)) {
    for k in range(cols(a)) {
        let aik: float = a[i][k]
        for j in range(cols(b)) {
            c[i][j] = c[i][j] + aik * b[k][j]
        }
    }
}
```

et la remplace par un appel à un noyau en blocs, comme le ferait un programmeur C qui appelle une
bibliothèque :

```c
if (clair_rt_matmul_ok(a, b, c, a->rows, b->cols, a->cols)) {
    clair_rt_matmul_f64(a->rows, b->cols, a->cols,
        (const double *)a->data, a->cols,
        (const double *)b->data, b->cols,
        (double *)c->data, c->cols);
} else {
    /* la triple boucle, exactement comme avant */
}
```

### Pourquoi c'est plus rapide

Dans la triple boucle simple, chaque multiplication relit et réécrit `c[i][j]` en mémoire. Le
noyau garde un bloc de 4 lignes × 8 colonnes de `c` dans les registres du processeur pendant que
`k` parcourt 128 valeurs. Sur ARM64, la boucle intérieure devient 16 instructions `fmla`
(multiplication-addition sur 2 nombres à la fois) pour seulement 6 instructions de lecture, et
aucune écriture.

Mesure sur PC : `matmul` 0,089 s → 0,065 s, `matrice` 0,113 s → 0,067 s, alors que la même
triple boucle écrite à la main en C prend 0,10 s. Dans le rapport, ces deux lignes montrent donc
maintenant Clair plus rapide que le C : la colonne C reste la triple boucle simple.

### Résultat identique au bit près

Chaque `c[i][j]` reçoit exactement les mêmes additions, dans le même ordre (`k` croissant), que
dans la triple boucle : seul l'ordre entre des cases différentes de `c` change. Le résultat est
donc identique au bit près, avec ou sans multiplication-addition fusionnée (FMA).

### Quand le noyau n'est pas utilisé

Clair garde la triple boucle d'avant (avec ses vérifications et ses messages d'erreur au même
moment) quand :

- les dimensions ne rentrent pas dans `a`, `b` ou `c` ;
- `c` est la même matrice que `a` ou `b` (le résultat dépendrait de l'ordre) ;
- une des dimensions vaut 0 ;
- pour les listes à plat, les lignes de `c` se chevauchent (`c[i * ld + j]` avec `ld` plus petit
  que le nombre de colonnes).

### Formes reconnues

- matrices `matrix[float]` (`c[i][j]`) ou listes à plat `list[float]` (`c[i * n + j]`) ;
- avec ou sans la variable `aik` ;
- les opérandes de `+` et de `*` dans n'importe quel ordre (`b[k][j] * aik + c[i][j]`) ;
- les boucles `range(n)` ou `range(0, n)`, avec des bornes qui ne changent pas : nombres,
  variables, `len()`, `rows()`, `cols()` et leurs `+ - *`.

## Rapport du code machine : temps médian

Le rapport garde maintenant le temps médian des 5 lancements au lieu du meilleur. Sur le
téléphone, les listes tombent à un endroit différent de la mémoire à chaque lancement, et ce
placement change le temps de quelques %. Le meilleur temps récompensait le lancement le plus
chanceux : `axpy` a donné +6 %, puis −3 %, puis +16 % d'une version à l'autre, alors que son code
n'avait pas changé.

## Tests

- `matmul_blocks.cl` : tailles de 1×1 à 64×300×33 (restes de lignes, de colonnes et de blocs de
  `k`), matrices et listes à plat ; chaque produit est comparé case par case à la triple boucle
  non reconnue : 0 différence ; `c` égale à `a` ; dimension nulle ;
- `matmul_blocks_bounds.cl` : `c` trop petite, l'erreur arrive au même moment qu'avant ;
- 750 produits tirés au hasard (formes, ordres des opérandes, `c` égale à `a` ou `b`, tailles
  qui ne rentrent pas, lignes qui se chevauchent, `k` jusqu'à 160) affichés avec 17 chiffres :
  sortie identique à la V0.24, avec et sans FMA ;
- en dehors des programmes qui contiennent un produit de matrices, le C généré est identique à
  la V0.24.
