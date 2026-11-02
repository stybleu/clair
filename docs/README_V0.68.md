# Clair V0.68

Le module `nbody` monte d'un cran, avec pour modèle **REBOUND** (H. Rein et al.), l'outil de référence
des N-corps en astronomie :

- **IAS15**, l'intégrateur par défaut de REBOUND : ordre 15, il choisit lui-même la longueur de chaque
  pas, et son erreur reste au niveau des derniers chiffres des nombres, même quand des corps se frôlent ;
- les **éléments orbitaux** : ajouter un corps par son orbite (a, e, i, Ω, ω, f) et relire l'orbite d'un corps ;
- le **système solaire** prêt à l'emploi, en unités astronomiques, masses solaires et années.

```
import nbody

def main() -> int {
    let s: Simulation = nbody.solar_system()     // le Soleil et les huit planètes, le 1er janvier 2000
    s.run(1000.0)                                // mille ans, avec IAS15
    print(s.semi_major_axis(5, 0))               // le demi-grand axe de Jupiter, en UA
    s.show()                                     // dans la fenêtre (Termux:X11)
    return 0
}
```

## IAS15

```
let s: Simulation = nbody.simulation(1.0)
s.integrator = "ias15"
```

`s.run(t)` avance exactement de `t`, en pas que l'intégrateur choisit (courts quand des corps se
rapprochent, longs sinon). `s.step()` fait un seul pas, de la longueur qu'il choisit. `s.dt` garde le
pas atteint, ce qui permet à la suite de repartir de là. `s.epsilon` règle la précision (1e-9 par
défaut, comme REBOUND ; 0 garde le pas `s.dt` fixe). `s.steps_done(0)` donne le nombre de pas faits, et `s.steps_done(1)` les pas
refaits parce qu'ils étaient trop longs. La fenêtre (`s.show()`) marche aussi avec IAS15.

Les intégrateurs d'avant restent là, et restent par défaut, pour que les programmes déjà écrits donnent
les mêmes résultats : `s.integrator = "leapfrog"` (le saute-mouton, ordre 2) ou `"yoshida"` (ordre 4,
comme `s.order = 4`).

Comment il marche : dans un pas, l'accélération est un polynôme de degré 7 du temps. Ses coefficients
sont trouvés en recalculant les forces aux 7 points de Gauss-Radau du pas, jusqu'à ce qu'ils ne bougent
plus. La longueur du pas suivant vient de l'accélération, de sa dérivée et de sa dérivée seconde en fin
de pas (critère de Pham, Rein et Spiegel 2024). Les sommes de nombreux petits termes gardent ce que
l'arrondi perd (sommation de Kahan) : positions, vitesses, coefficients et temps.

Sources : H. Rein et D. Spiegel, « IAS15: a fast, adaptive, high-order integrator for gravitational
dynamics, accurate to machine precision over a billion orbits », MNRAS 446 (2015) 1424 ; T. Pham,
H. Rein et D. Spiegel, « Re-evaluating the timestep criterion of IAS15 », OJAp 7 (2024). Le code est
écrit pour Clair à partir de ces articles. REBOUND, sous licence GPL, a servi à comparer les résultats ;
rien n'en est recopié (Clair est sous licence MIT). Les constantes (points de Gauss-Radau et passages entre les deux formes du polynôme)
sont recalculées à partir des polynômes de Legendre, avec 60 chiffres.

## Vérifié contre REBOUND

