# Clair V0.14

Clair est un langage compile. Le compilateur est ecrit en C, genere du C puis utilise Clang pour produire un executable natif.

Pipeline :

```text
source .cl -> lexer -> parser -> AST -> analyse semantique -> C genere -> Clang -> executable natif
```

## Nouveautes V0.14 : Class

Clair introduit une premiere version des classes avec le mot-cle `Class` (majuscule).

```text
Class Person {
    name: string
    age: int
}

def main() -> int {
    let p: Person = Person("Alice", 30)
    print(p.name)

    p.age = 31
    print(p.age)

    return 0
}
```

### Constructeur positionnel

Le constructeur porte le meme nom que la classe. Ses arguments suivent l'ordre des champs declares :

```text
Class Point {
    x: int
    y: int
}

let p: Point = Point(10, 20)
```

Le nombre et le type des arguments sont controles par l'analyse semantique avant la generation du C.

### References et aliasing

Les objets sont des references partagees :

```text
let a: Person = Person("Alice", 30)
let b: Person = a
b.age = 40
print(a.age) // 40
```

Les objets alloues sont suivis par le runtime Clair et liberes automatiquement a la fin du programme.

### Champs

Les champs peuvent utiliser les types deja connus de Clair, y compris les listes et d'autres classes :

```text
Class Team {
    leader: Person
    names: list[string]
}
```

Les acces chaines en lecture sont supportes :

```text
print(team.leader.name)
```

V0.14 autorise l'affectation directe d'un champ d'une variable objet :

```text
person.age = 31
person.tags = ["clair", "c"]
```

## Toujours supporte

- `int`, `float`, `bool`, `string`
- `list[int]`, `list[float]`, `list[bool]`, `list[string]`
- `def`, parametres, retours et appels de fonctions
- `let` et affectation
- `if` / `else`
- `while`
- `for` / `in` / `range()`
- `break` / `continue`
- `and` / `or` / `not`
- comparaisons numeriques
- egalite de strings par contenu
- concatenation `string + string`
- `len(string)` et `len(list[T])`
- indexation, affectation indexee et `append()` sur les listes
- controle des bornes a l'execution

## Limites volontaires V0.14

Pas encore de :

- methodes dans `Class`
- heritage
- champs prives/publics
- constructeur nomme ou valeurs par defaut
- egalite entre objets
- mutation chainee comme `team.leader.age = 40`
- appel comme `person.tags.append(...)` directement sur un champ (on peut remplacer le champ ou manipuler la liste via une variable)
- listes imbriquees comme `list[list[int]]`
- egalite entre listes

## Tests

```bash
clear
cd ~/clair
bash tests/run_tests.sh
```

La batterie couvre toutes les versions precedentes, les constructeurs, les champs, les mutations, l'aliasing, les references entre classes, plus de 100 allocations d'objets et les erreurs de type/nom de champ.
