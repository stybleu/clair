# Clair V0.42

## `bigint` et `bigfloat` : la mémoire est rendue partout

```text
def suite(n: int) -> bigint {
    let x: bigint = bigint("1")
    let m: bigint = bigint("1000000000000000000000000000000000000007")
    for i in range(n) {
        x = (x * bigint("3") + bigint("3")) % m
    }
    return x
}
```

En V0.40, les nombres créés dans une fonction (pas `main`) n'étaient jamais rendus avant la
fin du programme, et un seul champ `bigint` dans une classe coupait la récupération pour tout
le programme. Cette fonction, appelée avec `n = 3 000 000`, demandait plus d'un gigaoctet. En
V0.42, elle reste sous 2 Mo, comme dans `main`.

### Comment ça marche

- **Chaque appel de fonction a sa zone** : les nombres créés pendant l'appel. Chaque nombre
  porte un numéro croissant, et l'appel note en entrant le dernier numéro distribué.
- **À la fin d'un bloc** (tour de boucle, `if`, avant `break` et `continue`), Clair garde les
  nombres de la zone que montrent les variables de l'appel et rend les autres. Il ne regarde
  jamais les nombres plus anciens : dans `a * b + f(c)`, le résultat de `a * b` attend la fin
  de `f` sans risque.
- **En sortie d'appel**, seule la valeur rendue par `return` est gardée ; tout le reste de la
  zone est rendu. Une récursion (`fib(n - 1) + fib(n - 2)`) ne garde donc que ce qui sert.
- **Un nombre rangé dans un champ** (`self.solde = ...`, `Compte(..., bigint("100"))`) est
  épinglé : il reste jusqu'à la fin du programme, car un objet peut être partagé. Remplacer
  souvent un champ `bigint` dans une boucle garde donc les anciennes valeurs ; mieux vaut
  calculer dans une variable et ranger le résultat à la fin.
- Une fonction qui n'utilise pas de grands nombres n'a rien de tout cela : ses boucles
  restent aussi rapides qu'avant.

C'est aussi un peu plus rapide qu'en V0.40 (un seul bloc mémoire par nombre, plus de recherche
dans une liste) : sur la machine de test, une boucle de 300 000 tours passe de 0,086 s à 0,069 s, celle dans une
fonction de 0,164 s à 0,119 s.

## `clair` marche depuis n'importe quel dossier

En V0.40, un programme avec des `bigint` ne se compilait que lancé depuis `~/clair` (Clair
cherchait `src/clair_bigint.c` à côté). Le code des grands nombres est maintenant rangé dans
le compilateur et recopié dans le C du programme : `../build/clair prog.cl` marche partout, et
le C généré se compile seul.

## `let x = x + 1` dans un bloc

```text
let x: int = 5
if x > 0 {
    let x: int = x + 1      // 6 : lit le x du dessus
}
```

`let x = x` lisait déjà le `x` du dessus, mais `let x = x + 1` ou `let x = f(x)` lisaient en
C le nouveau `x`, pas encore rempli : un nombre au hasard. C'est corrigé pour tous les types.

## Reste à faire (note de transition de la V0.40)

- listes et matrices de `bigint`, conversions avec `int` et `float` ;
- un champ `bigint` souvent remplacé garde ses anciennes valeurs (voir plus haut) ;
- mesurer sur le téléphone les seuils Karatsuba et Toom-Cook 3.

## Tests

- `grands_nombres_memoire.cl` : récursion (`fib`, factorielle), boucles avec `break` et
  `continue`, `return` au milieu d'une boucle, variables masquées, alias, champs et méthodes,
  `bigfloat` ; sortie identique à Python ;
- `grands_nombres_longue_boucle.cl` : 3 millions de calculs dans une fonction et dans une
  méthode, sa mémoire maximale est mesurée et doit rester sous 100 Mo (la V0.40 dépasse 1 Go) ;
- `masquage_init.cl` ; `big_numbers_memory_v040.cl` est maintenant lancé par la suite ;
- les tests de la V0.40 compilés depuis le dossier `tests/` ;
- 400 programmes tirés au hasard (fonctions récursives, boucles, `break`, `continue`, `return`
  au milieu, masquage, alias, méthodes qui modifient un champ, appels dans des expressions) :
  sortie identique à Python ; la moitié passée sans erreur d'AddressSanitizer,
  d'UndefinedBehaviorSanitizer ni de fuite de mémoire ;
- 3000 opérations `+ - * / %` sur des `bigint` jusqu'à 200 bits : identiques à Python.
