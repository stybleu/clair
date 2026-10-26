# Transmission V0.44 → V0.45

La V0.45 (fonctions mathématiques pour `bigint` et `bigfloat`) part de la V0.43. Entre les deux,
la V0.44 a touché aux fichiers des grands nombres. Claude reportera la V0.45 sur la V0.44 ; cette
note dit exactement ce qui a changé, pour que rien ne se perde.

## `src/clair_bigint.c` et `src/clair_bigint.h`

- `reserve()` ne met plus à zéro la mémoire ajoutée (`memset` retiré). Une fonction qui cumule
  dans son résultat (`|=`, `+=`) doit le mettre à zéro elle-même. C'est fait dans :
  `clair_big_mul` (avant `mul_words`), `clair_big_shift_left`, et la division bit à bit
  (`CLAIR_BIG_FORCE_BIT_DIVISION`). **À vérifier pour tout nouveau code de la V0.45 qui appelle
  `reserve()`.**
- `clair_big_mul` : si un des deux nombres n'a qu'un mot, une seule boucle (sans mise à zéro).
- `add_abs` et `sub_abs` : la boucle sur les mots communs, puis la retenue sur le reste (plus de
  test `i < len` à chaque mot).
- nouvelle fonction `clair_big_addmul_i64(out, a, b, k)` : `out = a + b * k` en un passage quand
  les signes s'accordent, sinon multiplication puis addition.

## `src/clair_big_numbers_rt.c` et `src/clair_big_numbers_rt.h`

- `clair_rt_bigint_add_int`, `sub_int`, `int_sub`, `mul_int`, `div_int`, `mod_int`, `int_div`,
  `int_mod`, `cmp_int`, `addmul_int` : un `bigint` avec un `int64_t`. L'`int` devient un
  `ClairDynInt` d'un mot **sur la pile** (`CLAIR_SMALL_INT`), jamais enregistré dans le
  collecteur.
- (V0.42, déjà sur GitHub) collecteur par zones d'appel : `ClairNumFrameGuard`,
  `clair_rt_num_keep`, `clair_rt_num_pin`. Un nouveau résultat doit être créé par
  `clair_new_int()` / `clair_new_float()` pour être suivi.

## Compilateur

- `src/semantic.c` : `bigint op int` et `int op bigint` sont permis (`+ - * / %`, comparaisons) ;
  `bigint op float` et `bigint op bigfloat` restent refusés.
- `src/codegen_c.c` : appels `clair_rt_bigint_*_int` ; `a + b * k` devient
  `clair_rt_bigint_addmul_int(a, b, k)`.
- `tests/big_numbers_invalid_v040.cl` vérifie maintenant `bigint + 1.5` (refusé) au lieu de
  `bigint + 1` (permis).

## Rappels

- Le code des grands nombres est recopié dans le C de chaque programme (V0.42,
  `tools/embed_runtime.cmake`) : un nouveau fichier `src/clair_*.c` doit être ajouté à la liste
  `PARTS` de ce fichier et à `CLAIR_BIG_PARTS` dans `CMakeLists.txt`.
- Un nom `static` doit rester unique entre `clair_bigint.c`, `clair_bigfloat.c` et
  `clair_big_numbers_rt.c` (ils deviennent un seul fichier C) ; `pow10` a été renommé
  `clair_bf_pow10` pour cette raison.
