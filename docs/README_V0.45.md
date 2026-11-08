# Clair V0.45

## Les fonctions mathématiques des grands nombres

```text
let x: bigfloat = bigfloat(2, 50)              // 50 chiffres
print(sqrt(x))                                 // 1.4142135623730950488016887242096980785696718753769e0
print(pi(1000))                                // les 1000 premiers chiffres de pi
print(sin(bigfloat(1, 40)))
print(atan2(bigfloat(1, 30), -3))
let b: bigint = bigint("123456789012345678901234567890")
print(isqrt(b))                                // 351364182882014
print(pow(bigint(2), 1000, bigint(1000000007)))
```

Les mêmes noms que pour les `float`, comme `mpmath` en Python :

| fonctions | pour |
|---|---|
| `sqrt`, `exp`, `log`, `log10`, `log2` | `bigfloat` |
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2(y, x)` | `bigfloat` |
| `sinh`, `cosh`, `tanh`, `hypot(x, y)`, `pow(x, y)` | `bigfloat` |
| `pi(chiffres)` | un `bigfloat` de pi avec autant de chiffres |
| `floor`, `ceil` | `bigfloat` → `bigint` |
| `abs` | `bigint` et `bigfloat` |
| `isqrt` (racine entière exacte), `gcd`, `pow(b, k)`, `pow(b, e, m)` | `bigint` (et `int`) |
| `int(x)`, `float(x)` | `bigint` et `bigfloat` → `int` (vérifié) ou `float` |
| `bigint(x)` | `bigfloat` → `bigint`, vers zéro comme `int()` en Python |
| `bigfloat(x, chiffres)` | depuis une chaîne, un `int`, un `float`, un `bigint` ou un `bigfloat` |

- **Précision** : le résultat a la précision de son argument (la plus grande des deux pour
  `atan2`, `pow`, `hypot`). `sqrt` est arrondie exactement ; les autres ont une erreur de moins
  d'une unité sur le dernier chiffre (en pratique, toutes les valeurs essayées sont arrondies
  exactement).
- **Mélanges** : `x * 2 + 0.5`, `1 - x`, `x > 1`, `pow(x, 0.5)` : un `int`, un `float` ou un
  `bigint` à côté d'un `bigfloat` est converti exactement (`0.1` vaut alors
  `0.1000000000000000055511151231257827...`, sa vraie valeur), puis le calcul est arrondi à la
  précision du `bigfloat`. `%` n'existe pas pour les `bigfloat`.
- **Erreurs**, comme Python : `sqrt` d'un négatif, `log(0)`, `asin(2)` arrêtent le programme
  (`math domain error`) ; `isqrt(-4)`, `pow(b, e, 0)`, `int(b)` trop grand aussi.
- **Limites** : `exp(x)` demande `|x| < 10^16` (l'exposant d'un `bigfloat` tient sur 64 bits) ;
  `sin` et `cos` demandent moins de 100 000 chiffres avant la virgule.

### Comment c'est calculé

- Les calculs se font en virgule fixe binaire : un `bigint` X représente X / 2^B. Multiplier,
  c'est `(A * C) >> B` ; diviser par un petit entier ne coûte qu'un passage.
- `exp` : x = K ln 10 + r, puis r / 2^s, une série de Taylor et s mises au carré ;
  `log` : x = m 2^k, puis 2 atanh((m − 1)/(m + 1)) ; `sin`, `cos` : x moins un multiple de
  π/2, puis des demi-angles ; `atan` : des demi-angles jusqu'à |x| < 1/256, puis la série.
- π par la formule de Machin, ln 2 par atanh(1/3), ln 10 par le logarithme : gardés une fois
  calculés.
- Quand le résultat est petit (sin d'un nombre proche de π, log d'un nombre proche de 1), ses
  premiers bits sont des zéros : la fonction recommence avec plus de bits.
- `sqrt` utilise la racine entière exacte (Newton) : arrondi exact.

C'est rapide : 1000 chiffres de sin, exp ou log en quelques millisecondes, 10 000 chiffres de π
en 0,05 s (sur la machine de test).

### Autres changements

- Les erreurs des grands nombres s'écrivent comme les autres : `Clair runtime error: ...`, en
  anglais (avant : `Clair V0.40 big numbers: ...`).
- Nouveau fichier `src/clair_bigmath.c` (et `.h`), recopié dans le C des programmes comme les
  autres fichiers des grands nombres.

## Tests

- `grands_nombres_maths.cl` : `pi(1000)`, `sin(1e-30)`, `cos` près de π, `log` près de 1,
  `sqrt`, `exp`, `atan2`, `pow` entier et non entier, mélanges avec `int`, `float` et `bigint`,
  `floor`, `ceil`, `isqrt`, `gcd`, `pow` modulaire, `abs`, conversions ; identique à mpmath et
  à Python ;
- `bigfloat_domain_runtime.cl`, `bigfloat_log_zero_runtime.cl`, `isqrt_negative_runtime.cl`,
  `powmod_zero_runtime.cl`, `bigint_to_int_runtime.cl` ; `invalid_bigint_sqrt.cl`,
  `invalid_pi_digits.cl`, `invalid_bigfloat_mod.cl` ;
- 2300 appels tirés au hasard des 18 fonctions (précisions de 1 à 300 chiffres, nombres de
  1e-200 à 1e50, cas difficiles près de 1, de π/2, de 0) comparés à mpmath : tous arrondis
  exactement ; les erreurs de domaine aux mêmes endroits que mpmath ; seuls les résultats dont
  l'exposant dépasse 64 bits (`exp(-10^20)`) sont refusés là où mpmath répond ; 400 d'entre
  eux sans erreur d'AddressSanitizer ni d'UndefinedBehaviorSanitizer ;
- 1500 calculs `bigfloat` mêlés à des `int`, `float`, `bigint` et `bigfloat` d'autres
  précisions : tous arrondis exactement (comparés à mpmath) ;
- le C des programmes sans grands nombres est identique à la V0.44.
