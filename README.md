# Clair

Clair est un langage de programmation expérimental.
Son compilateur est écrit en C et utilise Clang.

## Fonctionnalités

- Types int, float, bool et string
- Fonctions, conditions et boucles
- Listes et dictionnaires typés
- Classes et objets, avec méthodes (`self`)
- Appels de fonctions et de méthodes en instruction, mutation chaînée (`team.leader.age = 40`)
- Boucles rapides : bornes vérifiées une fois avant la boucle, puis code vectorisable par Clang
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

## Rapport du code machine

```sh
bash tools/rapport_code_machine.sh
```

Compare quatre programmes types avec leur version écrite à la main en C, et indique si Clang a vectorisé chaque boucle.

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

- [Clair V0.17](docs/README_V0.17.md) : boucles rapides, C lisible, rapport du code machine
- [Clair V0.16](docs/README_V0.16.md) : méthodes, appels en instruction, mutation chaînée
- [Clair V0.14](docs/README_V0.14.md) : classes (documentation historique)
