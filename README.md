# Clair

Clair est un langage de programmation expérimental.
Son compilateur est écrit en C et utilise Clang.

## Fonctionnalités

- Types int, float, bool et string ; `float32` comme en IA (NumPy), deux fois moins de mémoire et 4 nombres par instruction NEON
- Entiers machine `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32` (`i64` = `int`) qui font le tour comme en C et NumPy ; opérations sur les bits `& | ^ ~ << >>` ; nombres `0xFF`, `0b1010`, `0o755`, `1_000_000`
- Grands entiers exacts `int128`, `int256`, `int512`, `int1024` (jusqu'à 308 chiffres) et sans signe `uint128` .. `uint1024`, avec `abs`, `min`, `max`, `pow` exacts ; dépassements détectés
- Nombres aléatoires `seed`, `random`, `randint`, `uniform`, `gauss`, `shuffle`, `choice` : mêmes tirages que Python pour la même graine
- Fonctions mathématiques (`sqrt`, `exp`, `log`, `tanh`, `pow`, `abs`, `min`, `max`…), conversions `int()` et `float()`, opérateur `%`, notation `1e-3`
- Fonctions, conditions et boucles, dont `for v in liste` et `for i, v in enumerate(liste)` comme en Python
- `x += v`, `-=`, `*=`, `/=`, `%=` ; tranches `xs[a:b:pas]` sur les listes et les chaînes, avec les règles de Python
- Listes et dictionnaires typés
- Matrices `matrix[float]`, `matrix[int]`, `matrix[u8]`... : `m[i][j]`, `rows(m)`, `cols(m)`, un seul bloc mémoire comme en C
- Classes et objets, avec méthodes (`self`)
- Écriture directe dans les champs : `self.w[i][j] = x`, `self.items.append(x)`
- Exemple complet : réseau de neurones entraîné sur XOR (`tests/xor_training.cl`)
- Appels de fonctions et de méthodes en instruction, mutation chaînée (`team.leader.age = 40`)
- Boucles rapides (`for` et `while`) : bornes vérifiées une fois avant la boucle, puis code vectorisable par Clang
- Doubles boucles (produit de matrices) vérifiées une seule fois avant la boucle extérieure
- Produit de matrices reconnu et calculé en blocs : plus rapide que la triple boucle en C, résultat identique au bit près
- Couche dense (matrice × vecteur) calculée 8 lignes à la fois : environ 2 fois plus rapide, résultat identique au bit près
- Grands produits de matrices et grandes couches denses (activation comprise) sur tous les cœurs, en `float` et en `float32`, même résultat quel que soit le nombre de cœurs (`CLAIR_THREADS=1` pour un seul)
- Option `--sans-verif` pour les longs calculs déjà testés : aucune vérification de bornes
- Listes et matrices rendues pendant le programme quand plus rien ne les utilise (fin de bloc, `break`, `return`, remplacement)
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

Les grands produits de matrices et les grandes couches denses utilisent tous les cœurs. Pour en
choisir le nombre :

```sh
CLAIR_THREADS=1 ./calcul
```

Les calculs à virgule donnent le même résultat partout, comme en Python. `--fma` laisse Clang
fusionner `a * b + c` (un peu plus rapide sur ARM, dernier chiffre parfois différent).

## Rapport du code machine

```sh
bash tools/rapport_code_machine.sh
```

Compare treize programmes types avec leur version écrite à la main en C : vectorisation de la boucle chaude des deux côtés, temps et écart en %. Clair et C tournent sur le même cœur, le plus rapide, et le temps médian de 5 lancements est gardé. Un second tableau donne le gain des programmes de matrices sur tous les cœurs.

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

- [Clair V0.37](docs/README_V0.37.md) : nombres aléatoires (`seed`, `random`, `randint`, `uniform`, `gauss`, `shuffle`, `choice`), mêmes tirages que Python ; couches denses 8 lignes à la fois
- [Clair V0.36](docs/README_V0.36.md) : produits de matrices et couches denses `float32` en blocs, sur tous les cœurs
- [Clair V0.35](docs/README_V0.35.md) : entiers machine `u8` .. `u64` et `i8` .. `i32`, opérations sur les bits, nombres en hexadécimal, binaire et octal
- [Clair V0.34](docs/README_V0.34.md) : `float32`, listes et matrices `float32`, résultats identiques à NumPy
- [Clair V0.33](docs/README_V0.33.md) : `+=` et ses cousins, `enumerate`, tranches `xs[a:b:pas]`
- [Clair V0.32](docs/README_V0.32.md) : entiers sans signe `uint128` .. `uint1024`, `abs`, `min`, `max`, `pow` exacts sur les grands entiers
- [Clair V0.31](docs/README_V0.31.md) : toute la couche dense sur tous les cœurs (sigmoïde comprise), partage plus léger
- [Clair V0.30](docs/README_V0.30.md) : grands produits de matrices et couches denses sur tous les cœurs
- [Clair V0.29](docs/README_V0.29.md) : boucles alignées, même vitesse quelle que soit la place du code
- [Clair V0.28](docs/README_V0.28.md) : mémoire rendue pendant le programme, rapport plus long
- [Clair V0.27](docs/README_V0.27.md) : couche dense calculée 4 lignes à la fois, calculs à virgule identiques partout
- [Clair V0.26](docs/README_V0.26.md) : grands entiers int128 à int1024, nombres trop grands pour int refusés
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
