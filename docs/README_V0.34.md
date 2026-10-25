# Clair V0.34

## `float32` : les nombres à virgule de l'IA

```text
def axpy(a: float32, x: list[float32], y: list[float32]) -> int {
    for i in range(len(x)) {
        y[i] += a * x[i]
    }
    return 0
}

def main() -> int {
    let w: matrix[float32] = matrix(1000, 1000)   // 4 Mo au lieu de 8
    let a: float32 = 0.1
    print(a * 3)                                   // 0.3
    return 0
}
```

Un `float32` tient sur 4 octets au lieu de 8, avec environ 7 chiffres exacts au lieu de 15.
C'est le format courant pour entraîner et faire tourner les réseaux de neurones (NumPy,
PyTorch).

- **Deux fois moins de mémoire** : `list[float32]`, `matrix[float32]`.
- **Deux fois plus de calculs par instruction** : une instruction NEON du téléphone traite
  4 `float32` au lieu de 2 `float`. Le rapport compare maintenant `axpy` et `axpy32`, le même
  programme en `float` et en `float32`.

Les `float` (64 bits) ne changent pas.

### Les règles, celles de NumPy

- `float32` avec `float32` donne un `float32` ;
- `float32` avec un `int` donne un `float32` ;
- `float32` avec un `float` donne un `float`, sans rien perdre ;
- un nombre écrit dans le programme à côté d'un `float32` est un `float32` : `a * 0.5`,
  `a < 0.1`, `max(a, 0.0)` se calculent en `float32` ;
- `let x: float32 = valeur_float` arrondit, comme NumPy ; `float32(x)` et `float(x)`
  convertissent ; `int(x)` coupe vers zéro et vérifie que le nombre tient ;
- `sqrt`, `exp`, `tanh`, `abs`, `min`, `max`, `pow`... d'un `float32` se calculent en `float32`
  (`sqrtf`, `expf` en C) ;
- `print` écrit le moins de chiffres qui redonnent exactement le même nombre, comme NumPy :
  `0.1` et non `0.100000001`.

Les résultats sont identiques au bit près à ceux de NumPy en `float32`. Seules `exp`, `tanh`,
`log`... peuvent différer d'une unité sur le dernier chiffre, selon la bibliothèque
mathématique du système.

### Le C généré

```c
static int64_t clair_fn_axpy(float a, ClairList x, ClairList y) {
    ...
            float *clair_data_y = (float *)y->data;
            float *clair_data_x = (float *)x->data;
            for (int64_t i = 0; i < clair_stop_i; i++) {
                clair_data_y[i] = clair_data_y[i] + a * clair_data_x[i];
            }
```

Un nombre écrit devient `0.1f`, ou `(float)0.1` dans les rares cas où lire le nombre
directement en `float` ne donnerait pas le même arrondi que NumPy.

Pour l'instant, les produits de matrices et les couches denses en `float32` passent par les
boucles rapides ordinaires, vectorisées par Clang. Les noyaux en blocs et le calcul sur
plusieurs cœurs viendront dans une prochaine version.

## Tests

- `float32.cl` : calculs mélangés avec `int`, `float` et nombres écrits, comparaisons,
  conversions, arrondi de 16777217, fonctions, listes, boucles rapides, tranches, `enumerate`,
  produit de matrices, couche dense dans une classe ; sortie identique à NumPy ;
- `invalid_float32_to_int.cl` ;
- 1900 programmes tirés au hasard (calculs entre `float32`, `float`, `int` et nombres écrits,
  rangements dans des `float32`, comparaisons) : environ 15 000 valeurs affichées, toutes identiques à
  NumPy en `float32` ;
- les neuf programmes du rapport donnent exactement le même assembleur ARM qu'en V0.33 ;
- les fuzzers des versions précédentes : toujours identiques.
