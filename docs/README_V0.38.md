# Clair V0.38

## Fichiers CSV de nombres

```text
def main() -> int {
    let d: matrix[float] = load_csv("fleurs.csv")          // 30 lignes, 4 colonnes
    let x: matrix[float32] = load_csv("images.csv", "float32")
    let classes: matrix[int] = load_csv("classes.csv", "int")
    print(rows(d))
    save_csv("resultats.csv", d)
    return 0
}
```

- `load_csv(chemin)` lit un fichier de nombres dans une `matrix[float]` ; avec `"float32"` ou
  `"int"` en second argument, dans une `matrix[float32]` ou une `matrix[int]` ;
- les valeurs sont séparées par `,`, `;`, des tabulations ou des espaces : Clair regarde la
  première ligne ;
- une première ligne qui n'est pas faite de nombres est un en-tête (`longueur,largeur,espece`)
  et est sautée ; les lignes vides aussi ; les fins de ligne Windows (`\r\n`) sont acceptées ;
- les nombres s'écrivent comme en Python : `2.5`, ` 3 `, `-1e-3`, `1_000`, `inf`, `nan` ;
- une erreur arrête le programme en disant où elle est :
  `load_csv(): line 7, value 3 of 'fleurs.csv' is not a number: 'abc'`,
  `load_csv(): line 9 of 'fleurs.csv' has 3 values instead of 4`,
  `cannot open file 'fleurs.csv': No such file or directory` ;
- `save_csv(chemin, m)` écrit une ligne par rangée, valeurs séparées par `,`, chaque nombre
  avec le moins de chiffres qui redonnent exactement le même nombre, écrit comme `str()` en
  Python (`0.1`, `100.0`, `1e+20`) : relire le fichier redonne exactement la même matrice.

## Lignes de texte, `split`, `strip`, nombres dans des chaînes

```text
for ligne in read_lines("notes.txt") {           // Alice : 15, 12.5, 18
    let parties: list[string] = split(ligne, ":")
    let nom: string = strip(parties[0])
    let somme: float = 0.0
    for note in split(parties[1], ",") {
        somme += float(note)
    }
}
```

- `read_lines(chemin)` : toutes les lignes du fichier, sans leur fin de ligne ;
- `split(s, sep)` coupe à chaque `sep` (`split("a,,b", ",")` donne `a`, `""` et `b`) ;
  `split(s)` coupe aux espaces, sans mot vide ; comme en Python ;
- `strip(s)` enlève les espaces au début et à la fin ;
- `int(s)`, `float(s)` et `float32(s)` lisent un nombre écrit dans une chaîne, avec les règles de
  Python : `int(" -42 ")` vaut -42, `int("1.5")` arrête le programme
  (`invalid literal for int() with base 10: '1.5'`), `float("abc")` aussi
  (`could not convert string to float: 'abc'`).

## Les chaînes comprennent `\n`, `\t`...

Comme en Python, `"\t"` est une tabulation, `"\n"` un retour à la ligne ; aussi `\r`, `\\`,
`\"`, `\'` et `\xHH`. Avant, ces deux caractères restaient écrits tels quels.

## Le rapport

Avant chaque mesure, le rapport vérifie que le cœur choisi répond toujours. Si le téléphone l'a
éteint (économie d'énergie, chaleur), il l'écrit au lieu d'afficher des temps faux, et il
prévient quand des cœurs sont éteints.

## Tests

- `fichiers.cl` : CSV avec en-tête, moyennes des colonnes, lecture en `float32` et en `int`,
  fichier séparé par des espaces et des tabulations avec fins de ligne Windows et ligne vide,
  écriture puis relecture identique, entiers de 64 bits, notes lues ligne par ligne avec
  `split`, `strip` et `float()` ; sortie identique à Python ;
- `csv_missing_runtime.cl`, `csv_bad_value_runtime.cl`, `csv_ragged_runtime.cl`,
  `float_string_runtime.cl`, `int_string_runtime.cl`, `split_empty_runtime.cl`,
  `invalid_load_csv_kind.cl` ;
- 300 fichiers CSV tirés au hasard (séparateurs, en-têtes, espaces, fins de ligne, nombres
  écrits de toutes les façons, `float`, `float32`, `int`) : lecture et écriture identiques à
  Python ; sans erreur d'AddressSanitizer ni d'UndefinedBehaviorSanitizer.
