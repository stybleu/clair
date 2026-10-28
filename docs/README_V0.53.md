# Clair V0.53

## Des mesures avec leur incertitude

```text
import phys

def chute(h: measure[m], g: measure[m/s^2]) -> measure[s] {
    return sqrt(2.0 * h / g)
}

def main() -> int {
    let g: measure[m/s^2] = 9.81 ± 0.02
    let h: measure[m] = 20.0 ± 0.05
    print(chute(h, g))                   // 2.0193 ± 0.0033
    print(measure(phys.G))               // (6.67430 ± 0.00015)e-11
    return 0
}
```

Une mesure, c'est une valeur et son incertitude-type : `9.81 ± 0.02`. Clair la fait suivre dans
tout le calcul, à travers les opérations, les fonctions, les objets et les boucles, sans
bibliothèque à ajouter. Les unités de la V0.48 marchent avec : `measure[m/s^2]` est vérifié à la
compilation comme `float[m/s^2]`.

### Écrire une mesure

- `9.81 ± 0.02`, ou `9.81 +/- 0.02` (plus facile à taper sur un téléphone), ou `measure(9.81, 0.02)`.
- Avec une unité : `20.0[m] ± 0.05` (l'incertitude prend l'unité de la valeur), ou
  `let h: measure[m] = 20.0 ± 0.05`.
- Un nombre ordinaire va partout où une mesure est attendue : c'est une mesure exacte (`± 0`).
- `value(m)` donne la valeur, `uncertainty(m)` l'incertitude (deux `float` dans la même unité).
- `print(m)`, `str(m)` : l'incertitude avec 2 chiffres significatifs, la valeur arrêtée au même
  chiffre, comme on l'écrit en physique : `686.980 ± 0.031`, `12350 ± 680`,
  `(6.67430 ± 0.00015)e-11`. `format(m, ".4f")` écrit les deux nombres avec `.4f`.

### Ce qu'on peut faire avec

- `+ - * /`, `-m`, entre mesures et avec des nombres ;
- `sqrt`, `cbrt`, `exp`, `exp2`, `expm1`, `log`, `log10`, `log2`, `log1p`, `sin`, `cos`, `tan`, `asin`,
  `acos`, `atan`, `sinh`, `cosh`, `tanh`, `asinh`, `acosh`, `atanh`, `abs`, `degrees`, `radians`,
  `pow`, `atan2`, `hypot` ;
- les comparaisons (`<`, `>`, `==`...) comparent les valeurs ;
- des paramètres, des valeurs rendues et des champs de classe de type `measure`.

Les autres fonctions (`min`, `max`, `floor`, `int`, `float`...) refusent une mesure et disent
d'utiliser `value(m)`.

### Comment l'incertitude est calculée

Comme le Guide pour l'expression de l'incertitude de mesure (GUM), au premier ordre : chaque mesure
écrite avec `±` est une source indépendante, et chaque résultat garde, pour chacune de ses sources,
la dérivée du calcul par rapport à elle multipliée par l'incertitude de la source. L'incertitude du
résultat est la racine de la somme de leurs carrés.

Une même mesure qui revient plusieurs fois est donc comptée une seule fois, avec son signe :

```text
let x: measure = 3.0 ± 0.1
print(x - x)          // 0 ± 0     (et non 0.00 ± 0.14)
print(x / x)          // 1 ± 0
print(x + x)          // 6.00 ± 0.20
print(x * x)          // 9.00 ± 0.60
```

Chaque mesure se souvient de 16 sources à part. Au-delà (une boucle qui additionne beaucoup de
mesures), Clair réunit les deux plus petites en une seule : l'incertitude reste exacte, mais la
corrélation avec ces deux petites sources est perdue. La somme de 100 mesures `1.0 ± 0.1` donne bien
`100.0 ± 1.0`.

### Les constantes de phys gardent leur incertitude

Les constantes de `phys` qui ne sont pas exactes (CODATA 2022) entrent avec leur incertitude dans un
calcul de mesures : `G`, `m_e`, `m_p`, `m_n`, `m_u`, `alpha`, `mu0`, `eps0`, `a0`, `R_inf`, `l_P`,
`m_P`, `t_P`, `T_P`. Depuis 2019, `c`, `h`, `hbar`, `e`, `k_B`, `N_A`, `R`, `F`, `sigma`, `b_wien`
sont exactes, comme `g0`, `atm`, `eV`. Les constantes de `astro` sont exactes : les GM de DE440
n'ont pas d'incertitude publiée, et elles sont connues bien mieux que G.

