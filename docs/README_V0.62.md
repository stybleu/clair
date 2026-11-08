# Clair V0.62

## V0.62.1 : `studio.sh` se suffit à lui-même

```sh
sh ~/clair/tools/studio/studio.sh
```

Avant, sur un appareil où Clair n'avait jamais été compilé (un nouveau téléphone, ou quelqu'un qui vient
de récupérer Clair sur GitHub), `studio.sh` s'arrêtait sur `../../build/clair: not found`. Maintenant :

- si Clair n'est pas compilé, ou si un de ses fichiers sources est plus récent que le compilateur (après
  un `git pull`), `studio.sh` le compile d'abord (`cmake`, puis `cmake --build`), le détail dans
  `~/clair_build.log` ;
- s'il manque `clang`, `cmake` ou `make`, il les installe avec `pkg` (Termux), le détail dans
  `~/clair_install.log` ; ailleurs qu'en Termux, il dit lesquels manquent ;
- puis il compile le Studio s'il le faut, et l'ouvre, comme avant ;
- tous les chemins partent de l'emplacement de `studio.sh` : il marche depuis n'importe quel dossier.

Pour un nouvel utilisateur, trois lignes suffisent (voir le README) :

```sh
pkg install -y git
git clone https://github.com/stybleu/clair ~/clair
sh ~/clair/tools/studio/studio.sh
```


## Clair Studio : thème, taille du texte, explorateur de fichiers

```text
sh ~/clair/tools/studio/studio.sh
```

### Le thème et la taille du texte

En bas du panneau ☰ :

- **Thème** : *Auto* (comme le téléphone, ce qui était le seul choix avant), *Clair* ou *Sombre*. Les
  figures de `plot` suivent le thème choisi.
- **Taille du texte** : A− et A+, de 10 à 26 pixels, pour l'éditeur, la console et les propositions
  (Ctrl + et Ctrl − avec un clavier).

Les réglages sont gardés par le Studio (`tools/studio/data/`) : ils restent après avoir fermé le
navigateur, en changeant de navigateur ou de port. Ils sont aussi appliqués avant le premier affichage,
sans éclair blanc en mode sombre.

### L'explorateur de fichiers

Le panneau ☰ montre maintenant les dossiers :

- les dossiers d'abord (📁), puis les fichiers (◆ pour un programme Clair, 🖼 pour une image) avec leur
  taille ;
- un appui sur un dossier l'ouvre ; `..` ou le chemin en haut (`clair_projets / meteo`) ramènent plus
  haut ; le panneau s'ouvre sur le dossier du fichier ouvert ;
- **+ Fichier** et **+ Dossier** créent dans le dossier affiché ;
- **⋯** sur un fichier ou un dossier : *Renommer ou déplacer* (le chemin entier se change :
  `meteo/outil.cl` devient `outil.cl` pour le remonter) et *Supprimer* (un dossier doit être vide) ;
- une image s'ouvre en grand au lieu d'aller dans l'éditeur.

Un programme d'un sous-dossier tourne **dans son dossier** : il y lit ses fichiers (`read_text("releve.txt")`),
y importe ses modules (`import outil` pour `meteo/outil.cl`), et `plot.image("carte.png")` cherche l'image à
côté de lui. Les erreurs en direct, l'autocomplétion (y compris des modules d'à côté), les tests, le C et
Git fonctionnent pareil dans les sous-dossiers. Le nom affiché en haut est le chemin : `meteo/station.cl`.

## Le module web : `<path:nom>`, comme Flask

Une partie de route `<path:chemin>` prend toute la fin du chemin, `/` compris :

```clair
@web.route("/docs/<path:chemin>")
def docs(chemin: string) -> string {
    return "docs : " + chemin          // /docs/sous/dossier/b.cl : "sous/dossier/b.cl"
}
```

- c'est la dernière partie de la route, et son paramètre est un `string` (sinon une erreur à la
  compilation le dit) ;
- une route plus précise passe avant : `/docs/liste` répond à `/docs/liste`, et `/docs/v/<n>/<path:reste>`
  à `/docs/v/2/x/y` ; les routes à `<path:...>` sont essayées après toutes les autres, la plus longue
  d'abord ;
- comme dans Flask, la fin du chemin n'est pas vide : `/docs` seul ne correspond pas.

Clair Studio s'en sert pour ses fichiers : `/api/lire/<path:nom>`. Le chemin est toujours vérifié : pas
de `..`, pas de nom caché, rien qui commence par `-`.

## Tests

- `tests/run_web.sh` : `<path:chemin>` avec un ou plusieurs dossiers, un `/` encodé (`%2F`), une route
  plus précise qui passe avant, un paramètre `int` avant `<path:...>`, `/docs` seul (404) ;
  `tests/invalid_web_path_milieu.cl` et `invalid_web_path_type.cl` refusés à la compilation ;
- `tests/run_studio.sh` : créer un dossier et un sous-dossier (et refuser un dossier qui existe) ;
  écrire, lister (dossiers d'abord), lire, vérifier et lancer dans un sous-dossier, avec un module et un
  fichier de données à côté (`24`) ; déplacer dans un sous-dossier ; un dossier plein qui ne se supprime
  pas, puis vide qui se supprime ; refusés : lister, créer ou déplacer hors du dossier, un dossier caché ;
  les réglages (départ, gardés, refusés s'ils ne sont pas du JSON) ;
- à part, l'éditeur dans Chromium à la taille d'un téléphone : créer `meteo`, y créer `outil.cl` et
  `station.cl` qui l'importe, aucune erreur en direct, lancer (`42` et une figure), l'autocomplétion de
  `outil.`, le panneau ouvert sur `meteo`, déplacer `outil.cl` à la racine, un dossier plein refusé, le
  thème sombre (fond de la page et des figures), le texte à 17 px (ligne de 26 px), les réglages gardés
  après rechargement et dans un autre navigateur ; aucune erreur JavaScript, rien qui dépasse de l'écran.
