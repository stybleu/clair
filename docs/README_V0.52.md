# Clair V0.52

## Les fonctions ne laissent plus de listes derrière elles

```text
def paire(a: int) -> list[int] {
    return [a, a + 1]
}

for i in range(1000000) {
    t += somme(paire(i))          // V0.51 : un million de listes gardées jusqu'à la fin
    for v in carres(8) {          // V0.52 : chacune est rendue dès qu'elle ne sert plus
        t += v
    }
}
```

Jusqu'à la V0.51, Clair rendait la mémoire d'une liste seulement quand elle était rangée dans une
variable (`let ys = echelle(xs, 2.0)`) et que la fonction qui la fabriquait la rendait par un nom
(`return ys`). Toutes les autres listes faites par une fonction restaient en mémoire jusqu'à la fin
du programme. Une boucle qui appelait `somme(paire(i))` prenait donc de plus en plus de mémoire.

### 1. Les fonctions qui fabriquent une liste neuve

Clair reconnaît maintenant toutes les fonctions dont chaque `return` donne une liste que personne
d'autre ne connaît :

```text
return [a, a + 1]                  // une liste écrite
return [x * k for x in xs]         // une liste en compréhension
return xs[1:]                      // une tranche (c'est une copie)
return [0] * n                     // une liste répétée
return paire(a + 1)                // une autre fonction de ce genre
return r                           // une variable locale faite d'une liste neuve (comme avant)
```

`split()`, `read_lines()` et `load_csv()` fabriquent aussi des listes ou des matrices neuves.

### 2. Les listes utilisées tout de suite

Une liste neuve qui est seulement lue, là où elle est faite, est rendue dès que l'instruction est
finie :

| où | exemple |
|---|---|
| donnée à une fonction qui ne la garde pas | `somme(paire(i))`, `somme(echelle(echelle(xs, 2.0), 0.5))` |
| lue à un indice | `paire(i)[1]`, `split(ligne, ",")[0]` |
| mesurée | `len(carres(n))`, `rows(lire(f))` |
| cherchée | `if x in voisins(k) { ... }` |
| coupée en tranche | `len(carres(9)[2:])` |
| parcourue | `for v in paire(i) { ... }` |
| jetée | `paire(3)` seule sur sa ligne |

- La liste est rangée dans une variable du C à l'endroit même où elle est faite : les appels gardent
  leur ordre, rien ne change dans ce que fait le programme.
- Dans un `if`, elle est rendue juste après le test, avant que le bloc tourne.
- Dans un `for v in ...`, elle est rendue après la boucle, ou par un `return` à l'intérieur ; `break`
  et `continue` la laissent à la boucle.
- Les fonctions récursives en profitent : `somme_rec(xs[1:])` ne garde plus toutes les tranches.

```c
ClairList clair_tmp_0 = NULL;
t += clair_fn_somme((clair_tmp_0 = clair_fn_paire(i)));
if (clair_tmp_0) clair_rt_list_free(clair_tmp_0);
```

### Ce qui reste gardé, comme avant

- Une liste qui peut servir après : un deuxième nom (`let b = garde(a)`), rangée dans un objet
  (`Sac(xs, 3)`), rendue par la fonction qui la reçoit (`def garde(xs) -> list[int] { return xs }`).
  Clair suit les appels de fonction en fonction pour le savoir.
- Une liste fabriquée dans la condition d'un `while` ou dans les bornes d'un `for i in range(...)`.
- **Les textes** (`"objet " + str(i)`, les morceaux de `split`) : ils restent en mémoire jusqu'à la
  fin, comme avant. Ce sera une prochaine étape.

## Mesures

Mesurées ici, sur un ordinateur (x86-64).

| programme | V0.51 | V0.52 |
|---|---|---|
| `fonctions_memoire.cl` (1 million de tours, 6 listes par tour) : mémoire | 553 Mo | 2 Mo |
| `fonctions_memoire.cl` : temps | 0,72 s | 0,21 s |
| `somme(echelle(xs, 0.5))`, un million de fois sur 50 nombres : mémoire | 558 Mo | 12 Mo |

Rendre la mémoire tout de suite va aussi plus vite : la même mémoire resservit, déjà dans le cache du
processeur.

## Tests

- `fonctions_listes.cl` : listes neuves en argument, à un indice, dans `len`, `in`, un `if`, un
  `for ... in` avec un `return` dedans, une fonction récursive sur des tranches, une fonction qui
  change la liste reçue ; listes gardées par un deuxième nom, un champ d'objet, une valeur rendue ;
  `split` ; identique à Python ;
- `fonctions_memoire.cl` : 6 millions de listes faites par des fonctions dans une boucle, moins de
  30 Mo (`tests/memoire_max.c`) ; identique à Python ;
- 1500 programmes tirés au hasard (fonctions qui fabriquent, reçoivent, changent, gardent ou rendent
  des listes ; listes neuves dans les arguments, les indices, `len`, `in`, les conditions, les
  `for ... in` avec `break`, `continue` et `return`, les `while`) : même sortie qu'avec la V0.51, et
  aucune erreur pour AddressSanitizer et UndefinedBehaviorSanitizer ;
- le C de 130 programmes de test et de mesure sur 150 est identique à la V0.51 ; les 20 autres
  rendent leurs listes plus tôt (passés aussi à AddressSanitizer).
