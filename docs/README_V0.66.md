# Clair V0.66

Deux nouvelles bibliothèques intégrées à Clair, comme `plot` :

- `gui` : une fenêtre où l'on dessine en temps réel, comme pygame ;
- `nbody` : des corps attirés par la gravité de Newton (planètes, étoiles, galaxies), qu'on voit bouger
  en temps réel dans une fenêtre de `gui`.

## Ce qu'il faut dans Termux (une seule fois)

```sh
pkg install x11-repo
pkg install sdl3 termux-x11-nightly
```

Il faut aussi l'appli Android **Termux:X11** : c'est elle qui montre la fenêtre. On la prend sur
https://github.com/termux/termux-x11/releases (`termux-x11-universal-debug.apk`, dans `nightly`). Dans ses
préférences, mettre *Touchscreen input mode* sur **Direct touch** : un doigt touche alors là où il est
posé, comme une souris.

Ensuite, rien d'autre à faire. Quand un programme ouvre une fenêtre, Clair démarre le serveur X de
Termux:X11 s'il ne tourne pas, et met l'appli devant. Pour revenir à Clair Studio, on revient au
navigateur. Un programme qui importe `gui` ou `nbody` sans ouvrir de fenêtre n'a besoin de rien : SDL3
n'est chargé qu'à l'ouverture de la première fenêtre.

## `gui` : une fenêtre comme pygame

```
import gui

def main() -> int {
    let w: Window = gui.open("Balle", 0, 0)           // 0, 0 : tout l'écran
    let x: float = 100.0
    while w.running() {
        x += 200.0 * w.dt()                          // 200 pixels par seconde, quelle que soit la vitesse
        w.clear(gui.color("noir"))
        w.circle(x, 300.0, 30.0, gui.color("jaune"))
        w.text("Été : " + str(int(w.fps())) + " images/s", 10.0, 10.0, 24.0, gui.color("blanc"))
        if w.button("Quitter", 10.0, 50.0, 160.0, 60.0) {
            w.close()
        }
        w.present()                                  // montre l'image, et attend pour faire 60 images par seconde
    }
    return 0
}
```

- **Dessiner** : `clear`, `rect` (plein) et `frame` (le bord), `line`, `lines` (une ligne qui passe par des
  points : une courbe, une trace), `point`, `circle` (plein) et `ring` (le bord), `triangle`, `polygon`
  (plein) et `outline`, `text` (avec les accents) et `text_width`.
- **Les couleurs** : `gui.color("rouge")` (en français ou en anglais : noir, blanc, rouge, vert, bleu, jaune,
  orange, violet, rose, marron, gris, cyan, magenta, sombre, clair, ciel), `gui.rgb(r, g, b)`,
  `gui.rgba(r, g, b, a)`, `gui.hex("#ffcc00")`.
- **Le doigt ou la souris** : `mouse_x`, `mouse_y`, `mouse_down(1)` (tenu), `mouse_pressed(1)` (appuyé à cette
  image), `clicked()` (lâché à cette image), `wheel()`, et `button(texte, x, y, l, h)`, qui dessine un bouton et
  dit s'il vient d'être touché.
- **Le clavier** : `key_down("gauche")` (tenue), `key_pressed("espace")` (appuyée à cette image),
  `key_released(...)` ; les noms : gauche, droite, haut, bas, espace, entree, echap, ou ceux de SDL ("a", "f1").
- **Tout ce qui s'est passé** : `event_count()` et `event(i)` (`kind` : quit, key_down, key_up, text,
  mouse_down, mouse_up, mouse_move, wheel, resize).
