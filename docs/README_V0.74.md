# Clair V0.74

Les listes et les dictionnaires peuvent maintenant contenir des objets, d'autres listes et d'autres
dictionnaires, comme en Python. C'est la brique qu'il faut pour écrire un arbre, une grille, un
index, et plus tard le compilateur de Clair en Clair.

## Listes d'objets

```
Class Point {
    x: float
    y: float
}

let ps: list[Point] = []
for i in range(5) {
    ps.append(Point(float(i), 1.0))
}
print(ps[2].x)
ps[0] = Point(10.0, 10.0)
ps[1].y = 9.5
for p in ps {
    print(p.x + p.y)
}
let q = [Point(1.0, 2.0), Point(3.0, 4.0)]      // list[Point], deviné
```

- Une liste garde ses objets par référence, comme Python : `ps[1].y = 9.5` change l'objet lui-même,
  et `[Point(0.0, 0.0)] * 3` contient trois fois le même objet.
- Les tranches (`ps[1:3]`), `len`, `for`, `append`, `xs[-1]`, les compréhensions
  (`[Point(float(i), 0.0) for i in range(3)]`) marchent comme pour les autres listes.

## Un arbre

Une classe peut contenir une liste d'objets de sa propre classe :

```
Class Noeud {
    nom: string
    enfants: list[Noeud]
}

def compte(n: Noeud) -> int {
    let c = 1
    for e in n.enfants {
        c += compte(e)
    }
    return c
}

let racine = Noeud("racine", [])
racine.enfants.append(Noeud("a", []))
racine.enfants[0].enfants.append(Noeud("a1", []))
```

## Listes de listes

```
let grille: list[list[int]] = []
for i in range(3) {
    grille.append([0] * 4)
}
grille[1][2] = 99
grille[1][2] += 1
grille[0].append(7)
let carres = [[i, i * i] for i in range(5)]        // list[list[int]]
let cube: list[list[list[int]]] = [[[1, 2], [3]], [[4]]]
cube[0][1][0] = 8
for ligne in grille {
    ligne.append(0)
}
```

- `grille[i][j] = v`, `grille[i][j] += v` et `grille[i].append(v)` écrivent dans la ligne, à n'importe
  quelle profondeur (`cube[a][b][c] = v`).
- Une liste de listes n'est pas une `matrix` : ses lignes peuvent avoir des longueurs différentes, et
  `matrix[float]` reste la forme rapide pour le calcul.

## Dictionnaires

```
let carres: dict[int, int] = {}
for i in range(-3, 5) {
    carres[i] = i * i
}
let noms = {1: "un", 2: "deux"}                    // dict[int, string], deviné
if 3 in carres {
    print(carres[3])
}
for k in carres {                                   // k est un int
    print(k)
}

let villes: dict[string, Ville] = {}
let groupes: dict[string, list[int]] = {}
groupes["pairs"] = []
groupes["pairs"].append(2)
let notes: dict[string, dict[string, int]] = {}
notes["léa"] = {}
notes["léa"]["maths"] = 15
```

- Les clés sont des textes ou, nouveau, des entiers, de -9223372036854775808 à 9223372036854775807.
- Les valeurs peuvent être des nombres, des booléens, des textes, et maintenant des objets, des listes
  ou des dictionnaires.

## Ce qui n'est pas encore là

- `p in ps` sur une liste d'objets ou de listes est refusé avec un message. Python compare avec `==`,
  que Clair n'a pas encore pour les objets ; une boucle fait la même chose.
- Comparer deux listes avec `==` reste refusé, comme avant.
- Une liste rangée dans une autre (ou dans un objet, ou dans un dictionnaire) vit jusqu'à la fin du
  programme. Avant la V0.74, c'était déjà le cas des listes rangées dans un objet.

## Aussi dans cette version

- **`let xs: list[Inconnue] = []`** : le type d'une variable est maintenant vérifié ; avant, une
  classe qui n'existait pas n'était vue que par Clang.
- **Correctif de la V0.73.** Une fonction sans type qui s'appelle elle-même comme une instruction ne
  pouvait pas être devinée. Exemple :
  `def feuilles(n: Noeud, acc: list[string]) { ... feuilles(e, acc) }`. Elle rend maintenant rien,
  comme il se doit.

## Comment c'est fait

- `src/parser.c` :
  - `list[Point]`, `list[list[int]]`, `dict[int, string]`, `dict[string, list[int]]` sont lus par
    `parse_inner_type` ;
  - `grille[i][j][k] = v` et `grille[i].append(v)` acceptent toute suite de champs et d'indices.
- `src/semantic.c` :
  - les types de listes et de dictionnaires ne sont plus une liste fixe : `list_element_type`,
    `dict_value_type` et `dict_key_type` lisent le nom du type ;
  - `grille[i][j] = v` sur une liste devient une écriture dans la ligne `grille[i]`.
- `src/codegen_c.c` :
  - une liste d'objets ou de listes garde des pointeurs ;
  - `clair_rt_list_at_any_checked` lit n'importe quel élément, avec la même vérification des
    indices ;
  - un dictionnaire à clés entières garde ses clés sous forme de texte (`clair_rt_ikey`), ce qui lui
    laisse l'ordre d'arrivée des clés, comme Python.

## Tests

- `listes_objets.cl` :
  - listes de `Point`, tranches, `[p] * 3` ;
  - un arbre de `Noeud` à 16 feuilles, des matrices en listes de listes, une transposée ;
  - des listes à trois niveaux, des compréhensions, un `dict[int, list[Point]]`.
- `dicts_listes.cl` :
  - le mémo de `fib(80)` dans un `dict[int, int]`, des clés négatives et extrêmes ;
  - des dictionnaires d'objets, de listes et de dictionnaires ;
  - une liste qui grandit pendant qu'on la parcourt.
- Les deux sont comparés à la sortie du même programme en Python (`tests/*.py`). Ils sont aussi
  compilés avec AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur de mémoire. Valgrind,
  dans mon bac à sable, ne voit aucune fuite.
- `invalid_in_objects.cl` : le refus de `in` sur une liste d'objets.
- `invalid_list_type.cl` : il vérifiait que `list[list[int]]` était refusé. Il vérifie maintenant
  qu'une liste d'une classe inconnue l'est.
- `types_devines.cl` : la fonction récursive sans valeur.
