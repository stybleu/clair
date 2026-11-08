# Clair V0.39

## Les briques de Python : `xs[-1]`, `x in xs`, `[... for ...]`, `for k in dico`

```text
def main() -> int {
    let xs: list[int] = [3, 1, 4, 1, 5, 9, 2, 6]
    print(xs[-1])                                   // 6, le dernier
    print(9 in xs)                                  // 1
    let carres: list[int] = [x * x for x in xs if x % 2 == 0]
    let grille: list[float] = [float(i) / 10.0 for i in range(10)]
    let stock: dict[string,int] = {"pommes": 3, "poires": 5}
    for nom in stock {
        print(nom)
    }
    return 0
}
```

### Indices négatifs

Comme en Python, `xs[-1]` est le dernier élément, `xs[-2]` l'avant-dernier, en lecture comme en
écriture (`xs[-1] = 0`), avec un nombre écrit ou une variable. Au-delà du début de la liste, le
programme s'arrête comme avant : `list index -9 out of bounds for length 8`. Les boucles rapides
ne changent pas : un indice dont on prouve qu'il reste dans la liste n'est jamais vérifié.

### `in` et `not in`

- `x in xs` : `x` est-il dans la liste ? (`2 in fs` marche avec une `list[float]`) ;
- `"ra" in "grace"` : un morceau de texte dans une chaîne ;
- `"pommes" in stock` : une clé dans un dictionnaire ;
- `x not in xs` est l'inverse. Comme en Python, `in` a la priorité des comparaisons.

Un mélange qui perdrait quelque chose est refusé : `1.5 in xs` avec une `list[int]` demande de
convertir d'abord.

### Listes en compréhension

```text
[expression for v in liste]
[expression for v in liste if condition]
[expression for i in range(a, b, pas) if condition]
[expression for cle in dico]
```

Le résultat est une nouvelle liste de nombres, de booléens ou de chaînes. Clair en fait une
petite fonction, qui reçoit les variables lues dedans :

```c
static ClairList clair_fn_comprehension_1(ClairList xs) {
    ClairList clair_h_result = clair_rt_list_make(NULL, 0, sizeof(int64_t));
    for (...) {
        ...
        *(int64_t *)clair_rt_list_append_slot(clair_h_result) = x * x;
    }
    return clair_h_result;
}
```

### `for k in dico`

Parcourt les clés dans l'ordre où elles ont été ajoutées, comme Python. Changer la valeur d'une
clé pendant la boucle est permis ; ajouter une clé arrête le programme, comme le `RuntimeError`
de Python : `dictionary changed size during iteration`.

## Tests

- `briques_python.cl` : indices négatifs en lecture et en écriture, `in` et `not in` sur des
  listes d'entiers, de nombres à virgule, de `u8`, de chaînes, sur des chaînes et des
  dictionnaires, compréhensions sur des listes, des `range` à pas, des dictionnaires, des
  champs de classes, avec conditions ; boucle sur un dictionnaire ; sortie identique à Python ;
- `dict_changed_runtime.cl`, `invalid_in_type.cl`, `invalid_in_right.cl`,
  `invalid_comprehension_list.cl`, `invalid_comprehension_cond.cl` ;
- les anciens tests d'indice négatif (`list_negative_index`, `list_set_negative_index`,
  `while_fast_path_bounds`, SIMD V3 et V4) vérifient maintenant un indice négatif *trop grand*,
  qui doit toujours arrêter le programme ;
- 600 programmes tirés au hasard (indices négatifs dans la liste et hors de la liste, `in`,
  compréhensions avec conditions sur des listes et des `range`, boucles sur un dictionnaire
  qui grandit) : sortie identique à Python.
