# Clair V0.37

## Nombres aléatoires, comme le module `random` de Python

```text
def main() -> int {
    seed(2026)
    let w: matrix[float32] = matrix(64, 64)
    for i in range(64) {
        for j in range(64) {
            w[i][j] = gauss(0.0, 0.125)      // poids d'une couche, loi normale
        }
    }
    let ordre: list[int] = [0, 1, 2, 3, 4, 5, 6, 7]
    shuffle(ordre)                           // exemples dans un ordre au hasard
    print(randint(1, 6))
    print(random())
    return 0
}
```

| fonction          | donne                                                        |
|-------------------|--------------------------------------------------------------|
| `seed(n)`         | choisit la graine : les mêmes tirages à chaque lancement     |
| `random()`        | un `float` entre 0 (compris) et 1 (non compris)              |
| `randint(a, b)`   | un `int` de `a` à `b`, les deux compris                      |
| `uniform(a, b)`   | un `float` entre `a` et `b`                                  |
| `gauss(mu, sigma)`| un `float` tiré selon la loi normale de moyenne `mu`, d'écart type `sigma` |
| `shuffle(xs)`     | mélange la liste sur place (tous types de listes)            |
| `choice(xs)`      | un élément de la liste, tiré au hasard                       |

**Mêmes tirages que Python** : Clair utilise le même générateur que Python (le Mersenne
Twister) et les mêmes façons de tirer. Avec `seed(2026)`, un programme Clair donne exactement
les mêmes nombres que le même programme Python avec `random.seed(2026)` : on peut comparer les
deux, ou reprendre en Clair un calcul commencé en Python.

- sans `seed()`, les tirages changent à chaque lancement, comme en Python ;
- `seed(-n)` donne les mêmes tirages que `seed(n)`, comme en Python ;
- `randint(5, 2)` arrête le programme (`randint() empty range`), `choice` d'une liste vide
  aussi (`choice() from an empty list`) ;
- `let w: float32 = gauss(0.0, 0.1)` arrondit en `float32`, comme NumPy.

Le générateur est rapide (quelques milliardièmes de seconde par nombre) : remplir une matrice
de 1000 × 1000 poids prend quelques millisecondes.

## Couches denses : 8 lignes à la fois

Le noyau des couches denses (`float` et `float32`) calcule maintenant 8 lignes à la fois au
lieu de 4 : 8 sommes indépendantes avancent ensemble, et le processeur n'attend plus la fin
d'une addition pour commencer la suivante. Chaque ligne garde ses additions dans le même ordre :
le résultat reste identique au bit près.

## Tests

- `hasard.cl` : graine, `random`, `randint` (petites et très grandes plages, nombres
  négatifs), `uniform`, `gauss`, 6000 lancers de dé, `shuffle` de listes d'entiers, de chaînes
  et de `u8`, `choice`, poids `float32` d'une couche ; sortie identique à Python ;
- `hasard_sans_graine.cl` : deux lancements donnent des tirages différents ;
- `randint_empty_runtime.cl`, `choice_empty_runtime.cl`, `invalid_randint_float.cl`,
  `invalid_shuffle_type.cl` ;
- 400 programmes tirés au hasard (graines nulles, négatives, de 64 bits, plages jusqu'à 2^62,
  mélanges et tirages enchaînés) : sortie identique à Python ;
- les fuzzers des couches denses, en `float` et en `float32` : toujours identiques aux boucles
  simples.
