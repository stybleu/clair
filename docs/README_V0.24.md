# Clair V0.24

## `for v in liste`

La boucle la plus courante de Python existe maintenant en Clair :

```text
let total: int = 0
for v in x {
    if v > seuil {
        total = total + v
    }
}
```

Elle marche avec `list[int]`, `list[float]`, `list[bool]` et `list[string]`, avec `break` et
`continue`, et sur n'importe quelle liste : une variable, un champ (`for v in self.items`), le
résultat d'un appel (`for v in make(5)`) ou une liste écrite directement (`for v in [1, 2, 3]`).

### Comme en Python

- la liste est évaluée une seule fois, au début : remplacer la variable dans la boucle
  (`x = autre`) ne change pas la liste parcourue ;
- `v` prend `x[0]`, `x[1]`… tant que l'indice est plus petit que `len(x)` : si la boucle modifie
  un élément qui n'a pas encore été lu, elle voit la nouvelle valeur ; si elle ajoute des éléments
  à `x`, elle les parcourt aussi ;
- changer `v` dans la boucle ne change pas la liste.

Une seule différence avec Python : `v` n'existe que dans la boucle, comme `i` dans
`for i in range(n)`.

### Le C généré

Quand rien dans la boucle ne peut ajouter d'élément à une liste (cas le plus courant), la longueur
et l'adresse des données ne peuvent pas changer. Elles sont lues une fois, et la boucle n'a aucune
vérification, puisque l'indice ne peut pas sortir de la liste :

```c
int64_t total = 0;
{
    int64_t *clair_items_v = (int64_t *)x->data;
    int64_t clair_count_v = (int64_t)x->len;
    for (int64_t clair_k_v = 0; clair_k_v < clair_count_v; clair_k_v++) {
        int64_t v = clair_items_v[clair_k_v];
        if (v > seuil) {
            total = total + v;
        }
    }
}
```

Clang vectorise cette boucle en NEON exactement comme le C écrit à la main (nouveau programme
`forin` dans le rapport : même temps que le C).

Si la boucle peut ajouter des éléments, la longueur et les données sont relues à chaque tour :

```c
for (int64_t clair_k_v = 0; clair_k_v < (int64_t)grow->len; clair_k_v++) {
    int64_t v = ((int64_t *)grow->data)[clair_k_v];
    ...
}
```

### Messages d'erreur

```text
for ... in needs a list, got int
for ... in needs a list, got matrix[float]: use for i in range(rows(m))
```

## Rapport du code machine : un seul cœur

Un téléphone a des cœurs rapides et des cœurs lents, et déplace les programmes de l'un à l'autre.
Dans le rapport V0.23, le même programme C a mesuré 0,212 s sur une ligne et 0,166 s sur une autre,
alors que Clair restait stable : l'écart de +27 % de `while` venait très probablement de là, pas de
Clair.

Le rapport fait maintenant tourner Clair et C sur le même cœur, le plus rapide
(`tools/pin_core.c`). La deuxième ligne du rapport dit lequel, par exemple :

```text
coeur 7 (3.19 GHz) pour Clair et C
```

Si le système refuse de choisir le cœur, le rapport l'indique et mesure comme avant.

## Tests

- `for_each.cl` : tous les types, `break`, `continue`, boucles imbriquées, liste qui grandit,
  liste remplacée, champ remplacé, élément modifié, liste vide, appel ; résultat identique à
  Python ;
- `invalid_for_each_type.cl`, `invalid_for_each_matrix.cl`, `invalid_for_each_element.cl` ;
- 600 programmes `for v in` tirés au hasard (listes qui grandissent, remplacées, modifiées,
  `break`, `continue`, boucles imbriquées) donnent exactement la même sortie que Python, erreurs
  d'indice comprises ;
- tous les programmes existants produisent exactement le même C qu'en V0.23.