REBOUND 5.2.2, compilé à part (il n'est pas dans Clair), avec IAS15 et ses réglages par défaut. Mêmes
conditions de départ des deux côtés :

| Essai | Clair | REBOUND |
|---|---|---|
| Kepler e = 0,9, 10 000 tours : pas | 971 575 | 971 575 |
| écart du demi-grand axe | 4e-15 | 1e-13 |
| écart de l'excentricité | 2e-15 | 1e-14 |
| retard sur l'orbite | 1,7e-7 degré | 2,4e-7 degré |
| Système solaire, 1 000 ans : pas | 159 603 | 159 603 |
| écart relatif de l'énergie | 8e-16 | 6e-16 |
| écart entre les positions des deux | 1,6e-10 UA au plus (Mercure) | |
| Problème pythagoricien jusqu'à t = 70, 20 essais : écart médian de l'énergie | 7e-11 | 5e-11 |

Les deux font les mêmes pas, de la même longueur, et arrivent aux mêmes positions à 23 mètres près pour
Mercure après mille ans. Le problème pythagoricien est chaotique : deux calculs qui diffèrent au 15e
chiffre finissent sur des trajectoires différentes, d'où la comparaison sur 20 essais.

## Les orbites

```
let s: Simulation = nbody.simulation(1.0)
let soleil: int = s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
s.add_orbit("Comète", 0.0, soleil, 3.0, 0.8, 10.0, 0.0, 90.0, 0.0)
let el: list[float] = s.elements(1, soleil)
```

`s.add_orbit(nom, masse, autour, a, e, i, Ω, ω, f)` : `a` le demi-grand axe, négatif pour une hyperbole
(e > 1) ; `e` l'excentricité. En degrés : `i` l'inclinaison, `Ω` la longitude du nœud ascendant, `ω`
l'argument du péricentre, `f` l'anomalie vraie. `autour` vaut -1 pour tourner autour du centre de masse
de tous les corps déjà là, comme REBOUND, ce qui donne des orbites emboîtées (coordonnées de Jacobi).
Pour partir de l'anomalie moyenne `M`, on utilise `nbody.true_anomaly(M, e)`, qui résout l'équation de
Kepler.

`s.elements(i, j)` donne l'orbite du corps `i` autour du corps `j` (-1 : autour du centre de masse des
corps d'avant) : `[a, e, i, Ω, ω, f, M, période]`. Raccourcis : `s.semi_major_axis(i, j)`,
`s.eccentricity(i, j)`, `s.inclination(i, j)`, `s.period(i, j)`.

## Le système solaire et les unités

- `nbody.G_solar()` : G en UA³ / (masse solaire × an²) = 39,476926421093573. Il vient de GM du Soleil
  = 1,32712440041e20 m³/s² (UAI 2009, TDB), 1 UA = 149 597 870 700 m (UAI 2012) et 1 an = 365,25 jours.
- `nbody.simulation_solar()` : une simulation vide dans ces unités, avec IAS15.
- `nbody.solar_system()` : le Soleil et les huit planètes au 1er janvier 2000 à 12 h (J2000), autour du
  centre de masse, dans le plan de l'écliptique. Les orbites sont la table de JPL valable de 1800 à 2050
  (E. M. Standish, *Keplerian Elements for Approximate Positions of the Major Planets*), et les masses
  celles de l'UAI 2009. La Terre est la Terre et la Lune ensemble. Les positions sont approchées (à
  quelques minutes d'arc) : c'est fait pour étudier la dynamique, pas pour pointer un télescope.

## Ce qui manque encore par rapport à REBOUND

Prévu ensuite :

- **WHFast** : l'intégrateur symplectique rapide pour suivre des systèmes planétaires sur des millions
  d'années ;
- sauver une simulation et la reprendre ;
- l'**arbre de Barnes-Hut** et plusieurs cœurs, pour des milliers de corps.

Pas prévu pour l'instant :

- les intégrateurs hybrides pour les rencontres proches dans de grands systèmes (MERCURIUS, TRACE) ;
- les équations variationnelles et le MEGNO (mesure du chaos) ;
- les forces en plus (relativité, frottement) ;
- les positions exactes venues de JPL Horizons, et une vue en 3D (la fenêtre montre le plan x-y).

## Tests

- `tests/nbody_ias15.cl` :
  - IAS15 sur Kepler e = 0,9 (cent tours : demi-grand axe et excentricité gardés à 1e-13) ;
  - le problème pythagoricien, comparé aux valeurs de REBOUND à t = 10 ;
  - les éléments orbitaux (ellipse, hyperbole, équation de Kepler) ;
  - le système solaire sur cent ans (énergie gardée à 1e-14) ;
  - une collision.
- `tests/nbody_ias15_fenetre.cl` : le système solaire dans la fenêtre, sans écran.

Les tests de la V0.66 (`nbody_module.cl`, `nbody_fenetre.cl`) donnent les mêmes sorties qu'avant.
