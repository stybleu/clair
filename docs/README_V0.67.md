# Clair V0.67

## Le designer de Clair Studio : des applications dessinées, comme Visual Basic

On dessine une fenêtre avec le doigt, on touche deux fois un bouton, et Clair Studio écrit la fonction qui
lui répond. L'application tourne dans le navigateur, comme une appli du téléphone : vrais champs de texte,
clavier Android, polices nettes. (Pour les jeux et le dessin en temps réel, c'est le module `gui` avec
Termux:X11.)

### Faire une application

1. Dans les fichiers (☰), **+ Fenêtre**, puis un nom : `Accueil`. Le Studio crée `Accueil.ui` (le dessin),
   `Accueil.cl` (le programme) et `Accueil.html` (la page), et ouvre le dessin.
2. En haut, la palette : **+ Bouton**, **+ Texte**, **+ Saisie** (un champ à remplir), **+ Case** (à cocher),
   **+ Curseur**, **+ Liste**, **+ Choix** (une liste qui se déroule), **+ Image**, **+ Minuteur**.
3. On glisse un contrôle pour le déplacer, on tire le rond orange pour changer sa taille. En bas, ses
   propriétés : son nom, son texte, sa place, sa taille, ses couleurs, visible, actif, et ce qui lui est propre
   (les éléments d'une liste, le minimum et le maximum d'un curseur, l'intervalle d'un minuteur...). Toucher le
   fond règle la fenêtre elle-même : son titre, sa taille, ses couleurs.
4. **Toucher deux fois un contrôle** : le Studio écrit sa fonction dans `Accueil.cl` et y emmène, le curseur à
   l'intérieur. Les boutons du panneau (`bouton1_click →`...) font de même pour chaque événement.
5. ▶ : le Studio remet le dessin et le code d'accord, compile, lance. La console montre **Ouvrir
   l'application ↗** : l'application s'ouvre dans un nouvel onglet. ■ l'arrête. Le bouton ▣ en haut passe du
   code au dessin, `{ }` du dessin au code.

### Le code

```
// quand on touche bouton1
def bouton1_click(f: Accueil) -> int {
    if not f.saisie1.is_number() {
        f.fenetre.message = "Écris un nombre"
        return 0
    }
    f.etiquette1.text = "Le double : " + forms.text_of(2.0 * f.saisie1.number())
    f.liste1.add(f.saisie1.text)
    return 0
}
```

- `f` est la fenêtre : `f.bouton1`, `f.saisie1`... et `f.fenetre` pour la fenêtre elle-même.
- Ce qu'on peut lire et changer, pour chaque contrôle : `text`, `value` (un curseur), `minimum`, `maximum`,
  `step`, `checked` (une case), `items` et `selected` (une liste, un choix), `visible`, `enabled`, `x`, `y`,
  `width`, `height`, `color`, `background`, `size` (la taille du texte), `interval` (un minuteur, en
  millisecondes), `source` (une image : un fichier de `static/`).
- Et : `choice()` (l'élément choisi), `add(texte)`, `remove(i)`, `clear()`, `show()`, `hide()`,
  `move(x, y)`, `is_number()` et `number()` (le texte comme nombre, avec une virgule ou un point).
- `f.fenetre.message = "..."` : une boîte de message, comme `MsgBox`. `f.fenetre.text` : le titre.
- Les événements : `_click` (bouton, texte, image), `_change` (saisie, case, curseur), `_enter` (Entrée dans
  une saisie), `_select` (liste, choix), `_tick` (minuteur), et `fenetre_load` quand la page s'ouvre.

Le début de `Accueil.cl`, jusqu'à la ligne « fin de ce qu'écrit le designer », est écrit par le designer :
la classe `Accueil` (un `Control` par contrôle), les routes du module `web` et l'appel des fonctions. Il est
réécrit à chaque changement du dessin et à chaque ▶ ; tout ce qui suit est à toi et n'est jamais touché.

### Comment ça marche

À chaque événement, la page envoie l'état de tous les contrôles ; le programme Clair refait la fenêtre avec,
appelle la fonction de l'événement, et renvoie le nouvel état, que la page montre. Le programme ne garde donc
rien entre deux événements (Clair n'a pas de variables globales) : tout est dans les contrôles. Pour garder
quelque chose, un contrôle caché fait l'affaire, ou `web.save` et `web.load`.

L'application sert sur `http://127.0.0.1:8100/`. La page est faite pour un téléphone tenu debout (360 points
de large au départ) et se met à la largeur de l'écran.

## Le module `forms`

`import forms` (le designer l'écrit) : la classe `Control`, `forms.read` (un contrôle tel que la page l'envoie),
`forms.json` (un contrôle pour la page), `forms.page`, `forms.run(port)`, et `forms.is_number`,
`forms.number`, `forms.text_of`. Écrit en Clair, sans partie en C.

## Dans le Studio

- `static/designer.js` et `static/designer.css` : le designer, à part de l'éditeur (`studio.js` ne fait que
  l'appeler : ouvrir un `.ui`, ▶, le bouton **+ Fenêtre**, le lien de l'application dans la console).
- Les fonctions qui écrivent le code et la page n'ont pas besoin d'un navigateur : `tests/forms/` a été écrit
  par elles.
- L'autocomplétion connaît `forms` et `Control`.

## Tests

- `tests/forms/Calcul.ui`, `.cl` et `.html`, écrits par le designer, avec trois fonctions ajoutées : le
  programme est lancé et reçoit les événements comme de la page (l'ouverture, un bouton qui multiplie deux
  nombres écrits avec une virgule, une liste, une boîte de message, un minuteur, un événement sans fonction).
- `tests/run_studio.sh` : le designer et la source du module `forms` sont servis.
- Essayé dans un navigateur de téléphone simulé : nouvelle fenêtre, contrôles posés, glissés au doigt,
  agrandis, propriétés, double toucher vers le code, ▶, l'application ouverte, le bouton qui répond, retour au
  dessin.
