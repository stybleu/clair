# Clair V0.33

## Syntaxe Python : `+=`, `enumerate`, tranches

```text
def main() -> int {
    let xs: list[int] = [5, 3, 8, 1, 9, 2]
    let total: int = 0
    for i, v in enumerate(xs) {
        total += i * v
    }
    print(total)            // 50
    print(len(xs[1:4]))     // 3
    let s: string = "bonjour"
    print(s[::-1])          // ruojnob
    return 0
}
```

### `+=`, `-=`, `*=`, `/=`, `%=`

Sur une variable, une case de liste (`y[i] += 1`), une case de matrice (`c[i][j] += a * b`) ou
un champ (`self.total += x`, `self.vus[k] += 1`). `x += v` veut dire exactement `x = x + v` :

- les boucles reconnues le restent : `s += w[i][j] * x[j]` est une couche dense, sur tous les
  cœurs ; `c[i][j] += aik * b[k][j]` est un produit de matrices, en blocs ;
- comme pour `/`, `n /= 4` sur un `int` est une division entière ;
- `s += "texte"` ajoute au bout d'une chaîne, `r *= i` marche sur les grands entiers ;
- une fonction dans l'indice (`xs[f(i)] += 1`) est refusée, car elle serait appelée deux fois :
  il faut d'abord mettre son résultat dans une variable.

Le C généré l'écrit aussi : `x = x + v` devient `x += v;` (le code machine ne change pas : les
neuf programmes du rapport donnent exactement le même assembleur ARM qu'en V0.32).

### `for i, v in enumerate(xs)`

Comme en Python : `i` vaut 0, 1, 2... et `v` les éléments de la liste. `enumerate(xs, 1)`
commence à 1. Toutes les règles de `for v in xs` restent vraies (la liste qui grandit pendant la
boucle est parcourue jusqu'au bout, `break`, `continue`, `i` modifié dans la boucle ne change pas
la suite).

```c
int64_t clair_h_enum1 = 0;
...
for (int64_t clair_k_v = 0; clair_k_v < clair_count_v; clair_k_v++) {
    int64_t v = clair_items_v[clair_k_v];
    int64_t i = clair_h_enum1;
    clair_h_enum1 += 1;
    total += i * v;
}
```

Quand `i` ne sert pas, c'est une boucle `for v in xs` ordinaire.

### Tranches `xs[a:b]`, `xs[a:]`, `xs[:b]`, `xs[a:b:pas]`

Sur les listes et les chaînes, avec les règles de Python :

- une nouvelle liste (ou chaîne), copiée : la modifier ne touche pas l'originale ;
- les indices négatifs comptent depuis la fin (`xs[-2:]` : les deux derniers) ;
- les bornes qui dépassent sont ramenées dans la liste, sans erreur (`xs[10:]` est vide) ;
- un pas négatif parcourt à l'envers (`xs[::-1]`) ; un pas de 0 arrête le programme
  (`slice step cannot be zero`) ;
- une chaîne se coupe en octets, comme `len` la compte.

```c
ClairList part = clair_rt_list_slice(xs, 1, 4, 1, 0);
const char *envers = clair_rt_string_slice(s, 0, 0, -1, CLAIR_NO_START | CLAIR_NO_STOP);
```

Une tranche créée dans une boucle est rendue à la fin de chaque tour, comme les autres listes
neuves. `xs[a:b] = ...` est refusé. Un indice seul négatif (`xs[-1]`) reste refusé, comme avant.

## Tests

- `syntaxe_python.cl` : les cinq opérateurs sur variables, cases, matrices, champs, chaînes et
  grands entiers ; `enumerate` avec début, `break`, `continue`, liste qui grandit ; tranches de
  listes d'entiers, de nombres à virgule et de chaînes, et de chaînes ; produit de matrices et
  couche dense écrits avec `+=` toujours reconnus ; sortie identique à Python ;
- `slice_step_zero_runtime.cl` ; `invalid_compound_call.cl`, `invalid_slice_assign.cl`,
  `invalid_enumerate.cl`, `invalid_slice_type.cl` ;
- 400 programmes de tranches tirées au hasard (bornes absentes, négatives, trop grandes, pas
  négatifs) : sortie identique à Python, et 150 d'entre eux sans erreur d'AddressSanitizer ni
  d'UndefinedBehaviorSanitizer ;
- 300 programmes tirés au hasard avec `enumerate` et `+=` sur des listes modifiées pendant la
  boucle : sortie identique à Python ;
- les fuzzers des versions précédentes (boucles `for v in`, mémoire rendue, couches denses,
  produits de matrices) : toujours identiques.
