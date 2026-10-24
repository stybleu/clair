# Clair V0.18

Les outils de base du calcul scientifique et de l'IA : fonctions mathématiques, conversions,
modulo et notation scientifique.

## Fonctions mathématiques

| Fonction | Arguments | Résultat |
|---|---|---|
| `sqrt`, `exp`, `log`, `log10`, `log2` | 1 nombre | `float` |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan` | 1 nombre | `float` |
| `sinh`, `cosh`, `tanh` | 1 nombre | `float` |
| `floor`, `ceil` | 1 nombre | `float` |
| `pow`, `atan2`, `hypot` | 2 nombres | `float` |
| `abs` | 1 nombre | même type que l'argument |
| `min`, `max` | 2 nombres | `int` si les deux sont `int`, sinon `float` |

Un `int` est accepté partout où un `float` est attendu (`sqrt(2)`). Comme en Python, une fonction
définie par l'utilisateur avec le même nom (`def abs(...)`) remplace la fonction intégrée.

Dans le C généré, ce sont directement les fonctions de `<math.h>` (`sqrt`, `exp`, `fabs`, `fmin`…).
Clair compile avec `-fno-math-errno` (il ne lit jamais `errno`) : Clang peut ainsi vectoriser une
boucle comme `y[i] = sqrt(x[i])` (`fsqrt v.2d` en NEON).

## Conversions

- `float(x)` : `int` vers `float` ;
- `int(x)` : `float` vers `int`, en tronquant vers zéro comme Python (`int(-7.9)` donne `-7`).
  Une valeur trop grande, infinie ou NaN arrête le programme : `int() argument out of range` ;
- `int(true)` donne `1`.

## Opérateur `%`

- entre deux `int` : reste de la division, avec la même convention que `/` en Clair (celle du C) :
  le signe suit le dividende, `-17 % 5` donne `-2` (Python donnerait `3`). `x % 0` arrête le
  programme : `modulo by zero` ;
- avec un `float` : `fmod`, `7.5 % 2` donne `1.5` ;
- `x % 2` est écrit tel quel dans le C ; seul un diviseur variable passe par la vérification.

## Notation scientifique

`1e3`, `2.5e-3`, `6.02e23` sont des `float`.

## Exemple : neurone et softmax

```text
def sigmoid(x: float) -> float {
    return 1.0 / (1.0 + exp(-x))
}

def softmax(v: list[float], out: list[float]) -> int {
    let m: float = v[0]
    for i in range(len(v)) {
        m = max(m, v[i])
    }
    let total: float = 0.0
    for i in range(len(v)) {
        out[i] = exp(v[i] - m)
        total = total + out[i]
    }
    for i in range(len(v)) {
        out[i] = out[i] / total
    }
    return 0
}
```

## Limites connues

- `floor` et `ceil` renvoient un `float` (comme en C) ; utiliser `int(floor(x))` pour un entier ;
- `sqrt` d'un nombre négatif donne `nan` au lieu d'une erreur ;
- les boucles `while` et `self.items[i]` ne profitent pas encore de la version rapide.
