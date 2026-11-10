# Clair V0.72

Cette version complète le langage en console, avant l'application autonome : on peut écrire le
squelette d'un programme sans que Clair refuse de le compiler, et les textes se travaillent comme en
Python.

## Le squelette d'un programme se compile

Comme en Python, on peut poser une fonction ou une classe vide et la remplir plus tard.

```
Class Moteur {
    pass
}

def plus_tard() {
    pass
}

def calcul(x: float) -> float {
    pass
}

def main {
}
```

- **`pass`** ne fait rien : il garde la place d'un corps encore vide, dans une fonction, une classe, une
  boucle ou un `if`. Ce n'est pas un mot réservé : une variable peut toujours s'appeler `pass`.
- **Sans paramètres, les parenthèses sont facultatives** : `def main { }`, `def bouge { }` dans une classe.
- **`-> type` est facultatif** : une fonction sans type ne rend rien, comme une fonction Python sans
  `return`. On l'appelle comme une instruction (`plus_tard()`) ; s'en servir comme valeur
  (`let x: int = plus_tard()`) est refusé avec un message clair.
- **`return` tout seul** sort d'une fonction sans type. Dans une fonction qui a un type, il faut une
  valeur (`return 0`).
- **`def main { }`** équivaut à `def main() -> int { ... return 0 }` : le programme rend 0 au système, et
  `return` tout seul dans `main` rend 0 aussi.
- **Une fonction typée qui arrive à sa fin sans `return`** rend 0, 0.0, false, "" ou une liste ou un
  dictionnaire vide, comme Python rend `None`, avec un avertissement :
  `warning: calcul() can reach its end without return: it then gives back 0.0 (line 24)`. Avant, elle
  rendait n'importe quoi. Clair voit qu'une fonction finit toujours par `return` quand c'est vrai sur tous
  les chemins : `if` / `else` qui rendent tous les deux, ou `while true` sans `break`. Pour une fonction
  qui rend un objet (une classe), Clair demande d'écrire le `return`.
- **`else if`**, l'`elif` de Python : `if x > 0 { } else if x < 0 { } else { }`.
- Les paramètres qu'une fonction n'utilise pas encore ne donnent plus d'avertissement de Clang.

## Les textes comptés en caractères

Un texte de Clair est en UTF-8 : « é » y prend 2 octets, « € » 3. Jusqu'ici `len("été")` donnait 5
(des octets) et `"été"[0:1]` coupait le « é » en deux. Maintenant tout compte en caractères, comme
Python :

```
let s: string = "Été à Paris"
print(len(s))          // 11
print(s[0])            // É
print(s[-5:])          // Paris
for c in "añb" {       // a, ñ, b
    print(c)
}
```

- `len(s)`, `s[i]` (nouveau, `i` négatif compte depuis la fin), `s[a:b:c]`, `for c in s` (nouveau) et les
  positions rendues par `find` comptent en caractères.
- `s[i]` hors du texte arrête le programme : `string index out of range (line 5)`.
- Un texte ne change pas sur place (`s[0] = "x"` est refusé), comme en Python : on en fait un nouveau.
- `"ab" * 3` donne `ababab`.
- Pour parcourir un long texte, `for c in s` est bien plus rapide qu'une boucle sur `s[i]` : chaque
  `s[i]` doit compter les caractères depuis le début.

## Les méthodes des textes

Les méthodes de `str` de Python, avec les mêmes résultats :

| Méthode | Ce qu'elle rend |
| --- | --- |
| `lower()`, `upper()`, `swapcase()` | minuscules, majuscules, inversées (`"Straße".upper()` donne `STRASSE`) |
| `capitalize()`, `title()` | première lettre du texte, ou de chaque mot, en majuscule |
| `strip()`, `lstrip()`, `rstrip()` | sans les espaces autour ; `strip("-")` sans les `-` |
| `split()`, `split(",")`, `split(",", 1)` | les morceaux (`list[string]`) ; `rsplit` part de la fin |
| `splitlines()` | les lignes (`\n`, `\r\n`...) ; `splitlines(true)` les garde |
| `", ".join(xs)` | les textes de `xs` mis bout à bout |
| `replace(a, b)`, `replace(a, b, 2)` | `a` remplacé par `b` (partout, ou les 2 premiers) |
| `find(x)`, `rfind(x)` | la position de `x` (en caractères), ou -1 |
| `index(x)`, `rindex(x)` | pareil, mais arrête le programme si `x` n'y est pas |
| `count(x)` | combien de fois `x` y est |
| `startswith(x)`, `endswith(x)` | commence, finit par `x` |
| `removeprefix(x)`, `removesuffix(x)` | sans `x` au début, à la fin |
| `ljust(n)`, `rjust(n)`, `center(n, "*")` | complété jusqu'à `n` caractères |
| `zfill(n)` | complété par des 0 à gauche (`"-7".zfill(4)` donne `-007`) |
| `isalpha()`, `isdigit()`, `isdecimal()`, `isnumeric()`, `isalnum()` | que des lettres, des chiffres... |
| `isspace()`, `isupper()`, `islower()`, `istitle()`, `isascii()` | que des espaces, des majuscules... |

`find`, `rfind`, `index`, `rindex`, `count`, `startswith` et `endswith` acceptent aussi un début et une
fin, comme Python : `s.find("a", 2)`, `s.count("a", 0, -1)`.

Les majuscules, les minuscules et les catégories de lettres viennent des tables d'Unicode de Python
lui-même (`tools/clair_text_tables.py`) : le grec (avec le sigma final, `"ΟΔΟΣ".lower()` donne `οδος`),
le cyrillique, les ligatures (`"ﬁ".upper()` donne `FI`), les chiffres arabes, les chiffres romains...
Le script vérifie ses tables sur chacun des 1 114 112 caractères d'Unicode avant de les écrire, et le
test `textes_python.cl` compare 739 résultats à ceux de Python.

Les anciennes fonctions `split(s)` et `strip(s)` restent là, inchangées.

## Comment c'est fait

- `src/parser.c` : `pass`, les parenthèses et `-> type` facultatifs, `return` seul, `else if`.
- `src/semantic.c` : les fonctions sans type (`void`), la valeur rendue au bout d'une fonction typée
  sans `return`. `s.lower()` devient l'appel `@tx_lower(s)` ; `s[i]`, `len(s)` et `s * n` deviennent
  `@tx_at`, `@tx_len` et `@tx_repeat`.
- `src/text_methods.h` : la liste des méthodes, leurs arguments et leurs valeurs par défaut.
- `src/clair_text_rt.c` : les fonctions C (`clair_tx_lower`...), recopiées dans le programme seulement
  s'il s'en sert ; les tables d'Unicode seulement s'il change la casse ou demande `isalpha`... Un
  caractère seul (`s[i]`, `for c in s`) est gardé une fois pour toutes : parcourir un texte ne prend pas
  de mémoire.

## Tests

`tests/run_tests.sh` vérifie en plus :

- `squelettes.cl` : `pass` partout, `def main { }`, une méthode `def ajoute { }`, `return` seul,
  `else if`, une variable appelée `pass`, et les avertissements des fonctions typées sans `return` ;
- `textes_python.cl` : 739 résultats identiques à Python (accents, grec, allemand, turc, ligatures,
  chiffres d'autres écritures) ;
- les refus : une fonction sans type utilisée comme valeur, `return 3` dans une fonction sans type,
  `return` seul dans une fonction typée, `s[0] = "x"`, une méthode qui n'existe pas ;
- les arrêts : `s[i]` hors du texte, `s.index(x)` sans `x`.
