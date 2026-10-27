# Clair V0.49

## Chaque erreur dit sa ligne

```text
semantic error: undefined variable 'y' (line 5)
semantic error: argument 1 of 'f' expects int, got string (line 7)
unit error: cannot add m and s (in main, line 4)
import error: module 'math' has no 'sqr' (in main() of prog.cl, line 3)
```

Avant, les erreurs de la compilation disaient seulement la fonction (`(in main)`) ou rien du tout.
Maintenant, chaque message donne la ligne du programme où est le problème. Les erreurs de
syntaxe la donnaient déjà, avec la colonne (`parse error at 2:17`).

Dans un module, le message dit aussi le fichier :

```text
semantic error: undefined variable 'z' (outil.cl, line 5)
unit error: cannot add s and m (in orbite.periode, orbite.cl, line 9)
```

### Comment c'est fait

- Chaque nœud de l'arbre du programme garde la ligne où il a été lu (`line`) et son fichier
  (`file`, vide pour le programme principal, `orbite.cl` pour un module). Le parser les donne au
  moment où il crée le nœud : la ligne du dernier mot lu.
- Pendant la vérification, Clair retient la ligne de l'expression ou de l'instruction qu'il
  regarde, et la reprend en sortant ; un message d'erreur la met à la fin, entre parenthèses.
- Les nœuds fabriqués par le compilateur lui-même (les listes en compréhension devenues des
  fonctions, par exemple) n'ont pas de ligne : leurs erreurs gardent celle du code d'où ils
  viennent.

Le C généré ne change pas : les lignes ne servent qu'aux messages.

## Limites

- Les erreurs pendant l'exécution (`list index 5 out of bounds for length 3`, division par zéro…)
  n'ont pas encore de ligne : il faudrait la passer à chaque vérification du C généré.
- Une expression écrite sur plusieurs lignes donne la ligne de son dernier morceau lu.

## Tests

- `ligne_erreur.cl` : une variable inconnue à la ligne 5, après une ligne vide ;
- `modules/erreur_module.cl` : une erreur à la ligne 5 du module `outil_casse.cl` ;
- `modules/unite_module.cl` : une erreur d'unité à la ligne 9 du module `orbite_unites.cl` ;
- `modules/nom_absent.cl` : un `from ... import` d'un nom absent, ligne 1 ;
- les messages d'unité des tests de la V0.48 donnent maintenant leur ligne ;
- 2000 programmes tirés au hasard (une erreur placée n'importe où, dans des `if`, `while` et
  `for` imbriqués, avec des lignes vides et des commentaires ; variable inconnue, mauvais type,
  mauvaise unité) : la ligne donnée est toujours la bonne ;
- le C des 149 programmes de test et de mesure est identique à la V0.48.
