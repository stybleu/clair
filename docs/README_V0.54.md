# Clair V0.54

## Une boucle ne garde plus ses textes

```text
let compte: dict[string, int] = {}
for ligne in read_lines("livre.txt") {
    for mot in split(ligne, " ") {
        if mot in compte {
            compte[mot] += 1
        } else {
            compte[mot] = 1
        }
    }
}
```

Jusqu'à la V0.53, chaque texte fabriqué (une ligne lue, les morceaux de `split`, `str(i)`,
`"a" + b`, `strip`, une tranche `s[1:4]`) et chaque objet restaient en mémoire jusqu'à la fin du
programme. Une boucle sur un gros fichier prenait de plus en plus de mémoire.

Maintenant, quand rien de ce qu'un tour de boucle fabrique ne peut servir après ce tour, sa mémoire
est rendue à la fin du tour. La boucle tourne dans la même mémoire, du premier tour au dernier.

### Comment c'est fait

Les textes et les objets sont rangés les uns après les autres dans une zone de mémoire (l'arène).
Au début de chaque tour, Clair note où en est l'arène ; à la fin du tour (et avant un `break` ou un
`continue`), il revient à ce point. Pour une boucle `while`, le point est noté avant la boucle : les
textes fabriqués par la condition sont rendus aussi.

```c
for (int64_t i = 0; i < 1000; i++) {
    ClairArenaMark clair_mark_2 = clair_rt_arena_mark();
    const char * ligne = clair_rt_string_concat("a b ", clair_rt_str_int((int64_t)(i % 3)));
    ...
    clair_rt_arena_reset(clair_mark_2);
}
```

Clair le fait seulement quand il a vérifié, dans tout le programme, que rien de ce tour ne peut
être gardé. Un texte ou un objet du tour serait gardé par :

- une variable déclarée hors de la boucle qui le reçoit (`dernier = s`), de type `string`, objet,
  `list[string]`, `dict[string, string]`, ou classe valeur qui contient un texte ;
- une liste venue d'ailleurs qui le reçoit (`tous.append(s)`, `tous[0] = s`) ;
- une valeur de dictionnaire (`d[k] = s`) ;
- un champ (`o.nom = s`) ;
- une fonction ou une méthode qui garde ce qu'elle reçoit, même de loin (Clair suit les appels).

Dans ces cas, la boucle garde sa mémoire, comme avant. Un `return` dans la boucle laisse aussi le
tour en place : le texte rendu par la fonction reste bon.

**Les dictionnaires copient maintenant leurs clés** : `compte[mot] += 1` ne garde rien du tour, et
le compte des mots d'un fichier tourne dans la même mémoire.

## Mesures

Mesurées ici, sur un ordinateur (x86-64).

| programme | V0.53 | V0.54 |
|---|---|---|
| `textes_memoire.cl` (un million de tours : textes, objets, mots comptés) : mémoire | 236 Mo | 2 Mo |
| `textes_memoire.cl` : temps | 0,73 s | 0,63 s |
| 5 millions de `"objet " + str(i)` : mémoire | 154 Mo | 12 Mo |
| compter les mots d'un million de lignes : mémoire | 215 Mo | 12 Mo |
| compter les mots d'un million de lignes : temps | 2,3 s | 1,7 s |

## Limites

- Une boucle garde tout ou rien : si un tour garde un seul texte (`dernier = s`), tous les textes
  de tous ses tours restent, comme avant.
- Les textes fabriqués hors des boucles (une fonction récursive sans boucle, par exemple) restent
  jusqu'à la fin.

## Tests

- `textes_boucles.cl` : ce qu'une boucle garde (une variable, une liste, une valeur de
  dictionnaire, un champ, une fonction qui garde) reste juste ; une boucle qui ne garde rien rend
  sa mémoire, avec les mots comptés ; une condition de `while` qui fabrique un texte, `break`,
  `continue`, et un `return` dans la boucle ; identique à Python ;
- `textes_memoire.cl` : un million de tours qui fabriquent des textes, des objets et comptent des
  mots, moins de 30 Mo (`tests/memoire_max.c`) ; identique à Python ;
- 2000 programmes tirés au hasard (textes, listes de textes, dictionnaires, objets changés ou non,
  classes valeur avec des textes, fonctions et méthodes qui gardent ou non, boucles `for`,
  `for ... in` et `while` imbriquées avec `break` et `continue`) : même sortie qu'avec la V0.53, et
  aucune erreur pour AddressSanitizer, qui interdit de relire la mémoire rendue par un tour ;
- le C des programmes dont aucune boucle ne rend de mémoire ne change que par l'arène (sa
  nouvelle fonction de retour en arrière) et la copie des clés de dictionnaire.
