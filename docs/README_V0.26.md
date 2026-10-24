# Clair V0.26

## Grands entiers : `int128`, `int256`, `int512`, `int1024`

```text
def factorielle(n: int) -> int1024 {
    let r: int1024 = 1
    for i in range(2, n + 1) {
        r = r * i
    }
    return r
}

def main() -> int {
    let g: int256 = 123456789012345678901234567890
    print(g * g)              // 15241578753238836750495351562536198787501905199875019052100
    print(factorielle(100))   // les 158 chiffres de 100!
    return 0
}
```

| type      | plus grande valeur        |
|-----------|---------------------------|
| `int`     | 9 223 372 036 854 775 807 (19 chiffres) |
| `int128`  | environ 1,7 × 10^38 (39 chiffres)   |
| `int256`  | environ 5,8 × 10^76 (77 chiffres)   |
| `int512`  | environ 6,7 × 10^153 (154 chiffres) |
| `int1024` | environ 9,0 × 10^307 (308 chiffres) |

Ce sont des entiers signés exacts, comme ceux de Python, mais de taille fixe.

### Ce qu'on peut faire

- `+ - * / %` et `-x` ; `/` et `%` coupent vers zéro, comme pour `int` ;
- `== != < <= > >=` ;
- `print(x)` affiche tous les chiffres ;
- les nombres écrits directement peuvent être aussi grands que le type :
  `let m: int128 = -170141183460469231731687303715884105728` ;
- mélanger les tailles : `int` + `int256` donne un `int256`, `int128` * `int1024` donne un `int1024`.
  L'agrandissement est automatique, il ne perd rien ;
- conversions : `int256(x)`, `int128(x)`, `int(x)`, et `float(x)` qui donne le nombre à virgule le
  plus proche ;
- variables, paramètres, valeurs de retour, champs de classes.

### Dépassement : le programme s'arrête

Comme pour la division par zéro, un résultat qui ne tient pas dans son type arrête le programme
avec un message clair, au lieu de donner un résultat faux :

```text
Clair runtime error: int128 overflow
Clair runtime error: int() argument out of range
Clair runtime error: division by zero
```

Mélanger un grand entier et un `float` est refusé à la compilation : il faut écrire `float(x)`.

### Le C généré

Chaque type est une structure de mots de 64 bits, et chaque opération est une fonction vérifiée :

```c
static ClairInt1024 clair_fn_factorielle(int64_t n) {
    ClairInt1024 r = clair_i1024_from_i64(1);
    ...
            r = clair_i1024_mul(r, clair_i1024_from_i64(i));
```

Seules les fonctions des tailles utilisées par le programme sont écrites dans le C. Un `int128`
coûte à peu près le même prix qu'un `__int128` vérifié écrit à la main en C : sur PC, 50 millions
d'additions prennent 0,034 s contre 0,025 s en C.

## Correction : nombre trop grand pour `int`

Avant, `let x: int = 9223372036854775808` donnait un nombre faux sans prévenir, et un nombre
encore plus grand faisait planter Clang avec un message incompréhensible. Maintenant :

```text
semantic error: integer 9223372036854775808 is too large for int (64 bits): use int128, int256, int512 or int1024
```

`-9223372036854775808`, le plus petit `int`, reste accepté.

## Avertissement de compilation supprimé

Le texte du code des dictionnaires était trop long pour un seul morceau (`-Woverlength-strings`) :
il est coupé en deux. La compilation de Clair n'affiche plus aucun avertissement.

## Tests

- `big_integers.cl` : 100!, fibonacci(300), 2^1000, 3^600 / 3^599, divisions négatives, champs
  de classe, conversions, mélanges de tailles ; sortie identique à Python ;
- `big_overflow_runtime.cl`, `big_div_zero_runtime.cl`, `big_narrow_runtime.cl` : erreurs au bon
  moment ;
- `invalid_int_literal_too_large.cl`, `invalid_big_literal_too_large.cl`,
  `invalid_big_float_mix.cl` ;
- 2000 programmes tirés au hasard (toutes les tailles, toutes les opérations, valeurs limites,
  dépassements, divisions par zéro, conversions) : sortie identique aux entiers exacts de Python,
  y compris les 1105 programmes qui doivent s'arrêter sur une erreur ;
- tous les programmes existants produisent exactement le même C qu'en V0.25.

Les idées et plusieurs cas de test viennent de la branche `feature/int128`, qui reste intacte.
