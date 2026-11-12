# Clair V0.76

Les tuples arrivent, comme en Python. Un tuple regroupe plusieurs valeurs. On peut :
- le rendre depuis une fonction ;
- le défaire en variables ;
- le comparer et s'en servir pour trier ;
- l'utiliser comme clé de dictionnaire.

## Un tuple

```
let t = (3, "trois", 2.5)
print(t[0])                              // 3
print(t[-1])                             // 2.5
print(len(t))                            // 3
let p: tuple[float, float] = (1, 2)      // (1.0, 2.0)
```

- Le type s'écrit `tuple[int, string, float]`. Comme pour `let x = 5`, il est deviné de la valeur.
- On lit un tuple avec un indice écrit en chiffres, `t[0]` ou `t[-1]`, parce que ses éléments ont chacun leur type. `t[i]` avec un `i` calculé est refusé.
- Un tuple ne change pas, comme en Python : `t[0] = 5` est refusé.
- Il se copie comme un nombre (c'est une structure du C) : il n'y a rien à rendre à la mémoire.
- Il peut contenir des nombres, des textes, des booléens, des objets, des listes, des dictionnaires, des fonctions et d'autres tuples.

## Plusieurs valeurs rendues

```
def divise(a: int, b: int) -> tuple[int, int] {
    return a / b, a % b
}

def bornes(xs: list[int]) {              // rend un tuple[int, int], deviné
    return min(xs), max(xs)
}

let (q, r) = divise(17, 5)
let lo, hi = bornes([4, 9, -2])
let _, milieu, _ = (1, 2, 3)
```

- `let a, b = ...` et `let (a, b) = ...` s'écrivent tous les deux.
- Il faut autant de noms que d'éléments, sinon c'est refusé. Python, lui, s'arrêterait en cours de route (`too many values to unpack`).
- `_` ne crée pas de variable.

## Échanger, avancer

```
a, b = b, a
xs[i], xs[j] = xs[j], xs[i]
self.a, self.b = self.b, self.a + self.b
```

Comme en Python, tout le côté droit est calculé avant d'écrire à gauche.

## Les boucles

```
for nom, age in gens {
    print(nom)
}
for mot, n in compte.items() {
    print(mot + " " + str(n))
}
for x, y in zip(xs, ys) {
    print(x + y)
}
```

- `for i, v in enumerate(xs)` reste comme avant.
- Un nom que la boucle ne lit jamais ne crée pas de variable, donc Clang ne signale pas de variable inutilisée.

## Comparer et trier

```
print((1, 2) == (1, 2))                  // 1
print((1, "b") < (2, "a"))               // 1
print("pomme" < "poire")                 // 1 : les textes se comparent aussi
for g in sorted(gens, key=lambda g: (-g[1], g[0])) {
    print(g[0])                          // par âge décroissant, puis par nom
}
print(max(gens)[0])
if (x, y) in visites {
    print("deja vu")
}
```

- Comme en Python, le premier élément qui diffère décide.
- Les textes se comparent par point de code.
- `==` et `<` demandent deux tuples des mêmes types. Leurs éléments doivent être des nombres, des booléens, des textes ou d'autres tuples.

## Les dictionnaires

```
let memo: dict[tuple[int, int], int] = {}
memo[(i, j)] = n
if (i, j) in memo {
    return memo[(i, j)]
}
for x, y in memo {
    print(x + y)
}

let compte: dict[string, int] = {}
for mot in texte.split() {
    compte[mot] = compte.get(mot, 0) + 1
}
for mot, n in sorted(compte.items(), key=lambda kv: (-kv[1], kv[0])) {
    print(mot)
}
print(sum(compte.values()))
```

- **Clé tuple.**
  - Elle contient des `int` et des `bool` : des coordonnées sur une grille, ou un mémo à deux arguments.
  - Elle est gardée en texte, `"3,4"`, comme les clés `int` de la V0.74.
- **`d.get(k, defaut)`** rend `defaut` quand la clé manque. Clair n'a pas `None`, donc la valeur par défaut est obligatoire.
- **`d.items()`, `d.keys()` et `d.values()`** donnent des listes. Python, lui, donne des vues.

## zip, partition

- `zip(xs, ys)` donne la liste des `(xs[i], ys[i])`. Elle est aussi longue que la plus courte des deux listes. En Python, on écrit `list(zip(xs, ys))`.
- `s.partition(sep)` et `s.rpartition(sep)` donnent `(avant, sep, après)`, comme en Python.

## Ce qui n'est pas encore là

- **Afficher un tuple.** `print(t)` est refusé, comme `print(xs)` pour une liste. On écrit `print(t[0])`.
- **Les unités dans un tuple.** `(1.0[m], 2.0[s])` est refusé : sinon, le tuple perdrait ses unités sans le dire.
- **Les formes imbriquées.** `for i, (a, b) in enumerate(paires)`, `let (a, (b, c)) = ...` et `[x for x, y in paires]` ne s'écrivent pas encore.
- **Le tuple vide `()`.**
- **Les clés tuple qui contiennent des textes**, comme `dict[tuple[string, int], ...]`.

## Aussi dans cette version

- **Les textes se comparent.** `<`, `<=`, `>` et `>=` comparent deux textes par point de code, comme Python. Avant, c'était refusé. Le test `invalid_string_order.cl` vérifie maintenant qu'un texte et un nombre ne se comparent pas.
- **Un message plus utile.** Ce test changé :

  ```
  let xs = [1, 2, 3]
  for i, v in xs {
      print(v)
  }
  ```

  Il dit maintenant que les éléments de `list[int]` ne sont pas des tuples, et propose `enumerate(xs)`.

## Comment c'est fait

- `src/parser.c` :
  - le tuple littéral `(a, b)` ;
  - les virgules sans parenthèses après `return`, `let x =` et `x =` ;
  - les types `tuple[...]`, et les clés tuple d'un dictionnaire ;
  - `let a, b = v` devient `let @tup1n2 = v`, puis `let a = @tup1n2[0]` et `let b = @tup1n2[1]` ;
  - `a, b = v` devient la même chose, avec des écritures à la place des `let` ;
  - `for a, b in xs` devient `for @elt2n2 in xs { let a = @elt2n2[0] ... }`.
- `src/semantic.c` :
  - les types des tuples, et le nombre de noms vérifié ;
  - `t[0]` devient `@tget` ;
  - `==` et `<` sur les tuples et les textes ;
  - `d.items()`, `d.get()` et `zip()` deviennent `@dict_items`, `@dict_get` et `@zip`.
- `src/codegen_c.c` :
  - chaque type de tuple devient `struct ClairTuple_<n> { f0; f1; }`, déclarée avant les classes qui la contiennent et après les classes valeur qu'elle contient ;
  - `==`, `<` et les clés `"x,y"` sont de petites fonctions, écrites une fois par type de tuple.
- `src/clair_text_rt.c` : `partition` et `rpartition`.
- `src/units.c` : refuse une valeur avec unité dans un tuple.

## Tests

- **`tuples.cl`** couvre :
  - les indices, `len`, les retours multiples, typés ou devinés ;
  - les échanges (`a, b = b, a`, deux éléments d'une liste), Fibonacci par `a, b = b, a + b` ;
  - les listes de tuples triées par une clé tuple, `max`, `in` ;
  - les comparaisons de tuples et de textes accentués ;
  - `zip`, `items`, `get`, `keys`, `values` ;
  - les chemins d'une grille avec un mémo `dict[tuple[int, int], int]`, et un parcours en largeur avec murs ;
  - des tuples d'objets, de listes et de fonctions ;
  - `partition` et `rpartition` ;
  - une boucle qui garde un tuple de textes.
- `tuples.cl` est comparé à la sortie du même programme en Python (`tests/tuples.py`). Il est aussi compilé avec AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur de mémoire. Valgrind, dans mon bac à sable, ne voit aucune fuite.
- **Six refus**, chacun avec son message :
  - un indice calculé ;
  - une écriture dans un tuple ;
  - trop de noms ;
  - deux tuples de types différents comparés ;
  - `get()` sans valeur par défaut ;
  - une clé tuple avec un texte.
- **`partition_empty_runtime.cl`** : un séparateur vide arrête le programme, à la bonne ligne, comme le `ValueError` de Python.
