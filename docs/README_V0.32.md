# Clair V0.32

## Entiers sans signe : `uint128`, `uint256`, `uint512`, `uint1024`

```text
def factorielle(n: int) -> uint1024 {
    let r: uint1024 = 1
    for i in range(2, n + 1) {
        r = r * i
    }
    return r
}

def main() -> int {
    let m: uint128 = 340282366920938463463374607431768211455   // le plus grand uint128
    print(m / 3)
    print(factorielle(160))
    return 0
}
```

| type       | valeurs                                  |
|------------|------------------------------------------|
| `uint128`  | de 0 à 2^128 - 1 (39 chiffres)           |
| `uint256`  | de 0 à 2^256 - 1 (78 chiffres)           |
| `uint512`  | de 0 à 2^512 - 1 (155 chiffres)          |
| `uint1024` | de 0 à 2^1024 - 1 (309 chiffres)         |

Un `uint` ne descend jamais sous 0 et va deux fois plus haut que l'`int` de même taille. Tout ce
qui marche avec `int128` .. `int1024` marche aussi : `+ - * / %`, comparaisons, `print`,
variables, paramètres, valeurs de retour, champs de classes.

### Les règles

- un résultat négatif ou trop grand arrête le programme :
  `Clair runtime error: uint128 overflow: negative result`, `uint128 overflow` ;
- un `int` se mélange avec un `uint` : il est converti, et s'il est négatif le programme s'arrête
  (`uint256 cannot hold a negative value`) ;
- `-u` est refusé à la compilation, ainsi qu'un nombre négatif écrit dans un `uint` ;
- un `uint` et un grand entier signé (`uint128` et `int256`) ne se mélangent pas : il faut
  choisir avec `int256(x)` ou `uint256(x)` ;
- agrandir ne perd rien et se fait seul : `uint128` vers `uint512`, `uint128` vers `int256` ;
- conversions écrites : `uint256(x)`, `int128(x)`, `int(x)`, `float(x)`, vérifiées.

## `abs`, `min`, `max` et `pow` sur les grands entiers

Ils donnent un résultat exact, dans le type du grand entier :

```text
let h: int256 = -170141183460469231731687303715884105728
print(abs(h))                    // 170141183460469231731687303715884105728
print(max(h, 3))                 // 3
print(pow(int128(-3), 77))       // -5474401089420219382077155933569751763
let d: int1024 = pow(2, 1000)    // les 302 chiffres de 2^1000
```

- `pow(x, n)` : `x` un grand entier, `n` un `int` positif ou nul ; calculé par carrés successifs
  (`x^13 = x * x^4 * x^8`), un dépassement arrête le programme ;
- `pow(2, 1000)` écrit là où un `int1024` est attendu calcule en `int1024`. Ailleurs,
  `pow` de deux `int` reste un calcul à virgule, comme avant ; pour un résultat exact,
  écrire `pow(int1024(2), 1000)` ;
- `abs` du plus petit `int128` ne tient pas dans un `int128` : le programme s'arrête.

## Le C généré

```c
static ClairUInt1024 clair_fn_factorielle(int64_t n) {
    ClairUInt1024 r = clair_u1024_from_i64(1);
    ...
            r = clair_u1024_mul(r, clair_u1024_from_i64(i));
```

Les fonctions communes des grands entiers ont été réorganisées : les opérations sur les nombres
positifs servent aux `uint` et aux valeurs absolues des `int`. Les `int128` .. `int1024` vont à
la même vitesse qu'avant.

## Tests

- `big_unsigned.cl` : 160!, 200 parmi 100, un générateur pseudo-aléatoire 128 bits dans un champ
  de classe, conversions, `abs`, `min`, `max`, `pow` ; sortie identique à Python ;
- `big_unsigned_negative_runtime.cl`, `big_unsigned_from_negative_runtime.cl`,
  `big_pow_negative_runtime.cl` : erreurs au bon moment ;
- `invalid_unsigned_negative.cl`, `invalid_unsigned_mix.cl`, `invalid_unsigned_minus.cl` ;
- 4000 programmes tirés au hasard sur les neuf types entiers (opérations, mélanges, conversions,
  `abs`, `min`, `max`, `pow`, valeurs limites) : sortie identique aux entiers exacts de Python, y
  compris les programmes qui doivent s'arrêter sur une erreur ; 250 d'entre eux passés aussi sous
  AddressSanitizer et UndefinedBehaviorSanitizer ;
- les 600 programmes tirés au hasard de la V0.26 (`int128` .. `int1024`) : toujours identiques à
  Python.
