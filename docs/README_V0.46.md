# Clair V0.46

## Lire l'entrée, écrire sans aller à la ligne, les octets

```text
let nom: string = input("Nom : ")              // une ligne de l'entrée, comme en Python
let n: int = int(input())
for ligne in read_lines() {                    // tout le reste de l'entrée, ligne par ligne
    write("[" + ligne + "] ")                  // write : sans aller à la ligne
}
let texte: string = read_text("notes.txt")     // tout un fichier
let data: list[u8] = read_bytes()              // toute l'entrée, en octets
write_file("copie.bin", data)
```

| fonction | donne |
|---|---|
| `input()`, `input(invite)` | la ligne suivante de l'entrée, sans sa fin de ligne ; l'invite est écrite avant |
| `read_lines()`, `read_lines(chemin)` | toutes les lignes (de l'entrée, ou d'un fichier comme avant) |
| `read_text()`, `read_text(chemin)` | tout le texte, `\r\n` devient `\n` comme en Python |
| `read_bytes()`, `read_bytes(chemin)` | tous les octets, tels quels, dans une `list[u8]` |
| `write(s)`, `write(octets)`, `write(octets, n)` | écrit sans aller à la ligne (les `n` premiers octets) ; rend le nombre d'octets |
| `write_file(chemin, s)`, `write_file(chemin, octets)` | crée ou remplace le fichier |
| `encode(s)`, `decode(octets)` | le texte en octets (UTF-8), et l'inverse |
| `ord(c)`, `chr(n)` | le numéro d'un caractère (`ord("é")` vaut 233) et le caractère d'un numéro (`chr(8364)` donne `€`) |

- Sans chemin, `read_lines`, `read_text` et `read_bytes` lisent l'entrée (ce qui est tapé, ou
  `programme < fichier`, ou `autre | programme`).
- `input()` à la fin de l'entrée arrête le programme : `EOF when reading a line`, comme Python.
- Une chaîne Clair ne peut pas contenir l'octet 0 : `input()`, `read_text()` et `decode()`
  s'arrêtent s'ils en trouvent un (`read_bytes()` le garde).
- `print` et `write` écrivent au même endroit, dans l'ordre.
- Une liste écrite directement est une `list[u8]` : `write([10])`, `decode([104, 105])`,
  `write_file("f.bin", [65, 0, 66] * 2)`.
- Une chaîne Clair est une suite d'octets : `len("é")` vaut 2. `ord` lit quand même le caractère
  entier (`ord("é")` vaut 233 comme en Python), et `chr(233)` donne les deux octets de `é`.
- Erreurs, en anglais comme les autres : `ord() expected a character, but string of length 2
  found`, `chr() arg not in range(0x110000)`, `write(): 5 bytes asked from a list of length 3`.

## Des listes toutes faites : `[0] * n`

```text
let seq: list[u8] = [0] * n          // n octets à zéro
let motif: list[int] = [1, 2] * 3    // 1 2 1 2 1 2
let vide: list[int] = [7] * -2       // vide, comme en Python
```

Une seule allocation, remplie d'un coup (mise à zéro par le système quand c'est 0). Avant, il
fallait une boucle de `append`.

## Plus rapide

- **`xs.append(v)`** : quand la liste a encore de la place, l'ajout se fait sur place, sans
  appel de fonction ; seule une liste pleine appelle la partie qui l'agrandit. Clang le faisait
  déjà dans les petits programmes, pas toujours dans les grands : une première version de
  reverse-complement, qui ajoutait chaque octet un par un, est passée de 0,113 s à 0,099 s sur
  la machine de test.
- **Une table lue avec un octet** : dans `comp[b]` avec `b` un `u8` (ou un `u16`), l'indice
  est entre 0 et 255 quoi qu'il arrive. La boucle vérifie une seule fois que `comp` a au moins
  256 cases, puis lit sans vérifier :

  ```c
  if (clair_rt_affine_ok(ligne, 0, 1, 0, m) && clair_rt_affine_ok(comp, 0, 0, 0, m) &&
      clair_rt_affine_ok(comp, 255, 0, 0, m) && clair_rt_affine_ok(seq, j, -1, 0, m)) {
      for (int64_t i = 0; i < m; i++) {
          clair_data_ligne[i] = clair_data_comp[clair_data_seq[j - i]];
      }
  } else { /* la boucle d'origine */ }
  ```

