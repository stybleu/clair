# Clair V0.63

## Des exemples météo, affichés avec `plot`

Trois programmes dans `exemples/meteo/`, chacun avec ses tests (`clair --test`). Dans Clair Studio : copier
le dossier dans `~/clair_projets`, puis ▶ ou 🧪.

### `lorenz.cl` : l'effet papillon

Le modèle de Lorenz (1963) vient d'un modèle de convection de l'atmosphère : trois équations,
calculées pas à pas par la méthode de Runge-Kutta d'ordre 4.

- la première figure est l'attracteur, le « papillon » (`x` et `z`) ;
- la seconde montre deux départs qui ne diffèrent que d'un millionième : ils restent ensemble, puis
  s'écartent tout à fait après `t = 26.7`. C'est pour cela qu'on ne prévoit pas le temps au-delà de
  quelques jours.

Tests : les points fixes ne bougent pas ; deux pas de temps différents donnent le même résultat ; sans
assez de chaleur (`rho < 1`), l'air se calme.

### `chaleur.cl` : de l'air chaud et de l'air froid emportés par le vent

La température est sur une grille de 64 × 40 cases, comme dans un modèle météo en beaucoup plus petit.
À chaque pas de temps, chaque case échange de la chaleur avec ses voisines : ce que le vent apporte
(advection, en prenant la température de la case d'où vient le vent) et ce qui s'étale tout seul
(diffusion). Le vent est fait de tourbillons tirés d'une « fonction de courant » : il ne crée ni ne
détruit d'air. Le résultat est une animation de 40 images, en couleurs, avec les flèches du vent.

Tests : la chaleur totale se conserve (à 10⁻⁹ près) ; aucune case ne devient plus chaude que la plus
chaude au départ, ni plus froide que la plus froide ; le vent ne crée ni ne détruit d'air.

### `pluie.cl` : un réseau de neurones qui prévoit la pluie

La question : va-t-il pleuvoir (au moins 0,5 mm) dans les 6 prochaines heures ?

- **Les données** : la météo d'un lieu heure par heure de 2019 à 2024 (température, humidité, pression,
  pluie, nuages, vent). Elles viennent d'Open-Meteo, qui donne gratuitement et sans compte la réanalyse
  ERA5 du centre européen (ECMWF). La première fois, le programme les télécharge avec `curl`
  (`pkg install curl`) dans `historique.csv`, environ 2 Mo, puis il relit ce fichier. Le lieu est Paris :
  pour un autre, changer `latitude()` et `longitude()` en haut du fichier et effacer `historique.csv`.
- **Ce que voit le réseau** à chaque heure : la pression et comment elle a changé en 3 et 12 heures,
  l'humidité et comment elle change, la température, les nuages, le vent, la pluie des 6 dernières
  heures, la saison et l'heure du jour (14 nombres).
- **Le réseau** : comme celui de XOR (`tests/xor_training.cl`), avec 16 neurones cachés (`tanh`) et une
  sortie, la probabilité de pluie. Il apprend sur les années 2019 à 2023, en 10 passages.
- **La mesure, honnête** : sur 2024, que le réseau n'a jamais vue, comparé à deux prévisions naïves,
  « toujours sec » et « comme les 6 heures d'avant ». Pour chacune : la réussite, la part des pluies
  annoncées, la part de fausses alertes.
- **Trois figures** : l'erreur à chaque passage (comparée à une probabilité toujours la même) ; les deux
  semaines de 2024 où il a le plus plu, avec la probabilité annoncée et les heures de pluie ; la
  fiabilité (quand il annonce 70 %, pleut-il 7 fois sur 10 ?).

Un petit réseau sur un seul point ne vaut pas Météo-France, qui voit toute l'atmosphère. Mais il apprend
de vrais liens (une pression qui baisse avec un air humide annonce la pluie), et les chiffres de 2024
disent s'il fait mieux que les prévisions naïves.

Tests, sans Internet : le gradient calculé est le bon (comparé à une différence finie) ; le réseau
apprend une règle simple (plus de 95 % de bonnes réponses) ; la lecture d'un fichier d'Open-Meteo (une
valeur manquante laisse l'heure de côté) ; les colonnes peuvent venir dans un autre ordre.

## Le module `plot`

- `plot.crange(bas, haut)` : les valeurs entre lesquelles vont les couleurs de `plot.heatmap` (sinon la
  plus petite et la plus grande du champ). Dans une animation, les mêmes couleurs veulent dire la même
  température d'une image à l'autre ;
- sur une carte en couleurs, les isolignes et les flèches sont foncées avec un liseré clair : elles se
  voient sur toutes les couleurs, en mode clair comme en sombre.

## Tests

- `tests/run_tests.sh` lance les tests des trois exemples, puis chacun en entier :
  - `lorenz.cl` : `t = 26.7` ;
  - `chaleur.cl` : la chaleur totale de la fin est celle du début, 40 images, la même échelle de couleurs
    sur chacune ;
  - `pluie.cl` sur un fichier au format d'Open-Meteo que fabrique `tests/meteo/fabriquer.cl` (quatre
    mois de météo de synthèse, deux valeurs manquantes) : la lecture (2902 heures), les deux années, 10
    passages, les scores, les figures.
- Le programme `pluie.cl` complet a été essayé sur six années de météo de synthèse au même format
  (52 608 heures, une demi-seconde). Le téléchargement depuis Open-Meteo n'a pas pu être essayé là où
  Clair a été écrit (pas d'accès à ce site) : en cas d'échec, le programme affiche l'erreur de `curl`.
