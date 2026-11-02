# Clair V0.64

## `pluie.cl` : le temps qui vient de l'ouest

En France, le mauvais temps arrive surtout de l'Atlantique : ce qu'il fait à Rennes annonce souvent ce
qu'il fera à Paris quelques heures plus tard. Le réseau de `exemples/meteo/pluie.cl` regarde maintenant
les deux lieux.

- **Deux lieux** : Paris (`lieu()`) et Rennes à l'ouest (`ouest()`), à changer en haut du fichier. Les
  heures des deux lieux sont mises en face l'une de l'autre (une heure manquante d'un côté ne décale pas
  l'autre).
- **Quinze ans** : de 2010 à 2024, au lieu de six. Le réseau apprend sur 2010 à 2023 et il est mesuré sur
  2024, qu'il n'a jamais vue.
- **La direction du vent** : Open-Meteo la donne en degrés ; le réseau voit le vent comme une flèche (sa
  composante est-ouest et sa composante nord-sud), pour que 359° et 1° soient proches.
- **Ce que voit le réseau**, 20 nombres : à Paris, la pression et comment elle a changé en 3 et 12
  heures, l'humidité et comment elle change, la température et comment elle change, les nuages, le vent
  (force et direction), la pluie des 6 dernières heures ; à Rennes, l'écart de pression avec Paris, comment
  elle change, la pluie des 6 dernières heures, l'humidité, les nuages ; la saison et l'heure. 24 neurones
  cachés, 8 passages.
- **Un seuil choisi** : le réseau donne une probabilité de pluie ; à 50 %, il annonce peu de pluies. Le
  programme essaie les seuils de 5 % à 95 % sur les années d'apprentissage et garde celui qui équilibre le
  mieux les pluies trouvées et les fausses alertes (le score F1). Les scores de 2024 sont donnés pour les
  deux seuils, 50 % et celui choisi.
- **Quatre figures** : l'erreur à chaque passage ; le choix du seuil (la part des pluies annoncées et celle
  des fausses alertes selon le seuil) ; les deux semaines de 2024 où il a le plus plu ; la fiabilité.

Les données sont dans `meteo_48.85_2.35.csv` (Paris) et `meteo_48.11_-1.68.csv` (Rennes), environ 7 Mo
chacun, téléchargés la première fois avec `curl`. L'ancien `historique.csv` de la V0.63 ne sert plus et
peut être effacé. Pour d'autres lieux, changer `lieu()` et `ouest()` : les nouveaux fichiers sont
téléchargés, les anciens restent.

Tests, sans Internet : en plus de ceux de la V0.63, l'alignement des heures des deux lieux, et le seuil
choisi (sur des probabilités inventées, il tombe là où il faut).

## Les dictionnaires : une table de hachage

Jusqu'ici, chercher une clé dans un dictionnaire de Clair voulait dire regarder les clés une à une. Avec
cent mille clés, mettre les heures de Rennes en face de celles de Paris prenait plus d'une minute.

Dès 8 clés, un dictionnaire a maintenant aussi une table de hachage : chaque clé se trouve d'un coup.
Rien ne change pour les programmes :

- `d[k]`, `d[k] = v`, `k in d` et `for k in d` s'écrivent pareil ;
- les clés restent dans l'ordre où elles sont venues, comme en Python ;
- les petits dictionnaires (moins de 8 clés) restent comme avant.

`pluie.cl` sur quinze ans de météo de synthèse : 2 secondes au lieu de 66. Un dictionnaire de 200 000 clés se remplit et se
relit en moins d'une seconde (`tests/dict_grand.cl`). Le module `web`, qui range les formulaires, les
cookies et les sessions dans des dictionnaires, en profite aussi.

## Clair Studio : copier la console

Un bouton **Copier** en haut de la console copie tout ce que le programme a affiché, pour le coller
ailleurs (un message, une note). Les figures y sont remplacées par `[figure]`.

## Tests

- `tests/run_tests.sh` :
  - `pluie.cl` sur deux fichiers au format d'Open-Meteo que fabrique `tests/meteo/fabriquer.cl`
    (`fabriquer paris` et `fabriquer ouest` : le même temps, 6 heures plus tôt à l'ouest, avec la
    direction du vent) : les heures lues aux deux lieux (2902 et 2904), l'apprentissage et la mesure, 8
    passages, les scores aux deux seuils, les quatre figures ;
  - `dict_grand` : 200 000 clés écrites puis relues, l'ordre des clés, `in`.
- Le C produit par le compilateur ne change que pour les programmes qui ont un dictionnaire (155
  programmes comparés à la V0.63).
- Les programmes à dictionnaires passent sous AddressSanitizer sans erreur.
