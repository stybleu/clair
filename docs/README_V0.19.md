# Clair V0.19

Les matrices, pour écrire `m[i][j]` au lieu de `a[i * n + j]`, avec la vitesse du C écrit à la
main.

## Utilisation

```text
let w: matrix[float] = matrix(3, 4)   // 3 lignes, 4 colonnes, remplie de 0
w[0][1] = 2.5
print(w[0][1])
print(rows(w))   // 3
print(cols(w))   // 4

let ids: matrix[int] = matrix(2, 2)
```

- deux types : `matrix[float]` et `matrix[int]` ;
- `matrix(lignes, colonnes)` prend son type du contexte (`let`, paramètre, champ, `return`) ;
- une matrice se passe aux fonctions, se renvoie et peut être un champ de classe. Comme les
  listes, c'est une référence : `let alias: matrix[float] = m` partage les mêmes données ;
- un indice hors limites arrête le programme :
  `matrix index [2][0] out of bounds for shape 2x2`.

## Exemple : couche dense d'un réseau de neurones

```text
Class Dense {
    w: matrix[float]
    b: list[float]

    def forward(x: list[float], y: list[float]) -> int {
        for i in range(rows(self.w)) {
            let s: float = self.b[i]
            let w: matrix[float] = self.w
            for j in range(cols(w)) {
                s = s + w[i][j] * x[j]
            }
            y[i] = 1.0 / (1.0 + exp(-s))
        }
        return 0
    }
}
```

## Dans le C généré

Une matrice est un seul bloc de mémoire contigu, ligne après ligne, alloué avec `calloc` comme le
ferait du C écrit à la main. Dans une boucle rapide (voir V0.17), `c[i][j]` devient directement :

```c
clair_data_c[i * clair_cols_c + j] = clair_data_c[i * clair_cols_c + j] + aik * clair_data_b[k * clair_cols_b + j];
```

Les deux indices sont vérifiés une seule fois avant la boucle (`clair_rt_affine_in`). Clang
vectorise alors le produit de matrices en NEON (`fmla v.2d`), comme la version écrite à la main.
Le rapport `bash tools/rapport_code_machine.sh` compare maintenant aussi ce produit de matrices
(`matrice`) avec le même code écrit à la main en C.

## Limites connues

- `self.w[i][j] = x` n'est pas encore possible : passer par une variable locale
  (`let w: matrix[float] = self.w` puis `w[i][j] = x`), comme pour les listes ;
- `m[i]` seul (une ligne entière) n'est pas une valeur : écrire `m[i][j]` ;
- pas de `len(m)` : utiliser `rows(m)` et `cols(m)`.
