# Clair V0.47

## `import`, `from ... import`, `as`

```text
import math as m                    // m.sqrt(x), m.pi
import phys                         // phys.G, phys.c
from astro import GM_mars, UA       // GM_mars et UA directement
import orbite                       // le fichier orbite.cl, à côté du programme
from orbite import periode as T     // T(gm, a) au lieu de orbite.periode(gm, a)

def main() -> int {
    print(m.sqrt(2.0))
    print(phys.G * 5.972e24)
    print(orbite.vitesse_circulaire(GM_mars, 3789500.0))
    print(T(1.32712440041279419e20, UA) / 86400.0)     // 365.256898327236 jours
    return 0
}
```

Comme en Python :

- `import x` donne `x.nom`, `import x as y` donne `y.nom`, `from x import a, b as c` donne `a` et `c`
  directement ;
- plusieurs modules sur une ligne : `import math, phys` ;
- une variable ou un paramètre du même nom qu'un module ou qu'un nom importé le cache dans toute
  sa fonction (`def f(m: int)` : `m` y est le paramètre, plus le module).

## Les trois modules de Clair

Ils sont rangés dans le compilateur : rien à installer. Une constante devient directement un
nombre dans le C (`phys.G` s'écrit `6.67430e-11`), et `m.sqrt(x)` est le même appel que
`sqrt(x)`. Ça ne coûte rien à l'exécution.

### `math`

| | |
|---|---|
| constantes | `pi`, `e`, `tau`, `inf`, `nan` |
| puissances, logarithmes | `sqrt`, `cbrt`, `exp`, `exp2`, `expm1`, `log`, `log2`, `log10`, `log1p`, `pow` |
| trigonométrie | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `hypot`, `degrees`, `radians` |
| hyperboliques | `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh` |
| arrondis | `floor`, `ceil`, `trunc`, `fabs`, `fmod`, `copysign` |
| spéciales | `erf`, `erfc`, `gamma`, `lgamma` |
| entiers | `isqrt`, `gcd` |
| grands nombres | `m.pi(50)` : π avec 50 chiffres (un `bigfloat`) |

Les nouvelles fonctions (`cbrt`, `erf`, `gamma`, `degrees`...) marchent aussi sans `import`,
comme `sqrt` avant. `m.sqrt` est toujours celle de Clair, même si le programme a sa propre
fonction `sqrt`.

### `phys` : CODATA 2022, unités SI

| nom | valeur | |
|---|---|---|
| `c` | 299792458 | m/s, exacte |
| `h`, `hbar` | 6.62607015e-34, 1.0545718176461565e-34 | J s, exacte |
| `e` | 1.602176634e-19 | C, exacte (charge élémentaire) |
| `N_A`, `k_B` | 6.02214076e23, 1.380649e-23 | exactes |
| `R`, `F` | 8.31446261815324, 96485.33212331001 | exactes |
| `sigma`, `b_wien` | 5.6703744191844294e-08, 0.0028977719551851727 | exactes |
| `g0`, `atm`, `eV` | 9.80665, 101325, 1.602176634e-19 | exactes |
| `G` | 6.67430e-11 | m³/(kg s²), ± 0.00015e-11 |
| `m_e`, `m_p`, `m_n`, `m_u` | 9.1093837139e-31, 1.67262192595e-27, 1.67492750056e-27, 1.66053906892e-27 | kg |
| `eps0`, `mu0` | 8.8541878188e-12, 1.25663706127e-06 | |
| `alpha`, `R_inf`, `a0` | 0.0072973525643, 10973731.568157, 5.29177210544e-11 | |
| `l_P`, `m_P`, `t_P`, `T_P` | 1.616255e-35, 2.176434e-08, 5.391247e-44, 1.416784e32 | unités de Planck |

### `astro` : unités SI

| nom | valeur | source |
|---|---|---|
| `UA` (ou `AU`) | 149597870700 m | exacte, UAI 2012 |
| `pc` | 3.085677581491367e16 m | UA × 648000 / π |
| `al` (ou `ly`) | 9460730472580800 m | exacte, année-lumière |
| `jour`, `an` | 86400 s, 31557600 s | année julienne |
| `GM_soleil` | 1.32712440041279419e20 m³/s² | éphéméride DE440 du JPL |
| `GM_mercure` ... `GM_pluton` | | DE440 ; Mars, Jupiter, Saturne, Uranus, Neptune et Pluton avec leurs lunes |
| `GM_terre`, `GM_lune` | 3.98600435507e14, 4.902800118e12 | DE440 |
| `R_soleil`, `L_soleil` | 6.957e8 m, 3.828e26 W | nominales, UAI 2015 |
| `R_terre`, `R_terre_polaire` | 6.3781e6 m, 6.3568e6 m | nominales, UAI 2015 |
| `R_jupiter`, `R_jupiter_polaire` | 7.1492e7 m, 6.6854e7 m | nominales, UAI 2015 |

Une constante est un `float` (environ 16 chiffres). Pour 50 chiffres exacts, écrire la valeur :
`bigfloat("6.67430e-11", 50)` (`bigfloat(phys.G, 50)` convertit exactement le `float`).

## Les modules en fichiers

```text
// orbite.cl
import math as m

Class Astre {
    nom: string
    gm: float
    def vitesse(r: float) -> float {
        return vitesse_circulaire(self.gm, r)
    }
}

def vitesse_circulaire(gm: float, r: float) -> float {
    return m.sqrt(gm / r)
}

def main() -> int {          // lancé seul, le module se teste ; importé, ce main est laissé de côté
    print(vitesse_circulaire(3.986e14, 6.778e6))
    return 0
}
```

- `import orbite` lit `orbite.cl` dans le dossier du programme compilé.
- Ses fonctions s'appellent `orbite.vitesse_circulaire(...)`, ou directement après
  `from orbite import vitesse_circulaire`. Dans le module lui-même, sans préfixe.
- Ses classes gardent leur nom : `Astre(...)` ou `orbite.Astre(...)`, `let a: Astre`. Deux
  fichiers ne peuvent pas avoir une classe du même nom.
- Son `main` n'est pas pris quand il est importé : un module peut garder son `main` de test.
- Deux modules peuvent s'importer l'un l'autre (`pair.cl` et `impair.cl`).
- Dans le C, `orbite.vitesse_circulaire` devient `clair_fn_orbite__mod__vitesse_circulaire` : deux
  modules peuvent avoir une fonction du même nom.

## Les erreurs

```text
import error: no module named 'orbit' (no file orbit.cl)
import error: module 'math' has no 'sqr' (in main() of prog.cl)
import error: phys.G is a constant, not a function (in main() of prog.cl)
import error: math.sqrt is a function: call it with its arguments
import error: cannot import 'vitesse' from 'orbite'
import error: 'c' is imported twice
import error: phys.c: the values of a module cannot be changed
semantic error: undefined function or class 'periode' (did you mean orbite.periode(...)?)
```

Une erreur de syntaxe dans un module est suivie de `import error: the error above is in orbite.cl`.

## Limites

- `from x import *` n'existe pas : on nomme ce qu'on importe.
- Les modules sont cherchés à côté du programme seulement (pas de dossier de bibliothèques).
- Un module ne doit pas importer le programme principal (il serait relu comme un second module).

## Tests

- `modules/importe.cl` : `import math as m`, `phys`, `from astro import`, un module avec une classe
  et une méthode qui appelle le module, `from orbite import periode as T`, un module qui en
  importe un autre, deux modules qui s'importent l'un l'autre, un `sqrt` du programme qui ne cache
  pas `m.sqrt`, un paramètre qui cache un module, les nouvelles fonctions de `math`, `inf`,
  `nan`, `m.pi(30)` ; identique à Python ;
- `modules/orbite.cl` lancé seul : son `main` tourne ;
- six erreurs : module absent, fonction inconnue de `math`, constante appelée, fonction d'un
  module sans son préfixe, nom absent d'un module, constante changée ;
- 800 combinaisons d'`import` tirées au hasard (modules de Clair, fichiers qui s'importent,
  alias, noms absents, doublons, variables du même nom), compilées par un Clair passé à
  AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur mémoire, aucune fuite ;
- le C des programmes sans `import` est identique à la V0.46 (147 programmes comparés).
