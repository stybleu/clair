# Clair V0.75

Les fonctions deviennent des valeurs, comme en Python. On peut les donner à une autre fonction, les
ranger dans une variable, une liste, un dictionnaire ou un champ, et en fabriquer avec `lambda`. Avec
elles arrivent `sorted`, `sort`, `min`, `max`, `sum`, `map` et `filter`, avec `key=` et `reverse=`.

## Une fonction donnée à une autre

```
def carre(x: float) -> float {
    return x * x
}

def integrer(f: def(float) -> float, a: float, b: float, n: int) -> float {
    let h = (b - a) / float(n)
    let s = 0.0
    for i in range(n) {
        s += f(a + (float(i) + 0.5) * h)
    }
    return s * h
}

print(integrer(carre, 0.0, 3.0, 1000))
let k = 3.0
print(integrer(lambda x: k * x, 0.0, 2.0, 8))
```

- Le type d'une fonction s'écrit comme sa signature :
  - `def(float) -> float` ;
  - `def(int, int) -> bool` ;
  - `def(int)` pour une fonction qui ne rend rien ;
  - `def() -> string` pour une fonction sans paramètre.
- On peut donner :
  - une fonction du programme ;
  - une fonction d'un module : `outils.triple`, ou `triple` après `from outils import triple` ;
  - une méthode : `Vec.norme2`, `str.lower` ;
  - les fonctions de Clair `len`, `str`, `abs`, `int`, `float`, `sum`, `min` et `max`.
- Si les types ne diffèrent que par une conversion, Clair la fait. Exemple : `sorted(entiers, key=carre)`
  avec `carre(x: float)`.

## lambda

```
let double: def(int) -> int = lambda x: 2 * x
for v in sorted(villes, key=lambda v: v.habitants) {
    print(v.nom)
}
```

- Les types des paramètres viennent de l'endroit où va la lambda : un paramètre, `let f: def(...)`,
  `key=`, `map()` ou `filter()`.
- `let f = lambda x: x` seul est refusé, et le message dit quoi écrire.
- **Une différence avec Python.** Une lambda garde la valeur qu'avaient les variables au moment où elle a
  été faite, comme `lambda x, m=m: ...` en Python. Python, lui, relit la variable à chaque appel :

  ```
  let m = 2
  let fois_m: def(int) -> int = lambda x: x * m
  m = 100
  print(fois_m(7))      // 14 ; Python donnerait 700
  ```

  Dans une boucle, chaque lambda garde donc son `i`. Le piège classique de Python, où toutes rendent le
  dernier `i`, n'existe pas.
- Un objet ou une liste gardé par une lambda est partagé, comme en Python : ses changements se voient.

## Des fonctions qui font des fonctions

```
def ajouteur(n: int) -> def(int) -> int {
    return lambda x: x + n
}

def compose(f: def(int) -> int, g: def(int) -> int) -> def(int) -> int {
    return lambda x: f(g(x))
}

let plus3 = ajouteur(3)
print(plus3(4))                       // 7
print(ajouteur(10)(5))                // 15
print(compose(plus3, double)(5))      // 13
```

## Listes, dictionnaires et champs de fonctions

```
let ops: list[def(int) -> int] = [double, plus3, lambda x: x * x]
for op in ops {
    print(op(7))
}
print(ops[2](9))

let table: dict[string, def(int) -> int] = {}
table["carre"] = lambda x: x * x
print(table["carre"](12))

Class Bouton {
    nom: string
    action: def(int) -> int
}
let b = Bouton("ok", lambda x: x - 1)
print(b.action(10))
```

## sorted, sort, min, max, sum, map, filter

| écrit | donne |
|---|---|
| `sorted(xs)` | une nouvelle liste triée ; `xs` ne change pas |
| `sorted(xs, key=len, reverse=true)` | triée par `len`, de la plus grande à la plus petite |
| `xs.sort()`, `xs.sort(key=..., reverse=...)` | trie `xs` elle-même |
| `min(xs)`, `max(xs)`, `max(xs, key=...)` | le plus petit ou le plus grand élément |
| `sum(xs)` | la somme : un `int` pour des entiers ou des booléens, un `float` pour des flottants |
| `map(f, xs)` | la liste des `f(x)` |
| `filter(f, xs)` | la liste des `x` pour lesquels `f(x)` est vrai |

- Le tri est stable : à clé égale, les éléments restent dans leur ordre. C'est vrai aussi avec
  `reverse=true`, comme en Python.
- Les textes se trient par point de code, comme en Python :
  - les majuscules passent avant les minuscules, et les lettres accentuées après ;
  - `key=str.lower` trie sans tenir compte des majuscules.
