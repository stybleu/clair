# Clair V0.16

Clair V0.16 complète les classes de la V0.14 : elles ont maintenant des méthodes.

## Méthodes

Une méthode se déclare avec `def` à l'intérieur d'une `Class`, au milieu des champs ou après eux.
Le paramètre `self` (l'objet lui-même) est implicite : ne pas le déclarer.

```text
Class Person {
    name: string
    age: int

    def birthday() -> int {
        self.age = self.age + 1
        return self.age
    }

    def older_than(other: Person) -> bool {
        return self.age > other.age
    }
}

def main() -> int {
    let p: Person = Person("Alice", 30)
    let q: Person = Person("Bob", 25)
    print(p.birthday())
    print(p.older_than(q))
    return 0
}
```

Règles :

- le constructeur positionnel ne dépend que des champs, pas des méthodes ;
- une méthode peut appeler les autres méthodes de sa classe (`self.autre()`), des fonctions
  ordinaires, et recevoir des objets de n'importe quelle classe ;
- deux méthodes d'une même classe ne peuvent pas porter le même nom, et une méthode ne peut pas
  porter le nom d'un champ ;
- le nombre et le type des arguments sont contrôlés par l'analyse sémantique.

Sous le capot, `Person.birthday` devient la fonction C `clair_fn_Person__birthday(self)` : le
générateur de C n'a donc pas besoin de connaître les méthodes. Il vaut mieux ne pas nommer une
fonction utilisateur `Classe__methode`.

## Appel en instruction

Un appel de fonction ou de méthode peut maintenant être écrit seul sur sa ligne. Son résultat est
ignoré.

```text
p.birthday()
bump(counter)
team.leader.birthday()
```

## Mutation chaînée

On peut affecter un champ à travers plusieurs niveaux :

```text
team.leader.age = 40
```

## C généré plus rapide

- toutes les fonctions sont émises en `static` avec le préfixe `clair_fn_` : Clang voit tous les
  appels et peut inliner, et un nom comme `abs`, `exit` ou `printf` ne heurte plus la libc ;
- les variables ou champs nommés comme un mot-clé C (`char`, `default`, `static`…) sont renommés
  automatiquement dans le C ;
- objets et chaînes concaténées sont alloués dans une arène (simple incrément de pointeur) au lieu
  d'un `malloc` suivi d'un enregistrement par allocation. Mesure sur 3 millions d'objets :
  0,22 s et 116 Mo avant, 0,05 s et 47 Mo après.

## Limites connues

- pas d'héritage, de champs privés, de valeurs par défaut ni de constructeur nommé ;
- `team.names.append(x)` n'est pas encore supporté : le message d'erreur demande de passer par
  une variable (`let names: list[string] = team.names` puis `names.append(x)`) ;
- l'affectation indexée exige une variable : `values[i] = x` fonctionne, `team.names[0] = x` non ;
- pas d'égalité entre objets ni entre listes, pas de listes imbriquées ;
- la mémoire des objets et des chaînes n'est rendue qu'à la fin du programme ;
- `s = s + "x"` dans une boucle recopie toute la chaîne à chaque tour (coût quadratique).

## Tests

```bash
bash tests/run_tests.sh
```

Le test SIMD V4 inspecte l'assembleur NEON : il est ignoré automatiquement hors ARM64.
