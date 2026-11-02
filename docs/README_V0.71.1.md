# Clair V0.71.1

Le module `nbody` peut compter la **relativité générale** du Soleil.

```
let s: Simulation = nbody.solar_system()
s.c = nbody.c_solar()        // la vitesse de la lumière, en UA par an : la relativité est comptée
s.run(100.0)
```

## Pourquoi

La gravité de Newton suffit presque partout. Près du Soleil, elle se trompe un peu, et l'écart s'accumule :

- en cent ans, la relativité déplace Mercure de 7 300 km, et la Terre de 6 100 km ;
- l'orbite de Mercure tourne de 43 secondes d'arc par siècle de plus que ce que dit Newton. C'est
  l'écart que la relativité d'Einstein a expliqué en 1915 ;
- un astéroïde suivi pendant deux ans et demi jusqu'à un passage près de la Terre : sans la relativité, la
  distance du passage est fausse d'environ 130 km.

Les éphémérides de la NASA (JPL) comptent la relativité. Pour comparer une simulation à leurs prévisions,
il faut donc la compter aussi.

## Comment

`s.c` est la vitesse de la lumière, exprimée dans les unités de la simulation. Elle vaut 0 par défaut : sans
relativité, comme avant.

| unités | `s.c` |
|---|---|
| UA, années, masses du Soleil (`nbody.simulation_solar()`, `nbody.solar_system()`) | `nbody.c_solar()` = 63 241,077 |
| mètres, secondes, kilogrammes (`nbody.simulation(nbody.G())`) | `nbody.c()` = 299 792 458 |
| UA et jours | `299792.458 * 86400.0 / 149597870.7` = 173,14 |

La relativité est celle du premier corps (l'étoile) sur chacun des autres. C'est le premier terme
post-newtonien : a = G M / (c² r³) × ((4 G M / r − v²) r + 4 (r·v) v), où r et v sont la position et la
vitesse par rapport à l'étoile. L'étoile reçoit la poussée opposée, pondérée par les masses : la quantité
de mouvement est gardée.

- **Seulement avec IAS15.** Cette force dépend des vitesses, et IAS15 prédit aussi les vitesses à
  l'intérieur de son pas. Le saute-mouton, Yoshida, WHFast et MERCURIUS supposent des forces qui ne
  dépendent que des places. Avec eux, Clair refuse `s.c` et dit pourquoi.
- Le calcul prend environ 25 % de temps en plus, pour le même nombre de pas.
- `s.save()` garde `s.c` et `nbody.load()` le relit.
- `s.energy()` reste l'énergie de Newton. Avec la relativité, elle oscille un peu (de l'ordre de v²/c²,
  soit 1e-8), sans dériver.

Sources :

- A. Einstein, L. Infeld et B. Hoffmann, « The gravitational equations and the problem of motion », Annals
  of Mathematics 39 (1938) 65. La formule ci-dessus est la limite de leurs équations quand un seul corps,
  le Soleil, est très lourd.
- C. M. Will, *Theory and Experiment in Gravitational Physics* : paramètres post-newtoniens β = γ = 1,
  coordonnées harmoniques, celles des éphémérides du JPL.

Le code est écrit pour Clair. REBOUNDx (licence GPL), l'extension de REBOUND pour les forces en plus, sert de
référence, et rien n'en est recopié.

## Vérifié

**Mercure**, seule autour du Soleil, 100 ans, IAS15 : son orbite tourne de 42,9806 secondes d'arc par
siècle. La formule d'Einstein, 6π G M / (c² a (1 − e²)) par tour, donne 42,9805. Sans `s.c`, l'orbite ne
tourne pas.

**Système solaire, 100 ans** (`nbody.solar_system()`). Écart entre les places, en km, comparé à REBOUND 5.2.2
avec REBOUNDx 5.1.0 :

| | Clair contre REBOUNDx `gr` | Clair contre REBOUNDx `gr_full` | effet de la relativité |
|---|---|---|---|
| Mercure | 0,045 | 3,5 | 7 323 |
| Vénus | 0,098 | 3,8 | 9 142 |
| Terre | 0,008 | 1,6 | 6 093 |
| Jupiter | 0,29 | 0,86 | 497 |

- `gr` : la relativité du Soleil seul, comme Clair.
- `gr_full` : la relativité de tous les corps les uns sur les autres. Elle ajoute quelques kilomètres en
  cent ans, soit moins d'un millième de l'effet.
- Sans relativité, Clair et REBOUND donnent les mêmes places à 1 mètre près.

**Un passage près de la Terre** : un astéroïde de test arrive à 38 011 km du centre de la Terre après
deux ans et demi (Newton seul). Les distances avec la relativité :

| | distance la plus courte |
|---|---|
| Clair | 38 140,212 km |
| REBOUND avec la même formule | 38 140,212 km |
| REBOUNDx `gr_full` | 38 140,267 km |
| REBOUNDx `gr` | 38 143,5 km |

La relativité ajoute 129 km à ce passage. Les façons de la compter s'accordent à 55 m près.

## Tests

`tests/nbody_relativite.cl` vérifie :

- la rotation de l'orbite de Mercure : 42,98 secondes d'arc par siècle, à 0,001 près de la formule
  d'Einstein (et 0 sans `s.c`) ;
- le système solaire sur 100 ans : la taille de l'effet, et les places de REBOUNDx `gr` à 1 km près ;
- la quantité de mouvement ;
- `s.c`, sauvé puis relu.

ASan et UBSan ne trouvent rien.

## Apophis

Le bloc 2 de cette mise à jour simule le passage d'Apophis près de la Terre le 13 avril 2029. Il part des
vraies positions du 1er novembre 2026, téléchargées depuis JPL Horizons, puis compare le résultat avec la
prévision du JPL.