- `key` est appelée une fois par élément, dans l'ordre.
- Une liste d'objets ou de listes se trie avec `key=` : `sorted(villes, key=lambda v: v.nom)`.
- `min` et `max` :
  - à égalité, ils rendent le premier élément ;
  - d'une liste vide, ils arrêtent le programme avec le message de Python,
    `min() iterable argument is empty`.
- `sum` de flottants additionne comme Python 3.12 (la somme compensée de Neumaier) :
  `sum([0.1] * 10) == 1.0` est vrai.
- `map` et `filter` donnent des listes. En Python, on écrit `list(map(...))`.
- Un dictionnaire donne ses clés : `for k in sorted(d)`, `max(d, key=lambda k: d[k])`.
- `min(a, b)` et `max(a, b)` de deux nombres restent ce qu'ils étaient.

## Ce qui n'est pas encore là

- Comparer deux fonctions avec `==` est refusé.
- Les arguments nommés et les valeurs par défaut pour les fonctions du programme
  (`aire(largeur=2.0)`).
- Trier une liste de listes sans `key=`. Python les compare élément par élément.
- `sorted(d.items())` : les dictionnaires n'ont pas encore `items()`, `keys()` ni `values()`.
- `sum(x * x for x in xs)` : on écrit `sum([x * x for x in xs])`.
- Une lambda contient une expression, comme en Python. `print` est une instruction en Clair : il ne va
  pas dans une lambda.

## Aussi dans cette version

- **`ops[i](x)`, `ajouteur(3)(4)` :** on peut appeler la valeur que donne une expression.
- **Correctif de la V0.74 (mémoire).** Une boucle rend à chaque tour la mémoire de ses textes et de ses
  objets, sauf ceux qu'elle range plus loin. Depuis la V0.74, ce test oubliait les nouveaux cas :
  - une liste d'objets ;
  - une liste de listes de textes ;
  - un dictionnaire d'objets.

  Exemple :

  ```
  let best: list[Point] = []
  for i in range(3) {
      best = [Point(float(i), "p" + str(i))]
  }
  print(best[0].nom)      // lisait une mémoire déjà rendue
  ```

  Ces listes gardent maintenant leurs textes.

## Comment c'est fait

- `src/parser.c` :
  - les types `def(A, B) -> R`, aussi dans `list[...]` et `dict[..., ...]` ;
  - `lambda` (ce n'est pas un mot réservé) ;
  - `key=valeur` dans un appel ;
  - l'appel d'une expression, `ops[i](x)`.
- `src/semantic.c` :
  - une lambda devient une fonction du programme, `lambda_1(variables gardées..., paramètres...)` ;
  - à sa place reste la valeur `@fn:lambda_1(variables gardées...)` ;
  - l'appel d'une valeur devient `@fncall` ;
  - `sorted`, `min`... deviennent `@sorted`, `@sort`, `@minl`, `@maxl`, `@sum`, `@map` et `@filter`.
- `src/codegen_c.c` :
  - chaque type de fonction est une structure C `{ R (*fn)(void *, A...); void *env; }`. L'appel se fait
    sans conversion de pointeur, donc Clang en vérifie les types ;
  - `clair_fw_f(env, x)` appelle `f` ;
  - `clair_mk_lambda_1(...)` range dans l'arène ce que la lambda garde ;
  - le tri est un tri fusion stable, écrit une fois par sorte de liste ;
  - les analyses (bornes, boucles, mémoire) considèrent qu'un appel par une valeur peut tout faire.
- `src/modules.c` : une fonction d'un module peut être donnée comme valeur.

## Tests

- `fonctions_valeurs.cl` :
  - `integrer(carre, ...)`, des lambdas qui gardent leurs variables ;
  - des fonctions qui rendent des fonctions, `compose` ;
  - des listes, des dictionnaires et des champs de fonctions ;
  - `Vec.norme2`, `str.upper`.
- `tris.cl` :
  - `sorted`, `sort`, `min`, `max`, `sum`, `map` et `filter` sur des entiers, des textes accentués, des
    objets, des listes de listes et des dictionnaires ;
  - la stabilité avec `key=` et `reverse=` sur 3 000 nombres ;
  - la somme de Neumaier.
- `regions_listes.cl` : le correctif de la mémoire des boucles.
- Ces trois tests sont comparés à la sortie du même programme en Python (`tests/*.py`). Ils sont aussi
  compilés avec AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur de mémoire. Valgrind,
  dans mon bac à sable, ne voit aucune fuite.
- Sept refus, chacun avec son message : une lambda sans type, une lambda au mauvais nombre de paramètres,
  une fonction du mauvais type, le tri d'objets sans `key=`, un argument nommé inconnu, l'appel d'une
  liste, un argument nommé pour une fonction du programme.
- `min_empty_runtime.cl` : `min()` d'une liste vide arrête le programme, à la bonne ligne.
