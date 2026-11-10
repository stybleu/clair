# Clair V0.73

Clair devine les types, comme Python : on n'écrit plus le type d'une variable quand sa valeur le
dit, ni le type rendu par une fonction quand ses `return` le disent.

## `let x = 5`

```
let x = 5                      // int
let f = 2.5                    // float
let nom = "Léa"                // string
let xs = [1.0, 2.0, 4.5]       // list[float] ; [1, 2.5] aussi
let d = {"a": 1, "b": 2}       // dict[string, int]
let p = Point(3.0, 4.0)        // Point
let mots = "un deux".split()   // list[string]
let n = len(mots)              // int
```

- Le type est celui de la valeur, une fois pour toutes : `x` reste un `int`, comme avant avec
  `let x: int = 5`. Clair compile donc exactement le même code machine.
- Une liste ou un dictionnaire vide n'a rien à regarder : `let xs = []` est refusé, avec la forme à
  écrire, `let xs: list[int] = []`.
- `let x: int = 5` s'écrit toujours : le type écrit sert quand on veut un autre type que celui de la
  valeur (`let petit: u8 = 5`, `let y: float = 1`).
- `let` reste nécessaire pour créer une variable. Avec lui, une faute de frappe (`totl = 5`) est
  refusée au lieu de créer en silence une deuxième variable.

## Le type rendu par une fonction

Une fonction sans `-> type` rend le type de ses `return` :

```
def double(x: int) {           // rend un int
    return x * 2
}

def fact(n: int) {             // rend un int, même en s'appelant elle-même
    if n < 2 {
        return 1
    }
    return n * fact(n - 1)
}

def melange(x: int) {          // return 1 et return 2.5 : rend un float
    if x > 0 {
        return 1
    }
    return 2.5
}

def rien() {                   // aucun return avec une valeur : ne rend rien
    print("rien")
}
```

- Ça marche aussi pour les méthodes (`def norme2 { return self.x * self.x + self.y * self.y }`), pour
  une fonction qui en appelle une autre sans type, et pour deux fonctions qui s'appellent l'une
  l'autre (`pair` et `impair`) dès que l'une a un `return` qui ne dépend pas de l'autre.
- `return 1` et `return 2.5` dans la même fonction : elle rend un `float`. `return 1` et
  `return "a"` : refusé, comme avant avec un type écrit.
- Une fonction qui ne rend que `[]` doit écrire son type : `def vide() -> list[int]`.
- Deux fonctions qui ne rendent que l'une l'autre n'ont rien à deviner : Clair demande d'écrire le
  type.
- Une fonction qui ne rend rien ne sert pas de valeur : `let y = rien()` est refusé.
- Les paramètres gardent leur type (`def aire(r: float)`). C'est lui qui donne à Clang de quoi faire du
  code machine rapide.

## Comment c'est fait

- `src/parser.c` : `let x = valeur` sans `: type` ; une fonction sans `-> type` est marquée `@infer`.
- `src/semantic.c` :
  - `let` sans type prend le type de sa valeur ;
  - `guess_return_types` lit chaque fonction `@infer` en silence, seulement pour voir ce que donnent
    ses `return` ;
  - une fonction qui en appelle une autre pas encore devinée attend le tour suivant ;
  - une fonction récursive prend, pour ses propres appels, ce que donnent ses autres `return`,
    jusqu'à ce que la réponse ne change plus ;
  - ensuite, la vérification habituelle relit tout avec les types trouvés.
- Le C écrit est le même qu'avec les types écrits à la main.

## Tests

- `types_devines.cl` :
  - `let` pour un int, un float, un texte, une liste, un dictionnaire, un objet, un booléen ;
  - le type rendu de fonctions simples, récursives (`fact`, `fib`), mutuelles (`pair` et
    `impair`), de méthodes, d'int et de float mélangés, et sans valeur ;
  - les signatures C sont vérifiées.
- Les refus : `let xs = []`, une fonction qui ne rend que `[]`, deux fonctions qui ne rendent que
  l'une l'autre.
- Le test de la V0.72 qui refusait `return 3` dans une fonction sans type disparaît : elle rend
  maintenant un `int`.
