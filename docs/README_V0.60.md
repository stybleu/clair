# Clair V0.60

## Git dans Clair Studio

```text
sh ~/clair/tools/studio/studio.sh
```

Le bouton Git (en haut, à côté de **C**) montre les modifications de ton dossier de programmes et en
garde les versions. Il porte le nombre de fichiers modifiés depuis la dernière version.

- **Créer un dépôt** : la première fois, le dossier n'a pas de dépôt Git ; un appui sur « Créer un
  dépôt Git dans ce dossier » le crée (branche `main`).
- **Changements** : chaque fichier nouveau (N), modifié (M), supprimé (D) ou renommé (R). Un appui
  montre ce qui a changé, en couleurs. « Annuler mes changements » remet le fichier comme dans la
  dernière version (après confirmation).
- **Enregistrer une version** : écris ce qui a changé, puis « Enregistrer une version » (un commit de
  tout le dossier). La première fois, le Studio demande ton nom et ton adresse mail pour signer les
  versions (gardés dans le dépôt du dossier).
- **Historique** : les versions, la plus récente en haut, avec leur auteur et leur date (« il y a 5
  min », « hier à 14:02 »). Un appui montre ce qu'elle a changé ; « Remettre bonjour.cl comme dans
  cette version » ramène le fichier ouvert à cet état (tu peux ensuite enregistrer ce retour comme une
  nouvelle version, ou l'annuler).
- **GitHub** : « Relier à GitHub… » demande l'adresse du dépôt (`https://github.com/toi/projet.git`,
  `git@github.com:...` ou `ssh://...`). Ensuite **↑ Envoyer** (`git push`) et **↓ Récupérer**
  (`git pull --ff-only`, puis le fichier ouvert est relu). Le Studio ne pose jamais de question dans
  Termux : si GitHub demande qui tu es, il l'affiche et rappelle de faire une fois `pkg install gh` puis
  `gh auth login` (ou d'utiliser une clé SSH).
- **La marge de l'éditeur** : les lignes ajoutées depuis la dernière version en vert, les lignes
  modifiées en bleu, et un trait rouge là où des lignes ont été retirées. Elle se met à jour à chaque
  enregistrement automatique.

Si Git n'est pas installé, le bouton le dit : `pkg install git`.

### Comment c'est fait

Le serveur du Studio (`tools/studio/studio.cl`, en Clair) lance `git -C dossier ...` avec le module
`os` ; l'éditeur lit ce que Git écrit (`status --porcelain`, `diff`, `log`). Pour la sécurité :

- seuls les fichiers du dossier des programmes : pas de `..`, pas de nom caché, rien qui commence par
  `-` (Git le prendrait pour une option), et toujours `--` avant les fichiers ;
- un numéro de version n'est accepté que s'il est fait de 4 à 40 chiffres hexadécimaux ;
- l'adresse du dépôt distant doit commencer par `https://`, `git@`, `ssh://` ou `/` (un dossier) :
  les adresses `ext::` qui lanceraient une commande sont refusées ;
- `GIT_TERMINAL_PROMPT=0` : Git ne reste jamais bloqué à attendre un mot de passe, et l'envoi comme la
  récupération s'arrêtent au bout de 90 s ;
- comme le reste du Studio, tout passe par le jeton et la session (V0.58).

Si le dossier des programmes est à l'intérieur d'un autre dépôt Git, le Studio utilise ce dépôt, mais ne
montre et n'enregistre que les fichiers du dossier.

## Tests

- `tests/run_studio.sh` : pas de dépôt, puis le créer ; un fichier nouveau et sa différence ; une
  version refusée sans message ; l'identité ; deux versions ; l'historique ; une version montrée ; un
  fichier modifié, sa différence et ses lignes changées ; remettre un fichier comme dans la première
  version, puis annuler ; relier à un dépôt distant (un dossier), envoyer, une version ajoutée ailleurs
  puis récupérée ; refusés : une adresse `ext::`, un fichier hors du dossier, une option à la place
  d'un fichier ou d'une version, l'état sans jeton ;
- à part, l'éditeur dans Chromium à la taille d'un téléphone (360 px) : créer le dépôt, la différence
  d'un fichier nouveau, une version, la marge (lignes modifiées et ajoutées) et le nombre sur le bouton,
  la différence en couleurs, une deuxième version, l'historique, remettre la première version puis
  annuler, aucune erreur JavaScript, rien qui dépasse de l'écran.
