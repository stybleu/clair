# Clair V0.35

## Entiers machine : `u8`, `u16`, `u32`, `u64`, `i8`, `i16`, `i32`

```text
// moyenne de deux images en niveaux de gris, un octet par pixel
def moyenne(a: list[u8], b: list[u8], c: list[u8]) -> int {
    for i in range(len(a)) {
        c[i] = (a[i] >> 1) + (b[i] >> 1) + (a[i] & b[i] & 1)
    }
    return 0
}

// generateur pseudo-aleatoire xorshift64
def xorshift(s: u64) -> u64 {
    let x: u64 = s
    x ^= x << 13
    x ^= x >> 7
    x ^= x << 17
    return x
}
```

| type  | valeurs                          | octets |
|-------|----------------------------------|--------|
| `u8`  | de 0 à 255                       | 1      |
| `u16` | de 0 à 65 535                    | 2      |
| `u32` | de 0 à 4 294 967 295             | 4      |
| `u64` | de 0 à 2^64 - 1                  | 8      |
| `i8`  | de -128 à 127                    | 1      |
| `i16` | de -32 768 à 32 767              | 2      |
| `i32` | de -2 147 483 648 à 2 147 483 647 | 4     |
| `i64` | le même type que `int`           | 8      |

Ce sont les entiers du processeur, ceux de C, NumPy (`np.uint8`...) et des formats de
fichiers : pixels, octets d'un fichier, couleurs, empreintes, générateurs pseudo-aléatoires,
poids d'IA quantifiés. Une `list[u8]` prend 1 octet par élément au lieu de 8, et une
instruction NEON du téléphone traite 16 `u8` à la fois.

Ils marchent partout où marche `int` : variables, paramètres, valeurs de retour, champs de
classes, `list[u8]`, `matrix[u8]`, indices, `range`, `print`, `abs`, `min`, `max`.

### Les règles, celles de C et NumPy

- **Les calculs font le tour** : avec `a: u8 = 200`, `a + 100` vaut 44 (300 - 256) et
  `100 - a` vaut 156. Avec `c: i8 = 127`, `c + 1` vaut -128. Comme en C et dans NumPy, sans
  erreur : c'est ce que veulent les empreintes, les générateurs et les calculs sur les octets.
- **Un nombre écrit à côté d'un `u8` est un `u8`**, et il doit tenir : `a + 300` est refusé à
  la compilation (`integer 300 does not fit in u8`), comme `let x: u16 = -1`.
- **Agrandir se fait seul quand rien ne se perd** : `u8` vers `u16`, `u8` vers `i16`, `u32`
  vers `int`. Un `u8` et un `u32` se calculent en `u32`, un `u16` et un `int` en `int`.
- **Les mélanges qui perdraient quelque chose sont refusés** : `u32` avec `i32`, `u64` avec
  `int`, `u8` avec `i8` (aucun des deux ne contient toutes les valeurs de l'autre). En C,
  `-1 == 4294967295u` est vrai : Clair demande de choisir, par exemple `int(x)`.
- **`/` et `%` coupent vers zéro**, comme pour `int`. Une division par zéro arrête le
  programme ; le plus petit `i8` divisé par -1 aussi (`integer division overflow`), comme
  pour `int`.
- **Avec un `float32`** : `u8`, `i8`, `u16`, `i16` et `int` restent en `float32` ; `i32`,
  `u32` et `u64` passent en `float` (64 bits), comme dans NumPy.
- **Conversions** : `u8(x)`, `i32(x)`... gardent les bits du bas, comme en C : `u8(300)` vaut
  44, `i8(200)` vaut -56, `u8(-1)` vaut 255. Un nombre à virgule est d'abord coupé vers zéro
  (`u16(3.9)` vaut 3). `int(x)` d'un `u64` trop grand pour un `int` arrête le programme.

## Opérations sur les bits : `& | ^ ~ << >>`

Sur tous les entiers machine, `int` compris, avec la priorité de Python :

- `a & b == 0` veut dire `(a & b) == 0` (en C ce serait `a & (b == 0)`) ;
- `1 + 2 << 3` vaut 24, `6 & 3 | 8 ^ 1` vaut 11 ;
- `x << n` a le type de `x` ; décaler de toute la taille ou plus donne 0 ; `>>` d'un nombre
  négatif garde le signe (`-9 >> 1` vaut -5) ; un décalage négatif arrête le programme
  (`negative shift count`) ;
- `&=`, `|=`, `^=`, `<<=`, `>>=` comme `+=`.

## Nombres en hexadécimal, binaire et octal

```text
let masque: u32 = 0xFF00_FF00
let drapeaux: u8 = 0b0000_0101
let droits: int = 0o755
let million: int = 1_000_000
let x: float = 1_000.5
```

Comme en Python : `_` entre les chiffres pour lire plus facilement, et `007` est refusé (en C,
ce serait de l'octal : 010 vaut 8). Il faut écrire `7` ou `0o7`.

## Le C généré

```c
static uint8_t clair_fn_melange(uint8_t a, uint8_t b) {
    return (uint8_t)(a * 31 + b);
}

static uint64_t clair_fn_xorshift(uint64_t s) {
    uint64_t x = s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

static uint32_t clair_fn_rotl(uint32_t x, int64_t r) {
    return clair_rt_shl_u32(x, r) | clair_rt_shr_u32(x, 32 - r);
}
```

Un calcul en `u8` ou `i16` se fait en `int` en C, puis il est ramené à 8 ou 16 bits par
`(uint8_t)(...)`, une seule fois pour toute l'expression. Les divisions et les décalages
d'une quantité inconnue passent par de petites fonctions vérifiées ; par un nombre écrit,
ils restent de simples `/` et `<<`.

Le rapport compare maintenant onze programmes : le nouveau, `octets`, fait la moyenne de deux
images d'octets (`list[u8]`), 16 pixels par instruction NEON.

## Tests

- `entiers_machine.cl` : calculs qui font le tour sur les sept types, opérations sur les bits,
  priorités de Python, conversions, mélanges permis, `abs`, `min`, `max`, nombre de bits à 1,
  rotation, xorshift64, empreinte FNV-1a dans une classe, `list[u8]`, `matrix[u8]`,
  `matrix[i16]`, tranches, `enumerate`, `float32` ; sortie identique à Python ;
- `machine_div_zero_runtime.cl`, `machine_shift_negative_runtime.cl`,
  `machine_int_range_runtime.cl` : erreurs au bon moment ;
- `invalid_machine_mix.cl`, `invalid_machine_literal.cl`, `invalid_machine_negative.cl`,
  `invalid_bit_float.cl`, `invalid_machine_narrow.cl`, `invalid_number_literal.cl`,
  `invalid_leading_zero.cl` ;
- 4000 programmes tirés au hasard (les huit types, valeurs limites, expressions imbriquées
  écrites avec les priorités de Python, conversions, `abs`, `min`, `max`, `+=` et ses
  cousins, `float32` et `float`) : sortie identique à un modèle exact en Python, y compris les
  1522 programmes qui doivent s'arrêter sur une erreur ;
- les dix programmes du rapport : C généré et assembleur ARM identiques à la V0.34 ;
- les fuzzers des versions précédentes : toujours identiques.
