# Clair V0.64.1

## Clair Studio : la barre des touches suit le clavier

Sur le téléphone, quand le clavier s'ouvre, la barre des touches (`{ } ( ) [ ] : " = -> , ...`) et
l'autocomplétion restent juste au-dessus de lui.

- Chrome rapetisse la page quand le clavier s'ouvre (`interactive-widget=resizes-content`, déjà là depuis
  la V0.58). D'autres navigateurs posent le clavier par-dessus la page, et cachaient la barre et
  l'autocomplétion. Le Studio regarde maintenant ce qui se voit vraiment de la page (`visualViewport`) et
  se met à cette taille. Pendant un zoom avec deux doigts, il ne change rien.
- Clavier ouvert pour écrire du code : la console se cache pour laisser la place au code ; elle revient dès
  que le clavier se ferme.
- Clavier ouvert pour répondre à un programme (la ligne « Entrée du programme ») : c'est l'éditeur qui se
  cache, la console prend toute la place.
- La ligne où l'on écrit reste visible quand l'éditeur rapetisse (le clavier s'ouvre, l'autocomplétion ou
  une erreur apparaissent).

## Tests

Essayé dans un navigateur de téléphone simulé (Pixel 7), avec un clavier de 330 pixels, dans les deux
cas : le navigateur qui rapetisse la page, et celui qui pose le clavier par-dessus (la page décalée
comprise). Dans les deux, la barre des touches touche le haut du clavier, l'autocomplétion est juste
au-dessus, et la ligne du curseur se voit. Rien ne change dans le compilateur ni dans le C produit.
