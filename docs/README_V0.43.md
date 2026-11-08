# Clair V0.43

## Les jeux de tests des langages

Les langages se comparent sur quelques programmes connus. Le dossier `tools/jeux/` en contient
six, chacun écrit en Clair et en C avec le même algorithme :

| programme   | d'où il vient                       | ce qu'il calcule                                        |
|-------------|-------------------------------------|---------------------------------------------------------|
| `nbody`     | Computer Language Benchmarks Game   | le Soleil et les 4 planètes géantes, énergie avant/après |
| `spectral`  | Computer Language Benchmarks Game   | la norme spectrale d'une matrice infinie                |
| `fannkuch`  | Computer Language Benchmarks Game   | des crêpes retournées sur toutes les permutations       |
| `pidigits`  | Computer Language Benchmarks Game   | les chiffres de pi, avec des `bigint`                   |
| `nqueen`    | PLB2                                | les façons de placer n reines, avec des opérations sur les bits |
| `plbmatmul` | PLB2                                | un produit de matrices fait comme dans PLB2 (b transposée) |

- La taille est lue au clavier (`echo 1000 | ./nbody`).
- Pour de petites tailles, la sortie de Clair est exactement la sortie officielle
  (`-0.169075164` et `-0.169087605` pour `nbody 1000`, `228` et `Pfannkuchen(7) = 16`...) :
  c'est vérifié par les tests.
- Le rapport (`tools/rapport_code_machine.sh`) a un nouveau tableau : chaque programme en Clair
  et en C, sur de grandes tailles (`tools/jeux/tailles.txt`), même cœur, médiane de 5 essais,
  et `RESULTAT DIFFERENT` si les deux n'écrivent pas la même chose.
- La version C de `pidigits` utilise GMP, comme le programme C officiel. Sans GMP, le rapport
  donne seulement le temps de Clair : `pkg install libgmp` sous Termux pour comparer.
- Les versions C sont du C simple, sans instructions SIMD écrites à la main : la même chose que
  ce que Clair écrit. Les programmes C les plus rapides du site officiel vont plus loin.

Ce que ces programmes montrent pour l'instant : les calculs à virgule (`nbody`, `spectral`,
`plbmatmul`) vont presque aussi vite qu'en C. `fannkuch` et `nqueen` paient les vérifications des
indices calculés pendant la boucle (`perm[hi]`, `l[k]`), qu'on ne peut pas prouver d'avance.
`pidigits` mesure les `bigint` face à GMP, une bibliothèque optimisée depuis plus de 30 ans.

## `str()` et `format()`

```text
print("Pfannkuchen(" + str(n) + ") = " + str(m))
print(format(e, ".9f"))            // -0.169075164
print(format(n, "5d"))             // "   42"
print(format(x, "10.3e"))          // " 2.500e+00"
```

- `str(x)` écrit comme Python : `str(0.1)` donne `0.1`, `str(100.0)` donne `100.0`,
  `str(1e20)` donne `1e+20`, `str(true)` donne `True`. Aussi pour les entiers machine, les
  `bigint` et les `bigfloat`. Un `float32` s'écrit comme NumPy (`1.2345678e+07`).
- `format(x, "spec")` reprend la partie courante du mini-langage de Python :
  `[largeur][.précision]` puis `f`, `e`, `g` ou `d`. Le format est écrit dans le programme :
  Clair le vérifie à la compilation.
- 3000 nombres tirés au hasard (très grands, très petits, `-0.0`, nombres à mi-chemin comme
  `2.675`) : `str` et `format` donnent exactement le texte de Python.

## `bigint(k)`

`bigint(2 * k + 1)` crée un grand entier à partir d'un `int`, sans passer par une chaîne.

## Deux boucles de plus vérifiées une seule fois

- **Boucles triangulaires** : `for j in range(i + 1, n)` dans `for i in range(n)` (n-body,
  comparer toutes les paires, une matrice symétrique). Avant, les indices étaient vérifiés à
  chaque tour de la boucle extérieure ; maintenant, une seule fois avant les deux boucles :
  Clair vérifie le plus petit début et la plus grande fin de la boucle intérieure.
- **Boucles à deux bouts** : `while lo < hi { ... xs[lo] ... xs[hi] ...; lo += 1; hi -= 1 }`
  (retourner une liste, un palindrome). `lo` monte, `hi` descend, d'un pas chacun : vérifier
  `lo` et `hi` à l'entrée suffit pour toute la boucle.

Si la vérification échoue, la boucle d'origine tourne, avec ses messages d'erreur au même
moment qu'avant.

## Tests

- les six jeux, sorties officielles pour de petites tailles ;
- `texte_nombres.cl` : `str` et `format` sur des entiers, des `float`, des `float32`, des `u64`,
  des `bigint`, des booléens ; sortie identique à Python ;
- `boucles_bornes.cl`, `triangle_bounds_runtime.cl`, `two_ends_bounds_runtime.cl` ;
- `invalid_format_spec.cl`, `invalid_format_int_d.cl`, `invalid_format_dynamic.cl`,
  `invalid_str_list.cl` ;
- 3000 programmes à doubles boucles dont la boucle intérieure dépend de l'extérieure, et 2000
  boucles à deux bouts (indices hors de la liste, `break`, deux pas au lieu d'un) : même sortie
  et mêmes erreurs qu'avec la V0.42.
