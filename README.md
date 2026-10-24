# Clair

Clair est un langage de programmation expérimental.
Son compilateur est écrit en C et utilise Clang.

## Fonctionnalités

- Types int, float, bool et string
- Fonctions mathématiques (`sqrt`, `exp`, `log`, `tanh`, `pow`, `abs`, `min`, `max`…), conversions `int()` et `float()`, opérateur `%`, notation `1e-3`
- Fonctions, conditions et boucles, dont `for v in liste` comme en Python
- Listes et dictionnaires typés
- Matrices `matrix[float]` et `matrix[int]` : `m[i][j]`, `rows(m)`, `cols(m)`, un seul bloc mémoire comme en C
- Classes et objets, avec méthodes (`self`)
- Écriture directe dans les champs : `self.w[i][j] = x`, `self.items.append(x)`
- Exemple complet : réseau de neurones entraîné sur XOR (`tests/xor_training.cl`)
- Appels de fonctions et de méthodes en instruction, mutation chaînée (`team.leader.age = 40`)
- Boucles rapides (`for` et `while`) : bornes vérifiées une fois avant la boucle, puis code vectorisable par Clang
- Doubles boucles (produit de matrices) vérifiées une seule fois avant la boucle extérieure
- Produit de matrices reconnu et calculé en blocs : plus rapide que la triple boucle en C, résultat identique au bit près
- Option `--sans-verif` pour les longs calculs déjà testés : aucune vérification de bornes
- C généré lisible, proche du C écrit à la main
- Optimisations de listes sur la pile
- Optimisations SIMD ARM64 NEON
- Division entière contrôlée

## Compilation sous Termux

Installer Clang, CMake et Make.

```sh
cmake -S . -B build
cmake --build build -j2
```

## Utilisation

```sh
./build/clair hello.cl -o hello
./hello
```

Pour un long calcul déjà testé, `--sans-verif` enlève les vérifications de bornes (un indice faux
n'est alors plus détecté) :

```sh
./build/clair calcul.cl -o calcul --sans-verif
```

## Rapport du code machine

```sh
bash tools/rapport_code_machine.sh
```

Compare huit programmes types avec leur version écrite à la main en C : vectorisation de la boucle chaude des deux côtés, temps et écart en %. Clair et C tournent sur le même cœur, le plus rapide, et le temps médian de 5 lancements est gardé.

## Tests

```sh
bash tests/run_tests.sh
```

## Statut

Clair est en développement.
SIMD V4 est encore expérimental.
Les validations principales sont réalisées sous Termux ARM64.
Le test SIMD V4 (assembleur NEON) est ignoré automatiquement hors ARM64.

## Documentation

- [Clair V0.25](docs/README_V0.25.md) : produit de matrices en blocs, rapport en temps médian
- [Clair V0.24](docs/README_V0.24.md) : `for v in liste`, rapport mesuré sur un seul cœur
- [Clair V0.23](docs/README_V0.23.md) : doubles boucles vérifiées une seule fois, option `--sans-verif`
- [Clair V0.22](docs/README_V0.22.md) : boucles `while` rapides, rapport du code machine plus fiable
- [Clair V0.21](docs/README_V0.21.md) : matrices à largeur fixe vectorisées (apprentissage XOR)
- [Clair V0.20](docs/README_V0.20.md) : écriture dans les champs, boucles rapides sur `self.w`, apprentissage de XOR
- [Clair V0.19](docs/README_V0.19.md) : matrices, couche dense d'un réseau de neurones
- [Clair V0.18](docs/README_V0.18.md) : fonctions mathématiques, conversions, `%`, notation scientifique
- [Clair V0.17](docs/README_V0.17.md) : boucles rapides, C lisible, rapport du code machine
- [Clair V0.16](docs/README_V0.16.md) : méthodes, appels en instruction, mutation chaînée
- [Clair V0.14](docs/README_V0.14.md) : classes (documentation historique)