Dans un `float`, une constante reste un nombre ordinaire, comme avant :

```text
let gf: float = phys.G                       // 6.6743e-11, rien ne change
let gm: measure = phys.G                     // (6.67430 ± 0.00015)e-11
let masse: measure[kg] = GM_terre / phys.G   // (5.97217 ± 0.00013)e+24
```

Les constantes de CODATA ne sont pas indépendantes : `mu0` vient de `alpha`, `eps0 = 1/(mu0 c²)`,
`m_e` de `R_inf` et `alpha`, les longueurs et masses de Planck de `G`... Clair les fait toutes
venir de six sources (`alpha`, `R_inf`, les masses atomiques relatives de l'électron, du proton et
du neutron, `G`) avec ces relations exactes. Chaque constante retrouve son incertitude publiée, et
les corrélations sont justes :

```text
print(measure(phys.eps0 * phys.mu0 * phys.c * phys.c))    // 1.00000000000119 ± 0
print(masse * phys.G)                                     // 398600435507000 ± 0
print(sqrt(2.0 * phys.G * masse / R_terre))               // 11179.9077548409 ± 0
```

La masse de la Terre n'est connue qu'à 2 × 10⁻⁵ près, à cause de G. Mais la vitesse de libération
ne dépend que de GM : passer par la masse et G ne fait rien perdre, l'incertitude de G s'en va. (Le 1.00000000000119 vient des valeurs publiées,
arrondies à 11 chiffres ; l'incertitude, elle, s'annule exactement.)

### Les erreurs

```text
unit error: the value is in m, its uncertainty in s (in main, line 2)
semantic error: cannot initialize float 'w' with measure: value(m) gives its value (line 3)
semantic error: min() is not defined for a measure: use value(m) to work on its value (line 3)
semantic error: ± writes a measure from two numbers (value ± uncertainty), got measure ± float (line 3)
Clair runtime error: the uncertainty after ± must be a number, 0 or more, got -0.1 (line 5)
```

### Ce qui reste à faire

- Pas de listes, de dictionnaires ni de matrices de mesures.
- Le calcul est au premier ordre (comme le GUM et le module Python `uncertainties`) : pour une
  incertitude grande devant la courbure de la fonction (`sin` d'un angle connu à 1 rad près), il
  faudrait une méthode de Monte-Carlo.
- Une mesure prend 272 octets (16 sources) : un calcul en mesures est plus lent qu'en `float`. Les
  programmes sans mesure ne changent pas : leur C est identique à la V0.52.

## Tests

- `mesures.cl` : une chute, un pendule (champs `measure` d'une classe), `x - x`, `x / x`, `hypot`,
  `atan2`, `pow`, `G`, `m_e`, `alpha`, `c` de CODATA 2022, `eps0 mu0 c²`, le rayon de Bohr calculé
  avec son incertitude publiée, la masse de la Terre, la vitesse de libération, 100 mesures
  additionnées, un `float` qui reste un `float` ;
- trois programmes refusés (unités différentes, mesure dans un `float`, `min` d'une mesure) et une
  incertitude négative qui arrête le programme avec sa ligne ;
- les 14 constantes de `phys` qui ne sont pas exactes redonnent leur incertitude publiée par CODATA
  2022 (vérifié avec les valeurs de SciPy) ;
- 2000 programmes tirés au hasard (mesures écrites avec `±` et `+/-`, constantes de `phys`,
  nombres, `+ - * /`, 14 fonctions, fonctions qui reçoivent et rendent des mesures, mêmes mesures
  réutilisées) : la valeur, l'incertitude et le texte sont comparés à un calcul exact en Python
  (mpmath à 80 chiffres, dérivées prises par différences finies le long de chaque source, une
  méthode indépendante de celle de Clair). 1414 programmes restent dans le domaine des fonctions
  (les autres prennent par exemple le logarithme d'un nombre négatif) : plus de 4500 mesures, toutes
  justes. Seules ont été mises de côté celles qu'un `float` ne peut pas calculer (le sinus d'un
  angle de 10²⁸ radians, des nombres plus petits que 10⁻¹⁵⁰) ;
- le C des 157 programmes de test et de mesure est identique à la V0.52.
