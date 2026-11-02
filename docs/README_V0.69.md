# Clair V0.69

Deux ajouts au module `nbody`, toujours avec REBOUND comme modèle :

- **WHFast**, l'intégrateur symplectique de Wisdom et Holman, pour suivre des planètes sur des
  millions d'années ;
- **sauver et reprendre** une simulation (`s.save(...)`, `nbody.load(...)`), au dernier chiffre près.

```
import nbody

def main() -> int {
    let s: Simulation = nbody.solar_system()
    s.integrator = "whfast"
    s.dt = 0.01                          // 3,65 jours
    s.run(100000.0)                      // cent mille ans
    s.save("systeme_100000_ans.txt")
    print(s.eccentricity(1, 0))          // l'excentricité de Mercure
    return 0
}
```

## WHFast

```
s.integrator = "whfast"
s.dt = 0.01
```

C'est pour un corps dominant (le premier ajouté, une étoile) et des corps qui tournent autour, comme un
système planétaire. Chaque corps suit exactement son orbite de Kepler autour du centre de masse des corps
d'avant (coordonnées de Jacobi) ; les attractions des planètes entre elles s'ajoutent en petites
poussées entre deux morceaux d'orbite. Cette méthode ne laisse pas l'énergie dériver, même sur des
millions de tours. Son erreur, bornée, vaut à peu près (masse des planètes / masse de l'étoile) ×
(pas / période)².

- **Le pas est fixe.** Il faut environ un vingtième de l'orbite la plus courte, ou moins. Sans `s.dt`,
  Clair prend un quarantième de l'orbite la plus courte : 2,9 jours pour le système solaire.
- **Ajouter les corps du centre vers l'extérieur.** `nbody.solar_system()` le fait déjà.
- **Les rencontres proches ne lui conviennent pas** (une comète qui frôle une planète). Pour ça, IAS15
  reste le bon choix. Les collisions (`s.merge`) ne marchent pas avec WHFast.
- La fenêtre (`s.show()`) marche avec WHFast.

Sources : J. Wisdom et M. Holman, « Symplectic maps for the N-body problem », AJ 102 (1991) 1528 ;
H. Rein et D. Tamayo, « WHFast: a fast and unbiased implementation of a symplectic Wisdom-Holman
integrator for long-term gravitational simulations », MNRAS 452 (2015) 376 ; J. M. A. Danby,
*Fundamentals of Celestial Mechanics* (variables universelles, fonctions de Stumpff). L'équation de
Kepler est résolue en variables universelles, ce qui couvre aussi bien les ellipses que les hyperboles.
La méthode de Newton est employée d'abord, puis celle de Laguerre-Conway, et enfin une bissection en
dernier recours. Le code est écrit pour Clair à partir de ces sources ; REBOUND (licence GPL) a servi à
comparer les résultats, et rien n'en est recopié.

## Vérifié

Système solaire (`nbody.solar_system()`), pas de 0,005 an, 1 000 ans :

| | Clair | REBOUND 5.2.2 |
|---|---|---|
| premier pas | identique au dernier chiffre | |
| écart relatif de l'énergie | 1,365e-10 | 1,364e-10 |
| écart à la méthode calculée en 80 bits (Mercure) | 7e-10 UA | 8e-8 UA |
| temps (cette machine) | 0,34 s | 0,41 s |

L'écart d'énergie est l'erreur de la méthode elle-même : il est le même dans les deux programmes.

Les deux programmes calculent la même chose, mais leurs arrondis s'accumulent différemment. Pour savoir
lequel reste le plus près du calcul exact, la même méthode a été refaite avec des nombres de 80 bits
(64 chiffres binaires au lieu de 53). Clair reste à 7e-10 UA de ce calcul, environ 100 fois plus près
que REBOUND sur cet essai.

Autres essais :

- **Deux corps.** WHFast doit alors suivre l'orbite de Kepler exactement, quel que soit le pas. Sur
  une ellipse e = 0,9 avec un pas de 0,3 (1 000 pas), le demi-grand axe est gardé à 8e-13 (REBOUND :
  3e-13). L'heure est juste à 5e-9 degré, comme REBOUND.
- **Un million de pas sur e = 0,1.** Le demi-grand axe s'écarte de 2,4e-13, contre 3,4e-13 pour
  REBOUND : les arrondis ne s'accumulent pas plus que dans REBOUND.
- **100 000 ans de système solaire** (pas de 0,01 an) : 16,5 secondes sur cette machine, l'écart
  d'énergie ne dépasse pas 7,3e-10. Le téléphone est plus lent, de deux à quatre fois selon le modèle.

## Sauver et reprendre

```
s.save("simulation.txt")
let s2: Simulation = nbody.load("simulation.txt")
```

Le fichier est un texte qu'on peut lire : les réglages, une ligne `clé valeur` chacun, puis un corps par
ligne : `nom<tab>m x y z vx vy vz rayon couleur`. Les nombres sont écrits avec 17 chiffres, ce qui
donne les mêmes nombres au dernier bit quand on les relit.

Une simulation reprise continue exactement comme si elle ne s'était pas arrêtée avec WHFast, le
saute-mouton et Yoshida. IAS15 recommence ses prédictions, ce qui change les derniers chiffres (5e-11
sur 500 ans de système solaire).

## Tests

`tests/nbody_whfast.cl` vérifie :

- deux corps, ellipse e = 0,9 et hyperbole : Kepler exact avec un grand pas ;
- le système solaire sur 1 000 ans, comparé au calcul en 80 bits à 1e-8 UA ;
- le pas par défaut ;
- `s.save()` et `nbody.load()` au dernier chiffre, avec une suite identique.

`tests/nbody_ias15_fenetre.cl` montre maintenant aussi WHFast dans la fenêtre.

## Ce qui manque encore par rapport à REBOUND

Prévu ensuite :

- l'arbre de Barnes-Hut et plusieurs cœurs, pour des milliers de corps.

Pas prévu pour l'instant :

- les correcteurs symplectiques de WHFast, et ses autres coordonnées (héliocentriques démocratiques, WHDS) ;
- les intégrateurs hybrides (MERCURIUS, TRACE) ;
- les équations variationnelles et le MEGNO ;
- les forces en plus.

IAS15 ne reprend pas de façon parfaitement identique après `nbody.load()`, parce que ses prédictions ne
sont pas sauvées.
