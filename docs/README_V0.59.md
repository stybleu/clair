# Clair V0.59

## Les tests dans le programme

Un test s'écrit à côté du code qu'il vérifie, comme en Rust ou en Zig :

```clair
def carre(x: int) -> int {
    return x * x
}

def main() -> int {
    print(carre(4))
    return 0
}

test "carre de 3" {
    assert_equal(carre(3), 9)
}

test "un carre est positif" {
    assert(carre(-2) > 0, "carre(-2) devrait etre positif")
}
```

- `assert(condition)` ou `assert(condition, message)` : le test rate si la condition est fausse.
- `assert_equal(valeur, attendu)` : le test rate si les deux sont différents. Il compare deux
  entiers (`int`, `u8`... `u64`), deux nombres à virgule, deux chaînes ou deux booléens.

`clair prog.cl -o prog` ne garde que le programme : les tests n'y sont pas, ils ne coûtent rien.
`clair --test prog.cl -o tests` garde les tests et écrit un `main` qui les lance tous :

```text
running 2 tests
test carre de 3 ... ok
test un carre est positif ... ok

test result: ok. 2 passed; 0 failed; finished in 0.00s
```

Quand un test rate, on voit ce qui était vérifié, ce qui a été trouvé et la ligne :

```text
---- carre de 3 ----
Clair runtime error: assertion failed: carre(3) == 9: got 10, expected 9 (line 11)
```

- Chaque test tourne à part (dans son propre processus) : un test qui rate, qui plante (indice hors de
  la liste, division par zéro) ou qui ne finit pas (plus de 10 s, `CLAIR_TEST_TIMEOUT` pour changer)
  n'empêche pas les autres de tourner.
- Ce qu'écrit un test n'est montré que s'il rate.
- `./tests carre` ne lance que les tests dont le nom contient `carre`.
- Le code de sortie est 1 si un test a raté : utilisable dans un script.
- Les tests d'un module ne tournent pas chez ceux qui l'importent : `clair --test outil.cl` les lance.
- `assert` et `assert_equal` marchent aussi dans le programme : ils l'arrêtent avec le même message.
- Erreurs à la compilation : un test dans une fonction, deux tests du même nom, `assert(3)`,
  `assert_equal(3, "3")`, `clair --test` sur un fichier sans test.

## `clair --check` : les erreurs sans compiler

`clair prog.cl --check` lit et vérifie le programme (tests compris) puis s'arrête, sans écrire de C ni
appeler clang : quelques millisecondes. Rien n'est affiché quand tout est juste.

## Clair Studio

```text
sh ~/clair/tools/studio/studio.sh
```

- **Les erreurs pendant que tu tapes** : dès que tu t'arrêtes de taper, le fichier est enregistré et
  vérifié (`clair --check`). La ligne fautive est soulignée, et le message s'affiche au-dessus du
  clavier ; un appui dessus emmène à la ligne. Sans appuyer sur ▶.
- **L'autocomplétion** : des propositions au-dessus des touches, un appui pour en prendre une (Tab avec
  un clavier, Échap pour les fermer). Elles dépendent de l'endroit :
  - après `os.` ou `web.`, les fonctions du module, lues dans sa source, avec leur signature et leur
    explication ; après le nom d'un de tes modules (`import outil`), ses fonctions ;
  - après `x.`, les champs et les méthodes de la classe de `x` (`let p: Point`, un paramètre, `self`,
    `r` de `let r: RunResult = os.run(...)`), et `append` après une liste ;
  - après `:` ou `->`, les types, tes classes comprises ;
  - sinon, les variables visibles à cet endroit, tes fonctions et classes, les fonctions de Clair et
    les mots-clés.
  Une fonction choisie arrive avec ses parenthèses, le curseur dedans, et sa signature reste affichée
  avec l'argument en cours en gras. Rien n'est proposé dans une chaîne ou un commentaire.
- **🧪 Tests** : lance les tests du fichier (`clair --test`) et affiche ✓ ou ✗ pour chacun, avec ce
  qu'a dit chaque test raté ; un appui sur `(line 16)` emmène à la ligne. ■ les arrête.
- **▶ plus rapide** : un programme qui n'a pas changé n'est pas recompilé. Le Studio garde, à côté de
  l'exécutable, ce qui l'a produit (le compilateur et chaque `.cl` du dossier, les modules importés
  compris) : si rien n'a bougé, le programme démarre tout de suite.
- **Les gros fichiers** : seules les lignes visibles sont dessinées et chaque ligne n'est coloriée
  qu'une fois. Mesuré dans Chromium sur un ordinateur (un téléphone est plus lent) : un fichier de
  6 000 lignes s'affiche en 60 ms, et dans un fichier de 8 000 lignes chaque touche coûte au Studio
  environ 4 ms (le reste est le travail du navigateur sur la zone de texte).
- Une erreur dans un module importé (`(outil.cl, line 3)`) ouvre ce fichier à cette ligne.

## Tests

- `tests/test_blocs.cl` : sans `--test`, seul `main` tourne et les tests ne sont pas dans le C ; avec
  `--test`, dix tests : `assert` et `assert_equal` (`int`, `u8`, `u64`, `float`, `string`, `bool`, une
  classe), un message, une condition sur deux lignes, une erreur d'indice, une boucle sans fin arrêtée,
  `input()` qui ne bloque pas ; le filtre ; le test d'un module importé ignoré ; le code de sortie ;
- `clair --check` : rien sur un programme juste ; `assert` et `assert_equal` mal employés, un test dans
  une fonction, deux tests du même nom ; `--test` sur un fichier sans test ;
- `tests/run_studio.sh` : le cache (un programme inchangé n'est pas recompilé, un programme modifié
  l'est et affiche sa nouvelle sortie), la vérification sans compiler, les tests (✓, ✗ avec
  `got 4, expected 5 (line 10)`, code 1), un fichier sans test, la source des modules (pas sans jeton) ;
- le C de tous les programmes des tests et des exemples est identique à celui de la V0.58 ;
- à part, l'éditeur dans Chromium à la taille d'un téléphone : erreur soulignée sans lancer, puis
  disparue une fois corrigée ; propositions (`ca` → `carre`, `os.ru`, `r.`, `p.wa`, `self.`, un type,
  un constructeur, rien dans une chaîne) ; tests ✓ / ✗ et ligne
  touchée ; cache ; fichier de 6 000 lignes ; aucune erreur JavaScript.
