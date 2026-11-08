# Clair V0.61

## Le module `plot` : des graphiques, comme matplotlib

```clair
import plot
import math

def main() -> int {
    let xs: list[float] = []
    let ys: list[float] = []
    for i in range(0, 201) {
        let x: float = float(i) / 20.0
        xs.append(x)
        ys.append(math.sin(x))
    }
    plot.title("sin(x)")
    plot.line(xs, ys)
    plot.name("sin")
    plot.show()
    return 0
}
```

- **Dans Clair Studio**, `plot.show()` affiche la figure dans la console, à sa place entre les lignes
  que le programme écrit. Un appui l'ouvre en grand (− et + pour zoomer, « Garder » l'enregistre dans le
  dossier des programmes).
- **Ailleurs**, `plot.show()` écrit `plot_1.svg`, `plot_2.svg`... à côté du programme (et le dit sur la
  sortie d'erreur) : un fichier SVG s'ouvre dans n'importe quel navigateur.

Les figures sont en SVG : des traits et du texte, nets à n'importe quel zoom, sans bibliothèque à
installer. Leurs couleurs suivent le mode clair ou sombre du téléphone.

### Ce que l'on peut tracer

| Fonction | Ce qu'elle trace |
|---|---|
| `plot.line(xs, ys)` | une courbe par les points `(xs[i], ys[i])` (un `nan` la coupe) |
| `plot.points(xs, ys)` | un point à chaque `(xs[i], ys[i])` |
| `plot.polygon(xs, ys)` | une forme fermée |
| `plot.bars(noms, valeurs)` | une barre par valeur, son nom dessous |
| `plot.histogram(valeurs, n)` | combien de valeurs tombent dans chacune des `n` classes |
| `plot.segment(x1, y1, x2, y2)` | un trait |
| `plot.arrow(x, y, dx, dy)` | une flèche |
| `plot.circle(x, y, rayon)` | un cercle |
| `plot.text(x, y, "texte")` | un texte centré sur `(x, y)` |
| `plot.heatmap(valeurs, nx, ny)` | un champ sur une grille, en couleurs, avec son échelle |
| `plot.contour(valeurs, nx, ny, n)` | les lignes de niveau du champ (isobares...), environ `n` |
| `plot.arrows(us, vs, nx, ny)` | une flèche `(us, vs)` par point de la grille (le vent...) |

Un champ sur une grille de `nx` colonnes et `ny` lignes est une `list[float]` : la valeur de la colonne
`i` et de la ligne `j` est `valeurs[j * nx + i]`, la ligne 0 en bas. `plot.extent(x0, x1, y0, y1)` dit où
la grille se trouve (sinon de 0 à `nx` et de 0 à `ny`).

### Régler la figure

- `plot.title`, `plot.xlabel`, `plot.ylabel` : le titre et les noms des axes ;
- `plot.xrange(a, b)`, `plot.yrange(a, b)` : ce que montrent les axes (sinon toutes les données) ;
- `plot.equal()` : la même échelle sur les deux axes (un cercle reste rond) ;
- `plot.hide_axes()` : ni axes ni grille, pour un dessin ;
- `plot.size(largeur, hauteur)` : en pixels (640 × 400 au départ) ;
- `plot.colormap("viridis")`, `"chaleur"` ou `"bleu-rouge"` : les couleurs de `plot.heatmap`.

Juste après une courbe, des points, des barres ou une forme :

- `plot.name("sin")` : son nom dans la légende (placée dans le coin qui cache le moins de points) ;
- `plot.color("rouge")` : `rouge`, `bleu`, `vert`, `orange`, `violet`, `jaune`, `noir`, `gris`, `rose`,
  `cyan`, `marron`... ou `"#1d4ed8"` ;
- `plot.fill("jaune")` : l'intérieur d'un polygone ou d'un cercle ;
- `plot.width(3.0)` : l'épaisseur du trait.

Sans `plot.color`, les courbes prennent des couleurs différentes l'une après l'autre.

### Montrer, garder, animer

- `plot.show()` : montre la figure, puis en commence une nouvelle ;
- `plot.save("courbe.svg")` : écrit la figure dans ce fichier (et la garde) ;
- `plot.frame()` puis `plot.animate(images_par_seconde)` : une animation. Après chaque image dessinée,
  `plot.frame()` la garde et efface les données (pas le titre, les axes, la taille) ;
  `plot.animate(12.0)` montre le tout. Dans Clair Studio, lecture, pause et un curseur pour aller à une
  image ; ailleurs, `animation_1.svg` tourne tout seul dans le navigateur ;
- `plot.image("photo.png")` : montre une image du dossier dans la console de Clair Studio.

### Exemples

`exemples/plot/` : `courbes.cl` (deux courbes et une légende), `pentagramme.cl`, `histogramme.cl`
(5000 tirages et des barres), `carte.cl` (une dépression : la pression en couleurs, les isobares et le
vent qui tourne autour), `animation.cl` (une onde qui avance).

### Les erreurs

Deux listes de longueurs différentes, une grille qui n'a pas `nx × ny` valeurs, une couleur inconnue :
le programme s'arrête avec un message qui dit ce qui ne va pas
(`plot.line(): the two lists have 2 and 1 values, they need as many`).

### Comment c'est fait

`src/clair_plot.cl` (le module, en Clair) appelle `src/clair_plot_rt.c` (en C, copié dans les
programmes qui importent `plot`) : la figure en cours y est gardée, les graduations sont choisies (1, 2,
5 × 10ⁿ), la carte en couleurs devient une image PNG écrite dans le SVG, les isolignes sont calculées par
« marching squares », et l'animation est un seul SVG dont les images se suivent (SMIL). Dans Clair
Studio, la variable `CLAIR_PLOT_DIR` dit où écrire les figures, et la ligne `ESC[clair:plot NOM]` que le
programme écrit est remplacée par la figure ; le Studio retire du SVG tout ce qui pourrait s'exécuter
avant de l'afficher.

## Tests

- `tests/plot_module.cl` : une courbe, des points, une légende et un titre à échapper, comparés au
  fichier attendu `tests/plot_module.expected.svg` ; un dessin sans axes (polygone rempli, cercle, trait,
  flèche, texte) ; `plot.save` ; barres ; histogramme ; carte en couleurs (une vraie image PNG), isolignes
  et flèches ; une animation de 5 images ; les lignes pour Clair Studio et les fichiers dans
  `CLAIR_PLOT_DIR` ; trois erreurs (`tests/plot_erreur_*.cl`) ;
- `tests/run_studio.sh` : un programme qui affiche une figure et une image : la sortie, la figure, l'image ;
  refusés : un nom de figure inventé, un fichier qui n'est pas une image, sans jeton ;
- à part : chaque SVG lu par un analyseur XML, l'image PNG décodée (CRC, taille, couleurs), le programme
  de test sous AddressSanitizer ; l'éditeur dans Chromium : trois figures dans la console entre les
  lignes écrites, l'animation mise en pause puis à l'image 6, la figure en grand puis gardée dans le
  dossier, l'autocomplétion de `plot.` ; aucune erreur JavaScript.
