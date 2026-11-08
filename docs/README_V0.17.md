# Clair V0.17

Objectif de Clair : la syntaxe de Python avec les performances du C, pour le calcul scientifique
et l'IA. La V0.17 vise donc le C généré : il doit être aussi rapide et aussi lisible que du C
écrit à la main, pour que Clang puisse l'optimiser à fond.

## Boucles rapides sur les listes

Dans une boucle `for i in range(...)`, chaque `x[i]` vérifiait les bornes à chaque tour. Clang ne
pouvait alors pas vectoriser la boucle (NEON sur ARM64, SSE/AVX sur PC), car chaque tour pouvait
s'arrêter sur une erreur.

Maintenant, quand les indices sont de la forme `c0 + c1 * i` (par exemple `x[i]`, `a[i * n + k]`,
`v[n - 1 - i]`), Clair vérifie une seule fois, avant la boucle, que tous les indices resteront
dans la liste. Si c'est le cas, une version sans vérification s'exécute. Sinon, l'ancienne
version vérifiée s'exécute, et l'erreur apparaît exactement au même moment qu'avant.

```text
def axpy(alpha: float, x: list[float], y: list[float]) -> int {
    for i in range(len(x)) {
        y[i] = alpha * x[i] + y[i]
    }
    return 0
}
```

devient :

```c
static int64_t clair_fn_axpy(double alpha, ClairList x, ClairList y) {
    {
        int64_t clair_stop_i = (int64_t)x->len;
        if (clair_rt_affine_ok(y, 0, 1, 0, clair_stop_i) &&
            clair_rt_affine_ok(x, 0, 1, 0, clair_stop_i)) {
            double *clair_data_y = (double *)y->data;
            double *clair_data_x = (double *)x->data;
            for (int64_t i = 0; i < clair_stop_i; i++) {
                clair_data_y[i] = alpha * clair_data_x[i] + clair_data_y[i];
            }
        } else {
            /* même boucle avec vérification à chaque accès */
        }
    }
    return 0;
}
```

Conditions pour la version rapide :

- boucle `for` avec un pas de 1, et sans autre boucle à l'intérieur (ce sont les boucles chaudes) ;
- la boucle ne modifie ni `i`, ni les variables utilisées dans les indices ;
- rien dans la boucle ne peut agrandir une liste : pas de `append`, et seulement des appels à des
  fonctions qui n'en font jamais (Clair le vérifie, appels imbriqués compris) ;
- la liste est une variable locale ou un paramètre (pas encore `self.items[i]`).

Mesures sur PC (x86-64, Clang 18), comparées au même programme écrit à la main en C :

| Programme | Clair V0.16 | Clair V0.17 | C écrit à la main |
|---|---|---|---|
| `axpy` (4096 éléments, 100 000 fois) | 0,47 s | 0,15 s | 0,16 s |
| produit de matrices 256 × 256 (4 fois) | 0,062 s | 0,048 s | 0,046 s |

Sur ARM64, `axpy` et le produit de matrices sont maintenant vectorisés en NEON (`fmla v.2d`),
comme la version écrite à la main ; ils ne l'étaient pas avant.

## Un C qui se lit comme du C écrit à la main

- `for (int64_t i = 0; i < n; i++)` au lieu de dix lignes de variables `__clair_range_*` ;
- parenthèses seulement là où le C en a besoin : `s = s + a[i] * b[i]` ;
- `x / 2` écrit tel quel quand le diviseur est une constante non nulle (seule une division par
  une variable passe par la vérification de division par zéro) ;
- accès typés `*clair_rt_list_at_f64_checked(a, i)` au lieu de casts `(*(double *)...)` ;
- le C n'inclut que les morceaux du runtime que le programme utilise.

## Correction : constantes entières en 64 bits

`100000 * 100000` donnait `1410065408`, car le C calculait les constantes en `int` 32 bits.
Les calculs entre constantes sont maintenant faits par le compilateur Clair, en 64 bits :
le C contient directement `10000000000`.

## Rapport du code machine

```bash
bash tools/rapport_code_machine.sh
```

Compile quatre programmes types (`tools/bench/`) avec Clair et leur équivalent écrit à la main en
C, puis affiche pour chacun : si Clang a vectorisé la boucle chaude, les deux temps, et si les
résultats sont identiques.

## Limites connues

- les boucles `while` ne profitent pas encore de la version rapide ;
- une somme de `float` (`s = s + a[i] * b[i]`) n'est pas vectorisée entièrement, ni en Clair ni en
  C : Clang garde l'ordre exact des additions pour ne pas changer le résultat ;
- pas encore d'opérateur `%` (modulo).
