# Clair V0.58

## Clair Studio : un IDE pour Clair, écrit en Clair

```text
sh ~/clair/tools/studio/studio.sh
```

Le navigateur du téléphone s'ouvre sur l'éditeur. Tes programmes sont dans `~/clair_projets`
(ou le dossier donné : `sh ~/clair/tools/studio/studio.sh ~/mes_programmes`). Ctrl+C dans Termux
arrête le Studio.

- **▶** enregistre, compile et lance le programme. Ce qu'il écrit s'affiche pendant qu'il tourne.
  **■** l'arrête (une boucle sans fin, par exemple).
- **Les erreurs sont sur leur ligne** : la ligne fautive est en rouge, son numéro aussi, et un appui
  sur `(line 3)` dans la console y emmène et la sélectionne. Pareil pour les erreurs qui arrivent
  pendant que le programme tourne (`Clair runtime error: ... (line 12)`).
- **Les programmes qui demandent** (`input()`, `input_int()`) : une ligne de saisie apparaît sous la
  console, Entrée envoie.
- **L'éditeur** : couleurs du code Clair, numéros de ligne, retrait automatique (Entrée après `{`
  ajoute 4 espaces, `}` les retire), une barre de touches au-dessus du clavier (`{ } ( ) [ ] : " =
  -> < > + - * / ± _ . ,`, annuler, refaire), Ctrl+Entrée lance, Ctrl+S enregistre. Le fichier est
  enregistré tout seul.
- **☰ Fichiers** : ouvrir, nouveau, renommer, supprimer.
- **C** : le C que Clair a écrit pour ton programme.
- Clair ou sombre selon le téléphone.

Pour l'avoir comme une appli : dans Chrome, menu ⋮ puis « Ajouter à l'écran d'accueil ».

### Écrit en Clair

Le serveur du Studio est `tools/studio/studio.cl`, un programme Clair de 230 lignes qui utilise les
modules `web` (V0.55, V0.57) et `os` (V0.56) : il lit et écrit les fichiers, appelle le compilateur,
lance le programme et lit ce qu'il écrit pendant qu'il tourne. L'éditeur est une page HTML
(`tools/studio/templates/index.html`, `tools/studio/static/`). `studio.sh` compile le Studio la
première fois, et de nouveau quand `studio.cl` ou le compilateur change.

### Sécurité

Un IDE qui lance des programmes ne doit répondre qu'à toi :

- il n'écoute que le téléphone (`127.0.0.1`), et n'accepte que les adresses `127.0.0.1` et
  `localhost` (V0.57) ;
- l'adresse affichée dans Termux porte un jeton (`?t=...`, 16 octets au hasard, gardé dans
  `tools/studio/data/`) : la première visite le met dans une session signée, valable 31 jours.
  Sans lui, une autre appli du téléphone ou une page web reçoit 403 ;
- une page d'un autre site ne peut rien lui envoyer (CSRF, V0.57) ;
- seuls les fichiers du dossier des programmes sont lus et écrits (pas de fichier caché, pas de
  `..`).

### Une petite correction du module web

`web.run` écrit maintenant ce que le programme a affiché avant de démarrer le serveur, même quand la
sortie va dans un fichier.

## Tests

- `tests/run_studio.sh` : le Studio interrogé comme par l'éditeur : sans jeton ou avec un mauvais
  jeton, refusé ; avec le jeton, une session ; liste des fichiers ; écrire, lire, lancer un
  programme et lire sa sortie (`un`, `42`, code 0) ; le C ; une erreur de compilation avec
  `(line 3)` ; un programme qui demande un nom ; une boucle sans fin arrêtée (code 137) ; renommer,
  refuser de renommer sur un fichier qui existe, supprimer ; refusés : un fichier hors du dossier
  (`..%2F`), un fichier caché, une page d'un autre site ;
- à part, l'éditeur dans Chromium à la taille d'un téléphone : ouvrir, lancer, une erreur marquée
  puis touchée, un programme qui demande deux fois, une boucle arrêtée, retrait automatique, barre de
  touches, nouveau / renommer / supprimer, le C, aucune erreur JavaScript, rien qui dépasse de la
  largeur de l'écran, clair et sombre.
