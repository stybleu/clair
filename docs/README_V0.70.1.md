# Clair V0.70.1

Avec la V0.70, la fenêtre de `nbody` ramait sur le téléphone. Le journal de cette mise à jour montrait
les chiffres suivants, sur un téléphone de 8 cœurs (aarch64, Android 13) :

| | 1 cœur | 8 cœurs |
|---|---|---|
| arbre, 1 000 étoiles | 25 ms par pas | 13 ms par pas |
| arbre, 10 000 étoiles | 590 ms par pas | 294 ms par pas |

Trois causes, et trois corrections :

1. **Les 8 cœurs n'allaient que 2 fois plus vite qu'un seul.** Un téléphone mélange des cœurs rapides et
   des cœurs lents. Chaque cœur recevait une part égale du travail, si bien que tout le monde attendait le
   plus lent. Maintenant, le travail est coupé en morceaux de 64 étoiles, et chaque cœur prend le morceau
   suivant dès qu'il a fini le sien. Chaque étoile est toujours calculée seule : le résultat reste le même
   au dernier chiffre, quel que soit le nombre de cœurs.
2. **La fenêtre attendait le calcul.** Avec 4 000 étoiles, un pas prend environ 100 ms sur le téléphone,
   et l'image n'était refaite que 10 fois par seconde, même pour zoomer. Maintenant, le calcul tourne
   dans un fil à part, à son rythme. Chaque image dessine les dernières places qu'il a données. La fenêtre
   reste fluide, et « (ralenti) » s'affiche quand le calcul ne suit pas la vitesse demandée.
3. **Le dessin coûtait cher.** Termux:X11 dessine sans carte graphique. Les petites étoiles sont
   maintenant dessinées toutes ensemble, couleur par couleur, en un seul appel. Les traces sont enlevées
   au départ au-delà de 500 étoiles ; le bouton Traces les remet.

Correction en plus : quand on demandait à IAS15 d'avancer de durées très courtes, l'une après l'autre, son
pas rapetissait à chaque fois (dans la nouvelle fenêtre : 78 000 pas au lieu de 500 pour 2 ans). Un dernier
pas raccourci pour tomber pile ne change plus le pas suivant, sauf si même ce pas court était trop long.

## Ce qui ne change pas

- Les résultats des programmes : `s.run()`, `s.step()` et les tests de la V0.66 à la V0.70 donnent les
  mêmes sorties.
- Pour les tests (`CLAIR_GUI_FRAMES`), la fenêtre calcule image par image dans un seul fil, comme avant,
  pour avoir les mêmes nombres à chaque fois. `CLAIR_NBODY_THREAD=1` force le fil à part, et
  `CLAIR_NBODY_THREAD=0` l'enlève.

## Vérifié

- `tests/nbody_fil.cl` fait tourner la fenêtre avec le calcul dans un fil à part : IAS15, WHFast, et un
  amas avec l'arbre et des collisions. Le temps avance, l'énergie et la quantité de mouvement sont
  gardées, et le pas d'IAS15 reste normal.
- ThreadSanitizer ne trouve aucune course aux données entre la fenêtre, le fil de calcul et les cœurs de
  l'arbre. ASan et UBSan non plus.
- Sur 2 cœurs, sans écran, deux amas de 2 000 étoiles s'affichent à 52 images par seconde, avec 46 pas
  de calcul par seconde.

## La mesure dans le journal

La commande lance maintenant aussi `tools/bench/nbody_fenetre.cl` (sans écran) : le nombre d'images par
seconde de la fenêtre et de pas de calcul par seconde, avec 500 puis 4 000 étoiles. Les résultats vont dans
le journal, à côté de ceux de `tools/bench/nbody_vitesse.cl`.
