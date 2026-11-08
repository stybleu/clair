# Clair V0.48

## Les unités vérifiées à la compilation

```text
let d: float[m] = 2.25e11
let t: float[s] = 3.0e7
let v: float[m/s] = d / t               // vérifié : m / s
let g: float[m/s^2] = 9.81
let x = d + t                           // refusé : unit error: cannot add m and s
```

Clair calcule l'unité de chaque calcul et refuse le programme avant de le lancer quand elles ne
collent pas. Ensuite, les unités disparaissent : le C est exactement le même qu'avec des `float`
ordinaires, le programme va aussi vite.

En 1999, la sonde Mars Climate Orbiter a été perdue parce qu'une équipe donnait des livres-force et
l'autre attendait des newtons. C'est cette erreur-là que Clair refuse.

### Écrire une unité

- Après `float` ou `float32` : `float[m]`, `float[m/s^2]`, `float[kg*m/s^2]`, `float[m^3/(kg*s^2)]`,
  `float[1/s]`, `float[m^-1]` ; dans une liste : `list[float[m]]`.
- Après un nombre : `9.81[m/s^2]`, `400[km]` (c'est `400.0[km]`).
- Partout où on déclare : `let`, paramètres, valeur rendue (`-> float[m/s]`), champs de classe.
- Les unités du SI sont écrites avec les unités de base (m, kg, s, A, K, mol, cd) : `N` est `kg*m/s^2`,
  `J` est `N*m`, `W` est `J/s`, aussi `Pa`, `Hz`, `C`, `V`, `Ohm`, `F`, `T`, `Wb`, `H`, `S`, `lm`,
  `lx`, `Gy`, `Sv`, `Bq`, `kat`. `rad` et `sr` sont sans unité.
- Tout autre nom est une unité à part : `km`, `h`, `deg`, `au`... `km` et `m` ne se mélangent jamais.
  Pour convertir, on multiplie par le rapport : `altitude * 1000.0[m/km]`.

### Les règles

| calcul | unité |
|---|---|
| `a + b`, `a - b`, `a % b`, `a < b`... | les deux doivent avoir la même unité |
| `a * b`, `a / b` | les unités se multiplient, se divisent |
| `sqrt(a)`, `cbrt(a)` | la racine de l'unité (`sqrt` de `m^2` donne `m`) ; refusé pour `m` |
| `pow(a, 3)` | l'unité au cube ; l'exposant doit être un nombre entier écrit |
| `abs`, `floor`, `ceil`, `trunc`, `min`, `max`, `hypot`, `fmod` | la même unité |
| `sin`, `cos`, `exp`, `log`, `erf`, `gamma`... | un nombre sans unité, rendent un nombre sans unité |
| `atan2(y, x)` | `y` et `x` dans la même unité, rend un angle (sans unité) |
| `radians(a)`, `degrees(a)` | `deg` vers sans unité, et l'inverse |

- **Un nombre écrit sans unité** prend l'unité de ce qu'il initialise : `let g: float[m/s^2] = 9.81`,
  un argument, une valeur rendue, un champ. Il multiplie sans rien changer : `0.5 * g * t * t`.
  Mais à côté d'une unité dans `+`, `-` ou une comparaison, il est refusé : `d + 1.5` donne
  `1.5 has no unit next to m: write 1.5[m]`. Seul `0` va avec toutes les unités : `if v > 0.0`.
- **Un `float` sans unité n'est pas vérifié** : on peut mettre des unités petit à petit, et un
  programme sans unité ne change pas.
- **Les constantes ont leur unité** : `phys.G` est en `m^3/(kg*s^2)`, `phys.c` en `m/s`, `GM_mars`
  en `m^3/s^2`, `UA` en `m`, `jour` en `s`... `math.pi` est un nombre sans unité.

### Exemple : de la Terre vers Mars

```text
import math as m
from astro import GM_soleil, UA, jour

def vitesse_ellipse(r: float[m], a: float[m]) -> float[m/s] {
    return m.sqrt(GM_soleil * (2.0 / r - 1.0 / a))
}

def main() -> int {
    let r1: float[m] = UA
    let r2: float[m] = 1.523679 * UA
    let a: float[m] = (r1 + r2) / 2.0
    let dv1: float[m/s] = vitesse_ellipse(r1, a) - m.sqrt(GM_soleil / r1)
    let duree: float[s] = m.pi * m.sqrt(pow(a, 3) / GM_soleil)
    print(dv1)               // 2944.68925638263 m/s
    print(duree / jour)      // 258.865758895105 jours
    return 0
}
```

### Les erreurs

```text
unit error: cannot add m and s (in main)
unit error: 1.5 has no unit next to m: write 1.5[m] (in main)
unit error: sin() needs a number without unit, it receives m (in main)
unit error: argument 1 of aire (r) is in m, it receives s (in main)
unit error: cannot add km and m (in main)
unit error: the value returned is in m/s, it receives m*s (in vitesse)
unit error: x is in m, it receives m^3/(kg*s^2) (in main)
parse error at 2:17: empty unit
```

### Limites

- Pas d'unités dans les matrices ni les dictionnaires, ni pour les `int`, les `bigint` ou les
  `bigfloat`.
- La variable `v` de `for i, v in enumerate(xs)` n'a pas d'unité (elle n'est pas vérifiée).
- Les erreurs n'ont pas encore de numéro de ligne : elles disent la fonction.

## Corrigé

- Importer un module qui a une classe avec des méthodes faisait lire au compilateur de la mémoire
  déjà rendue (V0.47). Le résultat était juste, mais le compilateur pouvait s'arrêter. Trouvé par
  AddressSanitizer.

## Tests

- `unites.cl` : un transfert de Hohmann de la Terre vers Mars (2944,7 m/s puis 2648,9 m/s,
  258,9 jours), une orbite basse depuis des km convertis, poids en `N`, énergie en `J`, puissance
  en `W`, un angle en `deg`, une liste de `float[m]` ; identique à Python ;
- huit programmes refusés : `m + s`, un nombre sans unité, `sin` de mètres, un argument, `km + m`,
  la valeur rendue, une constante de `phys`, une unité mal écrite ;
- 8000 programmes tirés au hasard (unités de base, dérivées, conversions, `+ - * /`, `sqrt`,
  `abs`, `sin`, `pow`, comparaisons, nombres avec et sans unité, variables avec et sans unité) :
  le compilateur accepte ou refuse exactement comme un modèle des règles écrit en Python, avec le
  même message ; pour les 975 acceptés, le C est identique octet pour octet à celui du même
  programme sans ses unités ; 600 d'entre eux compilés par un Clair passé à AddressSanitizer et
  UndefinedBehaviorSanitizer ;
- le C des programmes sans unité est identique à la V0.47 (148 programmes comparés).
