# Clair V0.44

## Un indice qui monte et descend, prouvé une seule fois

```text
let k: int = 0
while k >= 0 {
    let y: u32 = (l[k] | c[k] | r[k]) & y0      // nqueen
    ...
    if k < n - 1 {
        k += 1
        l[k] = (l[k - 1] | z) << 1
    } else {
        k -= 1
    }
}
```

Dans une pile ou un retour en arrière (nqueen, sudoku, un parcours d'arbre), l'indice `k` monte
et descend : aucune des preuves d'avant (`for i in range(n)`, `while i < n`) ne s'applique, et
chaque `l[k]` était vérifié.

Clair devine maintenant un intervalle pour `k` au début de chaque tour, à partir de la condition
de la boucle (`k >= 0`) et des tests autour de `k += 1` (`if k < n - 1`) : ici `0 <= k <= n - 1`.
Puis il le prouve : il parcourt le corps de la boucle avec des intervalles (`k += 1` dans
`if k < n - 1` donne `1 <= k <= n - 1`, `k -= 1` donne `-1 <= k <= n - 2`...) et vérifie que
chaque retour au début de la boucle reste dans l'intervalle deviné.

Avant la boucle, il ne reste à vérifier qu'une fois :

```c
if ((__int128)k >= 0 && (__int128)k <= ((__int128)n - 1) &&
    ((__int128)n - 1) < (__int128)l->len && ...) {
    /* la boucle, sans aucune vérification de l[k], c[k], r[k], a[k] */
} else {
    /* la boucle d'origine, avec ses erreurs au même moment */
}
```

Les bornes sont des nombres ou `n + c` avec `n` qui ne change pas dans la boucle (un nom ou
`len(liste)`). Deux bornes ne se comparent que si elles ont le même `n` : quand Clair ne peut pas
conclure, il garde la vérification.

Sur la machine de test, nqueen passe de +50 % à presque égalité avec le C.

## `bigint` avec un `int`, comme en Python

```text
acc = (acc + num * 2) * k2          // avant : (acc + num * bigint(2)) * bigint(k2)
let d: bigint = (num * 3 + acc) / den
if num > acc { ... }
```

- `+ - * / %` et les comparaisons acceptent un `bigint` d'un côté et un `int` (ou un entier
  machine qui tient dans un `int`) de l'autre. L'`int` n'est pas transformé en `bigint` : il
  reste un mot sur la pile.
- `a + b * k` se calcule en un seul passage, comme `mpz_addmul_ui` de GMP.
- `bigint + float` reste refusé.

La bibliothèque des grands nombres va aussi plus vite :

- la multiplication par un seul mot (`num * k`, `acc * 10`) a sa propre boucle ;
- `reserve()` ne remet plus à zéro la mémoire qu'elle agrandit : seules la multiplication et le
  décalage, qui cumulent dans le résultat, la remettent à zéro (c'était un quart du temps de
  pidigits) ;
- les boucles d'addition et de soustraction ne testent plus la longueur à chaque mot.

pidigits fait 2,3 fois moins d'instructions qu'en V0.43 (mesuré avec callgrind).

## Tests

- `indices_prouves.cl` : une pile qui monte et descend, sortie identique à Python ;
  `range_bounds_runtime.cl` : une boucle dont la preuve échoue (`n` plus grand que la liste)
  s'arrête toujours sur l'erreur, au même moment ;
- `bigint_avec_int.cl` : `a + 1`, `1 - a`, `a * k`, `1000000 / a`, `a + a * 9`, comparaisons,
  factorielle de 30 ; identique à Python ;
- `big_numbers_invalid_v040.cl` vérifie maintenant que `bigint + float` est refusé ;
- 4000 boucles tirées au hasard où l'indice monte et descend (conditions, tests, pas de 1 ou 2,
  `k = 0`, `break`, `continue`, listes trop courtes) : même sortie et mêmes erreurs qu'avec la
  V0.43 ; 2529 sont prouvées, et un septième d'entre elles passe sans erreur
  d'AddressSanitizer ;
- 4000 opérations `bigint` avec `int` tirées au hasard (jusqu'à 300 bits, `int` de 64 bits,
  signes mêlés) et 3000 opérations entre `bigint` : identiques à Python, sans erreur
  d'AddressSanitizer ;
- 300 programmes à grands nombres tirés au hasard (récursion, boucles, champs) : identiques à
  Python.