- **Le temps** : `dt()` (la durée de la dernière image), `time()`, `fps()`, `set_fps(n)`.
- **Les images** : `gui.image("photo.png")` (PNG ou BMP), puis `draw`, `draw_scaled`, `draw_rotated`.
- **Le reste** : `width`, `height`, `title`, `screenshot("copie.png")`, `pixel(x, y)`, `close()` (une autre
  fenêtre peut ensuite s'ouvrir), `gui.available()` (SDL3 est-il là ?).

## `nbody` : la gravité de Newton entre N corps

```
import nbody

def main() -> int {
    let s: Simulation = nbody.simulation(nbody.G())   // en unités SI ; nbody.simulation(1.0) : G = 1
    let ua: float = 1.495978707e11
    let soleil: int = s.add("Soleil", 1.989e30, 0.0, 0.0, 0.0, 0.0)
    s.color(soleil, "jaune")
    let terre: int = s.orbit("Terre", 5.97e24, soleil, ua, 0.0)
    s.orbit("Lune", 7.35e22, terre, 3.844e8, 0.0)
    s.orbit("Mars", 6.42e23, soleil, 1.524 * ua, 120.0)
    s.center()
    s.time_unit = "jours"
    s.time_scale = 86400.0
    s.length_unit = "UA"
    s.length_scale = ua
    s.show()                                          // la simulation en temps réel, jusqu'à « Quitter »
    return 0
}
```

- **Les corps** : `s.add(nom, masse, x, y, vx, vy)` dans le plan, `s.add3(...)` dans l'espace,
  `s.orbit(nom, masse, autour, distance, angle)` (une orbite ronde autour d'un autre corps), `s.radius(i, r)`,
  `s.color(i, "bleu")`, `s.find("Terre")`. Ils sont dans les listes `s.x`, `s.y`, `s.z`, `s.vx`, `s.vy`, `s.vz`,
  `s.m`, `s.r`, `s.name` : on peut les lire, les changer, les donner à `plot`.
- **Faire avancer le temps** : `s.step()` (un pas), `s.run(durée)`. Le pas est `s.dt` ; s'il vaut 0, il est
  choisi seul (un cinq-centième du temps que met la paire de corps la plus serrée à tomber l'une sur l'autre).
- **La méthode** : le saute-mouton (leapfrog), qui garde l'énergie sur des millions de pas ; `s.order = 4` :
  la méthode de Yoshida, d'ordre 4, bien plus précise pour le même pas. `s.soft` : une longueur
  d'adoucissement, pour que deux corps très proches ne s'envoient pas à l'infini (utile pour une galaxie).
- **Les collisions** : avec `s.merge = true`, deux corps qui se touchent (leurs rayons) n'en font plus qu'un, qui
  garde la masse, la quantité de mouvement et le volume. L'autre reste dans les listes avec une masse de 0 et
  un rayon de -1 : les numéros des corps ne changent pas. `s.count()` : les corps qui restent.
- **Pour vérifier** : `s.energy()` (`kinetic` + `potential`), `s.momentum(axe)`, `s.angular_momentum()`,
  `s.center_of_mass(axe)`, `s.center()` (le centre de masse à 0, immobile), `s.distance(i, j)`, `s.velocity(i)`.
- **Un corps sans masse** (`masse` 0) est attiré sans attirer : une poussière, une sonde.

### La fenêtre de `s.show()`

Elle prend tout l'écran (ou la fenêtre de `gui` déjà ouverte) et montre les corps qui bougent, avec leur
trace (`s.trail` : sa longueur en points, 400 au départ ; 0 : pas de trace).

- **Les boutons** : Pause / Lecture, `-` et `+` (zoom), `<<` et `>>` (deux fois moins ou plus vite), Centrer
  (tout dans la fenêtre, autour du centre de masse), Traces, Quitter. Sur un téléphone tenu debout, ils sont
  sur deux lignes.
- **Avec le doigt** : glisser déplace la vue ; toucher un corps le suit (la vue reste sur lui), le toucher
  encore le lâche. La molette zoome là où est la souris.
- **Au clavier** : espace (pause), `+` et `-`, flèches gauche et droite (vitesse), `c` (centrer), `t` (traces),
  échap ou `q` (quitter).
- **En haut** : le temps de la simulation, ce que vaut une seconde (`s.speed` ; 0 au départ : une orbite rapide
  en 4 secondes ; « ralenti » quand le téléphone ne suit pas), le nombre de corps et l'écart d'énergie depuis le
  départ (il doit rester tout petit : sinon, le pas est trop grand). En bas, une échelle des longueurs.

Les forces sont calculées en C, chaque paire une seule fois : quelques centaines de corps restent fluides.

## Clair Studio

L'autocomplétion connaît `gui` et `nbody` (fonctions, classes, champs, méthodes). ▶ sur un programme qui
ouvre une fenêtre la montre dans Termux:X11 ; ce que le programme écrit reste dans la console.

## Dans le compilateur

- `src/clair_gui.cl` et `src/clair_gui_rt.c`, `src/clair_nbody.cl` et `src/clair_nbody_rt.c`, intégrés comme
  `plot` (fonctions `__gui_...` et `__nbody_...`, que seuls ces fichiers peuvent appeler).
- SDL3 n'est pas lié au programme : il est chargé par `dlopen` à l'ouverture de la fenêtre (le programme est
  lié avec `-ldl`). Les quelques structures de SDL3 employées sont déclarées dans `clair_gui_rt.c` ;
  `tests/gui_abi.c` les compare aux en-têtes de SDL3 quand ils sont là.
- Les lettres : une police de 8 × 8 points du domaine public (Daniel Hepper, d'après les polices VGA d'IBM),
  avec les accents du français.
- Une méthode qui ne se sert pas de `self` ne donne plus d'avertissement de Clang.

## Tests

- `tests/nbody_module.cl` (sans fenêtre) : une orbite ronde revient après un tour, avec l'énergie, le moment
  cinétique et la quantité de mouvement gardés ; l'ordre 4 plus précis que le saute-mouton ; un corps sans
  masse attiré ; le « huit » à trois corps de Chenciner et Montgomery revient après une période ; une
  collision garde la masse, la quantité de mouvement et le volume ; le pas choisi seul.
- `tests/gui_module.cl`, sans écran (le pilote `offscreen` de SDL3), avec des touchers et des touches simulés :
  les formes, les couleurs des pixels, les lettres accentuées, un bouton, la touche espace, une copie BMP et
  PNG relue comme image, droite et tournée, une deuxième fenêtre. `tests/nbody_fenetre.cl` : la fenêtre de
  `s.show()`, Pause, Lecture, Quitter. Sans SDL3, ces deux tests sont sautés, et `tests/gui_sans_sdl.cl`
  vérifie que `gui.open` dit quoi installer.
- Le C des programmes qui n'importent ni `gui` ni `nbody` ne change pas (156 programmes comparés à la V0.65.3).
- AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur (fenêtre, collisions, déplacement, zoom).
