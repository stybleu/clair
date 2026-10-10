# Clair

Clair est un langage de programmation expérimental.
Son compilateur est écrit en C et utilise Clang.

## Fonctionnalités

- Types int, float, bool et string
- Fonctions, conditions et boucles
- Listes et dictionnaires typés
- Classes et objets
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

## Tests

```sh
bash tests/run_tests.sh
```

## Statut

Clair est en développement.
SIMD V4 est encore expérimental.
Les validations principales sont réalisées sous Termux ARM64.

## Documentation historique

[Clair V0.14](docs/README_V0.14.md)
