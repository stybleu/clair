# Clair V0.71.8

Troisième étape pour construire l'appli Clair Studio sur le téléphone.

## Ce qui s'est passé avec la V0.71.7

La V0.71.7 est passée : 279 tests, et elle a été envoyée sur GitHub. Ses deux corrections ont marché :

- `d8` a traduit les classes de l'appli sans planter ;
- la question « l'appli est-elle installée ? » a eu sa réponse.

La construction s'est arrêtée à l'étape suivante, celle où `aapt2` lie les ressources de l'appli (son nom,
son icône, son thème) à celles d'Android :

```
error: failed to load include path /system/framework/framework-res.apk.
```

Pour lier, `aapt2` cherche dans les ressources d'Android le numéro de chaque attribut (`android:name`,
`android:icon`…) et de chaque style (`Theme.DeviceDefault.DayNight`). Le téléphone a ces ressources dans
`/system/framework/framework-res.apk`. Mais l'`aapt2` de Termux ne sait pas lire celles d'Android 16.
Android les range maintenant de façon plus compacte, et cet `aapt2` est plus ancien.

## Corrigé : un petit cadre d'Android

L'appli ne se sert que de 31 attributs d'Android et d'un style. Leurs numéros publics ne changent jamais
d'une version d'Android à l'autre : `android:name` vaut toujours `0x01010003`.

- `tools/studio/android/cadre/res/values/cadre.xml` décrit ces 31 attributs et ce style, avec leurs
  numéros, leur type et les valeurs possibles. Par exemple, `launchMode` peut valoir `singleTask` = 2.
- `fabriquer.sh` en fait un petit paquet `android`, avec `aapt2 link -x`, et y lie l'appli à la place de
  `framework-res.apk`. Il n'y a rien à télécharger.
- `cadre/generer.py` réécrit `cadre.xml` à partir d'un `android.jar` complet. Il faut le relancer seulement
  si un fichier XML de l'appli se sert d'un nouvel attribut `android:…`. Le script trouve lui-même la liste
  des attributs dans les fichiers.
- `CLAIR_ANDROID_FRAMEWORK=…/android.jar` permet encore de lier à un vrai `android.jar` à la place du
  cadre.

## Vérifié

L'appli a été liée deux fois dans le bac à sable : au cadre, puis au vrai `android.jar` d'Android 15
(API 35). Ces fichiers sont identiques octet par octet :

- `resources.arsc` ;
- l'icône (`icone.xml`, `icone_avant.xml`).

Le manifeste lié au vrai `android.jar` a seulement quatre attributs de plus, qui ne servent qu'à
l'information : `compileSdkVersion`, `compileSdkVersionCodename`, `platformBuildVersionCode` et
`platformBuildVersionName`.

Toute la construction passe ensuite par `fabriquer.sh` :

- `aapt2 dump badging` lit `clair.studio` en version 2 ;
- `apksigner verify` accepte la signature ;
- `zipalign -c` vérifie l'alignement.


## Android 16 : ouverture de Clair Studio

Sur certains appareils Android 16, Termux ne peut
pas obtenir la version de l'application installee
par les commandes `cmd package` ou `am broadcast`.

`studio.sh` privilegie donc l'ouverture de
l'application Android sans bloquer sur cette
verification.

Si l'application est absente, le navigateur reste
une solution de repli.

La construction, l'installation et le demarrage
sont geres par `fabriquer.sh`. En cas de refus
d'installation automatique, l'APK peut etre
installe depuis les telechargements Android.

La detection automatique des nouvelles versions
installees n'est pas garantie sur Android 16.


## Installation Android : téléchargements

La construction de l'application est effectuée
dans Termux avec :

```sh
sh ~/clair/tools/studio/android/fabriquer.sh
```

Le script construit et signe l'APK, puis le copie
dans le dossier Téléchargements du téléphone.

Il demande ensuite l'ouverture des téléchargements
avec l'action Android standard :

```sh
am start -a android.intent.action.VIEW_DOWNLOADS
```

Cette méthode ne dépend pas du gestionnaire de
fichiers propre à Samsung.

L'utilisateur sélectionne ensuite
`clair-studio.apk` et confirme l'installation.

Si Android refuse d'ouvrir les téléchargements,
le fichier reste accessible depuis le gestionnaire
de fichiers du téléphone.

Cette procédure a été testée sur Android 16
avec un appareil Samsung. La compatibilité avec
les autres fabricants reste à vérifier.

La compilation de Clair et son fonctionnement
dans Termux ne sont pas modifiés.