- **Un parcours qui ne fait qu'avancer** : dans

  ```text
  while i < n {
      if data[i] == 10 {
          break
      }
      i += 1
  }
  ```

  `i` part de sa valeur à l'entrée de la boucle et ne fait que monter. La preuve des indices de
  la V0.44 sait maintenant utiliser cette valeur d'entrée comme borne : `data[i]` est vérifié une
  fois avant la boucle (`i >= 0` et `n - 1 < len(data)`), puis lu sans vérification. De même pour
  un indice qui ne fait que descendre.

## Deux nouveaux jeux de tests des langages

- **fasta** (Benchmarks Game) : trois séquences d'ADN, une répétée et deux tirées au hasard avec le
  générateur du jeu, écrites en lignes de 60 lettres avec `write(ligne, m + 1)` ;
- **reverse-complement** : relit la sortie de fasta avec `read_bytes()` et écrit chaque séquence
  à l'envers, chaque lettre remplacée par sa complémentaire (table de 256 octets).

Le rapport mesure fasta pour 2 500 000 et reverse-complement sur ce que fasta écrit pour
2 500 000 (25 Mo). Sur la machine de test : fasta +16 %, reverse-complement +9 % par rapport au C.

## Limites

- Un `\r` seul (anciens fichiers Mac) n'est pas une fin de ligne.
- `print` d'un `float` garde son écriture (`print(1.0)` écrit `1`) ; `str(x)` écrit comme Python.

## Tests

- `entree_sortie.cl`, avec en entrée un texte écrit par la suite de tests : `input` avec invite, `int(input())`,
  `read_lines()` (fins de ligne Windows, ligne vide, dernière ligne sans fin), `write`,
  `write(b, n)`, `encode`, `decode`, `ord`, `chr` jusqu'à 😀, `[x] * n` (`n` négatif aussi),
  `write_file`, `read_text`, `read_bytes`, une table lue avec des octets ; identique à Python ;
- `input_eof_runtime.cl`, `decode_zero_runtime.cl`, `ord_runtime.cl`, `chr_runtime.cl`,
  `write_bytes_runtime.cl`, `read_text_zero_runtime.cl` ; `invalid_write_int.cl`,
  `invalid_decode_string.cl`, `invalid_list_repeat_float.cl`, `invalid_write_count_string.cl` ;
- les jeux fasta (1000) et reverse-complement (sur fasta 1000) : sommes de contrôle de la sortie
  du programme de référence, qui suit l'algorithme officiel ;
- 4000 programmes tirés au hasard (`input`, `read_lines`, `read_text`, `read_bytes`, `write`,
  `write(b, n)`, `encode`, `decode`, `ord`, `chr`, `[x] * n`, fichiers, entrées avec `\r\n`, sans
  dernière fin de ligne, avec un octet 0) : sortie et erreurs identiques à un modèle en Python ;
  1000 d'entre eux sans erreur d'AddressSanitizer, d'UndefinedBehaviorSanitizer ni de fuite ;
- 6937 boucles de parcours tirées au hasard (indice qui monte ou qui descend, départ négatif ou
  après la fin, `break`, `continue`, pas de 0, 1 ou 2, pas dans le mauvais sens, listes trop
  courtes) : même sortie et mêmes erreurs qu'avec la V0.45 ; sans erreur d'AddressSanitizer ;
- 1949 boucles lisant une table avec un `u8` ou un `u16` (tables de 0 à 65 585 cases, valeurs
  dedans et dehors) : même sortie et mêmes erreurs qu'avec la V0.45 ; sans erreur
  d'AddressSanitizer ;
- le C des programmes sans `append`, sans fichiers et sans ces boucles est identique à la V0.45.
