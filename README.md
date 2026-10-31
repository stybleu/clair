# Clair

Clair est un langage de programmation expérimental.
Son compilateur est écrit en C et utilise Clang.

## Fonctionnalités

- Types int, float, bool et string ; `float32` comme en IA (NumPy), deux fois moins de mémoire et 4 nombres par instruction NEON
- Entiers machine `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32` (`i64` = `int`) qui font le tour comme en C et NumPy ; opérations sur les bits `& | ^ ~ << >>` ; nombres `0xFF`, `0b1010`, `0o755`, `1_000_000`
- Grands entiers exacts `int128`, `int256`, `int512`, `int1024` (jusqu'à 308 chiffres) et sans signe `uint128` .. `uint1024`, avec `abs`, `min`, `max`, `pow` exacts ; dépassements détectés
- `str(x)` et `format(x, ".9f")` comme en Python
- Fichiers CSV de nombres (`load_csv`, `save_csv`), lignes de texte (`read_lines`, `split`, `strip`), `int("42")` et `float("2.5")` comme en Python
- Entrée et sortie comme en Python : `input()`, `read_lines()`, `read_text()`, `read_bytes()` (l'entrée ou un fichier), `write(s)` sans aller à la ligne, `write(octets)`, `write_file` ; octets `list[u8]` avec `encode`, `decode`, `ord`, `chr`
- Listes toutes faites : `[0] * n`, `[1, 2] * 3`, comme en Python
- Unités vérifiées à la compilation : `float[m]`, `float[m/s^2]`, `9.81[m/s^2]` ; `m + s` est refusé avant de lancer le programme, et le C reste le même (aucun coût) ; les constantes de `phys` et `astro` ont leur unité
- Mesures avec incertitude : `9.81 ± 0.02` (ou `+/-`), `measure[m/s^2]` ; Clair propage l'incertitude dans tout le calcul (corrélations comprises : `x - x` vaut `0 ± 0`), et les constantes de `phys` gardent leur incertitude de CODATA 2022
- Chaque erreur de compilation dit sa ligne (et son fichier dans un module) : `semantic error: undefined variable 'y' (line 5)`
- `import math as m`, `from astro import GM_mars`, `import orbite` (un fichier `orbite.cl`) comme en Python ; modules `math`, `phys` (constantes CODATA 2022) et `astro` (UAI, éphéméride DE440) rangés dans le compilateur, sans coût à l'exécution
- Nombres aléatoires `seed`, `random`, `randint`, `uniform`, `gauss`, `shuffle`, `choice` : mêmes tirages que Python pour la même graine
- Fonctions mathématiques (`sqrt`, `exp`, `log`, `tanh`, `pow`, `abs`, `min`, `max`…), conversions `int()` et `float()`, opérateur `%`, notation `1e-3`
- Fonctions, conditions et boucles, dont `for v in liste`, `for i, v in enumerate(liste)` et `for k in dico` comme en Python
- `xs[-1]`, `x in xs`, `x not in xs`, listes en compréhension `[x * x for x in xs if x > 0]`
- `x += v`, `-=`, `*=`, `/=`, `%=` ; tranches `xs[a:b:pas]` sur les listes et les chaînes, avec les règles de Python
- Listes et dictionnaires typés
- Matrices `matrix[float]`, `matrix[int]`, `matrix[u8]`... : `m[i][j]`, `rows(m)`, `cols(m)`, un seul bloc mémoire comme en C
- Classes et objets, avec méthodes (`self`)
- Boucles sans fuite : les textes et les objets fabriqués pendant un tour de boucle sont rendus à la fin du tour quand rien ne les garde (lire un gros fichier ligne par ligne, compter des mots) ; les dictionnaires copient leurs clés
- Fonctions sans fuite : une liste faite par une fonction et utilisée tout de suite (`somme(paire(i))`, `paire(i)[1]`, `for v in carres(n)`, `x in voisins(k)`) est rendue à la mémoire dès la fin de l'instruction
- Classes rapides : une classe dont les champs ne changent jamais devient une structure du C, copiée comme un nombre ; un objet qui ne sort pas de sa fonction est rangé sur la pile ; aucune mémoire prise dans les boucles
- Écriture directe dans les champs : `self.w[i][j] = x`, `self.items.append(x)`
- Exemple complet : réseau de neurones entraîné sur XOR (`tests/xor_training.cl`)
- Appels de fonctions et de méthodes en instruction, mutation chaînée (`team.leader.age = 40`)
- Boucles rapides (`for` et `while`) : bornes vérifiées une fois avant la boucle, puis code vectorisable par Clang ; aussi les boucles triangulaires, à deux bouts, et les indices qui montent et descendent (piles, retours en arrière)
- Doubles boucles (produit de matrices) vérifiées une seule fois avant la boucle extérieure
- Produit de matrices reconnu et calculé en blocs : plus rapide que la triple boucle en C, résultat identique au bit près
- Couche dense (matrice × vecteur) calculée 8 lignes à la fois : environ 2 fois plus rapide, résultat identique au bit près
- Grands produits de matrices et grandes couches denses (activation comprise) sur tous les cœurs, en `float` et en `float32`, même résultat quel que soit le nombre de cœurs (`CLAIR_THREADS=1` pour un seul)
- Option `--sans-verif` pour les longs calculs déjà testés : aucune vérification de bornes
- Blocs `absolue { }` : la mémoire comme en C (`ptr[float32]`, `p[i]`, `p + k`, `address(xs)`, `alloc(n, 64)` aligné, `copy`, `fill`, `free`, `bytes`), sans aucune vérification, et `absolue def`
- Listes et matrices rendues pendant le programme quand plus rien ne les utilise (fin de bloc, `break`, `return`, remplacement)
- Jeux de tests des langages (n-body, spectral-norm, fannkuch-redux, pidigits, fasta, reverse-complement, nqueen, matmul de PLB2) comparés au C dans le rapport
- C généré lisible, proche du C écrit à la main
- Optimisations de listes sur la pile
- Optimisations SIMD ARM64 NEON
- Optimisation DCE (`--dce`) : suppression des fonctions, méthodes, classes et de certaines variables inutilisées ([documentation](docs/README_DCE.md))
- Différentiation automatique (--ad) : dérivées et gradients, compatible --dce.
- Division entière contrôlée

- Grands nombres dynamiques `bigint` et `bigfloat` (exacts, sans limite de taille), mélangés aux `int` comme en Python (`num * k + acc`), avec leurs fonctions mathématiques (`sqrt`, `sin`, `exp`, `log`, `pi(1000)`, `isqrt`, `gcd`...), mémoire rendue pendant le programme, même dans les fonctions et les méthodes

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
- [Différentiation automatique](docs/README_AD_V4_25.md) : option --ad.
- [Optimisation DCE](docs/README_DCE.md) : option --dce.

- [Clair V0.54](docs/README_V0.54.md) : une boucle ne garde plus ses textes : la mémoire d'un tour est rendue à la fin du tour ; les dictionnaires copient leurs clés
- [Clair V0.53](docs/README_V0.53.md) : mesures avec incertitude (`9.81 ± 0.02`, `measure[m]`), propagée dans tout le calcul ; les constantes de `phys` avec leur incertitude CODATA 2022
- [Clair V0.52](docs/README_V0.52.md) : les listes faites par les fonctions et utilisées tout de suite sont rendues à la mémoire : une boucle ne grandit plus
- [Clair V0.51](docs/README_V0.51.md) : classes valeur (structures du C) et objets sur la pile : aussi rapide que le C, plus de mémoire prise dans les boucles
- [Clair V0.50](docs/README_V0.50.md) : calculs certifiés avec certfloat, certsqrt(), certdigits() et mémoire améliorée
- [Clair V0.49](docs/README_V0.49.md) : chaque erreur de compilation dit sa ligne, et son fichier dans un module
- [Clair V0.48](docs/README_V0.48.md) : unités vérifiées à la compilation (`float[m/s]`, `9.81[m/s^2]`, `N`, `J`, `W`...), sans coût à l'exécution
- [Clair V0.47](docs/README_V0.47.md) : `import`, `from ... import`, `as`, modules en fichiers `.cl`, modules `math`, `phys`, `astro` ; `cbrt`, `erf`, `gamma`, `degrees`...
- [Clair V0.46](docs/README_V0.46.md) : `input()`, `read_text`, `read_bytes`, `write`, `write_file`, octets (`encode`, `decode`, `ord`, `chr`), `[0] * n`, jeux fasta et reverse-complement
- [Clair V0.45](docs/README_V0.45.md) : `sqrt`, `exp`, `log`, `sin`, `cos`, `atan2`, `pow`, `pi(chiffres)`... pour les `bigfloat`, `isqrt`, `gcd`, `pow(b, e, m)` pour les `bigint`, comparés à mpmath
- [Clair V0.44](docs/README_V0.44.md) : indices qui montent et descendent prouvés une fois (nqueen), `bigint` avec `int` comme en Python, grands nombres plus rapides
- [Clair V0.43](docs/README_V0.43.md) : jeux de tests des langages (Benchmarks Game, PLB2) dans le rapport, `str()`, `format()`, `bigint(k)`, boucles triangulaires et à deux bouts vérifiées une fois
- [Clair V0.42](docs/README_V0.42.md) : mémoire des `bigint` et `bigfloat` rendue dans toutes les fonctions, `clair` depuis n'importe quel dossier, `let x = x + 1` dans un bloc
- [Clair V0.41](docs/README_V0.41.md) : blocs `absolue { }`, `ptr[T]`, `address`, `alloc` aligné, `copy`, `fill`, `free`, `bytes`, `absolue def`
- [Clair V0.40](docs/README_V0.40.md) : grands entiers dynamiques, flottants decimaux precis (experimental)
- [Clair V0.39](docs/README_V0.39.md) : `xs[-1]`, `x in xs`, listes en compréhension `[x * x for x in xs if ...]`, `for k in dico`
- [Clair V0.38](docs/README_V0.38.md) : fichiers CSV (`load_csv`, `save_csv`), `read_lines`, `split`, `strip`, `int()` et `float()` de chaînes, `\t` et `\n` dans les chaînes
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

## Version V0.50

- [Documentation V0.50](docs/README_V0.50.md)
- [Rapport d'audit V0.50](docs/AUDIT_V0.50.md)
