# Clair V0.71.6

Clair Studio devient une **application Android**, avec sa propre icône. Elle affiche le même Studio, mais
sans le navigateur autour.

## Ce qui change

| dans Chrome | dans l'appli Clair Studio |
|---|---|
| le menu d'un texte sélectionné propose aussi Partager, Rechercher, Traduire, Gemini… | Couper, Copier, Coller, Tout sélectionner, rien d'autre |
| barre d'adresse qui apparaît et disparaît | tout l'écran pour le Studio |
| tirer vers le bas recharge la page | rien ne se recharge par accident |
| Coller (la touche du Studio) demande la permission, ou ne marche pas | Copier et Coller passent directement par le presse-papiers d'Android |
| le bouton retour quitte la page | le bouton retour met l'appli de côté : le Studio reste ouvert |
| les liens vers d'autres sites | s'ouvrent dans le navigateur |

Les barres du téléphone prennent la couleur du Studio, en thème clair comme en sombre.

## Lancer le Studio

Deux façons, au choix :

- **Toucher l'icône Clair Studio.** Si le Studio ne tourne pas, l'appli demande à Termux de le lancer, dans
  une nouvelle session de Termux (nommée « Clair Studio »), puis elle l'affiche. La première fois, Android
  demande d'autoriser Clair Studio à « exécuter des commandes dans l'environnement Termux ».
- **Dans Termux, comme avant** : `sh ~/clair/tools/studio/studio.sh`. Le Studio s'ouvre dans l'appli au
  lieu du navigateur.

Pour garder le navigateur : `CLAIR_STUDIO_APP=0 sh ~/clair/tools/studio/studio.sh`.

## L'installation

`studio.sh` s'occupe de tout, comme il le fait déjà pour compiler Clair. Quand le téléphone n'a pas l'appli,
ou en a une version plus ancienne, il :

1. installe la première fois les outils de Termux qui construisent une appli Android, soit environ 300 Mo :
   - Java (`openjdk-21`) ;
   - `d8`, qui traduit les classes Java pour Android ;
   - `aapt` et `aapt2`, pour les ressources ;
   - `apksigner` ;
   - `ecj`, pour ses classes d'Android ;
2. construit l'appli sur le téléphone, en quelques secondes ;
3. la propose à l'installation. Android demande de confirmer, et la première fois d'autoriser Termux à
   installer des applis.

L'appli est proposée une seule fois par version. Si tu as refusé, `sh ~/clair/tools/studio/android/fabriquer.sh`
la propose de nouveau. Le détail de la construction est dans `~/clair_android.log`.

La commande de mise à jour de la V0.71.6 fait tout cela d'elle-même.

L'appli est signée par une clé créée sur le téléphone, `~/.clair_studio.jks`. Cette clé ne quitte pas le
téléphone et ne va pas sur GitHub. Android n'accepte une mise à jour de l'appli que si elle est signée par
la même clé. Si la clé est perdue, par exemple sur un nouveau téléphone, il faut désinstaller l'ancienne
appli d'abord.

## La clé du Studio

L'adresse du Studio contient une clé, `?t=…` : seul celui qui l'a peut entrer. L'appli l'obtient de deux
façons :

- **depuis Termux** : `studio.sh` lui passe l'adresse complète ;
- **depuis l'icône** : elle demande à Termux de lire `tools/studio/data/jeton`.

Elle garde ensuite l'adresse pour les fois suivantes.

Pour que l'appli puisse passer des commandes à Termux, `fabriquer.sh` ajoute `allow-external-apps = true`
dans `~/.termux/termux.properties`. Une appli ne peut s'en servir que si tu lui as donné la permission
« exécuter des commandes dans l'environnement Termux ». Sans cette permission, l'appli marche quand même :
il suffit de lancer le Studio depuis Termux.

## Les fichiers

`tools/studio/android/` :

| fichier | rôle |
|---|---|
| `AndroidManifest.xml` | le nom, l'icône, les permissions ; `versionCode` augmente à chaque changement de l'appli |
| `java/clair/studio/Studio.java` | toute l'appli, environ 600 lignes |
| `res/` | l'icône (un C et un curseur) et le thème |
| `fabriquer.sh` | installe les outils, construit, signe et propose l'APK |

Dans `Studio.java` :

- **Le menu de sélection** :
  - `setDisabledActionModeMenuItems` retire Partager, Rechercher sur le Web et les applis qui traitent le
    texte (Traduire, Gemini…) ;
  - `setTextClassifier(NO_OP)` retire Appeler et Itinéraire ;
  - une dernière vérification cache tout ce qui n'est pas Couper, Copier, Coller ou Tout sélectionner.
- **`window.ClairAndroid`**, ce que la page peut demander à l'appli :
  - `copier(texte)` et `coller()`, le presse-papiers ;
  - `barres(sombre)`, les couleurs des barres du téléphone ;
  - `reconnecter()`, quand le Studio répond 403 parce que la clé ne vaut plus.

`studio.js` s'en sert quand la page est dans l'appli, et fait comme avant dans un navigateur.

## Tests

- **Construction** : l'appli est construite dans le bac à sable avec la même suite d'étapes que
  `fabriquer.sh`. Seuls les outils viennent d'ailleurs, ceux d'Apktool, de dex2jar et d'uber-apk-signer :
  - `aapt2 dump badging` lit `clair.studio` en version 1, Android 10 minimum ;
  - `apksigner verify` accepte la signature ;
  - `zipalign -c` vérifie l'alignement.
- **Ouverture** : `tests/run_studio.sh` vérifie qu'au démarrage le Studio ouvre l'appli quand `studio.sh`
  l'a trouvée installée, et le navigateur sinon. `am` et `termux-open-url` y sont imités : rien ne s'ouvre
  vraiment.
- **Pas testé** : l'appli elle-même ne peut tourner que sur un vrai Android. Le journal de la mise à jour
  dit si elle a été construite et installée.
