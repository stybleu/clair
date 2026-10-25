# Clair V0.36

## Produits de matrices et couches denses en `float32`, sur tous les cœurs

```text
def couche(w: matrix[float32], b: list[float32], x: list[float32], y: list[float32]) -> int {
    for i in range(rows(w)) {
        let s: float32 = b[i]
        for j in range(cols(w)) {
            s += w[i][j] * x[j]
        }
        y[i] = sigmoid(s)
    }
    return 0
}
```

En V0.34, les matrices `float32` prenaient deux fois moins de mémoire, mais leurs produits et
leurs couches denses passaient par les boucles ordinaires. Ils ont maintenant les mêmes noyaux
que les `float` :

- **produit de matrices** (`c[i][j] += a[i][k] * b[k][j]`, en matrices ou en listes à plat
  `c[i * n + j]`) : calculé en blocs, et en `float32` un bloc de 4 × 16 nombres de `c` reste dans
  les registres (16 registres NEON de 4 `float32`), au lieu de 4 × 8 en `float` ;
- **couche dense** (`s += w[i][j] * x[j]`, puis l'activation) : 4 lignes à la fois ; une grande
  couche est partagée entre tous les cœurs, activation comprise (`sigmoid`, `tanh`, `relu`...
  écrites en `float32`) ;
- les grands produits de matrices sont aussi partagés entre tous les cœurs.

Comme pour les `float`, chaque nombre reçoit exactement les mêmes additions dans le même ordre
que les boucles simples : le résultat est identique au bit près, quel que soit le nombre de
cœurs (`CLAIR_THREADS=1` pour un seul).

### Le C généré

```c
float clair_sums_small[CLAIR_SUMS_SMALL];
float *clair_sums = clair_rt_sums_make_f32(w->rows, clair_sums_small);
...
clair_rt_layer_f32(w->rows, w->cols, clair_layer_couche_1, &clair_layer);
```

Les noyaux `float32` sont les mêmes que ceux des `float`, écrits avec `float` : leurs noms
finissent par `_f32`. Un programme qui n'utilise que des `float` génère exactement le même C
qu'en V0.35.

### Le rapport

Deux programmes de plus, `matmul32` et `couche32` : les mêmes que `matmul` et `couche`, en
`float32`, comparés au même programme écrit en C. Ils sont aussi dans le tableau des cœurs.

## Tests

- `noyaux_float32.cl` : produits de matrices 130 × 70 × 150 (en matrices et en listes à plat,
  tailles qui ne tombent pas juste sur les blocs), couche dense 300 × 257 avec `tanh` et
  `relu`, couche dont le reste affiche des valeurs (sommes seules sur les cœurs) ; sortie
  identique au bit près à la V0.34 (boucles simples), avec 1, 2, 3 ou 7 cœurs ; sans erreur
  d'AddressSanitizer, d'UndefinedBehaviorSanitizer ni de ThreadSanitizer ;
- les fuzzers de produits de matrices et de couches denses, passés en `float32` : programmes
  tirés au hasard (formes qui ne vont pas, matrices qui se recouvrent, presque-motifs, tailles
  pour plusieurs cœurs) ; sortie identique à la V0.34 ;
- les onze programmes du rapport : C généré identique à la V0.35.
