# Clair V0.27

## Couche dense : 4 lignes à la fois

La boucle d'une couche de réseau de neurones (produit matrice × vecteur) :

```text
for i in range(rows(w)) {
    let s: float = b[i]
    for j in range(cols(w)) {
        s = s + w[i][j] * x[j]
    }
    y[i] = sigmoid(s)
}
```

est maintenant calculée 4 lignes à la fois :

```c
for (; clair_row + 4 <= w->rows; clair_row += 4) {
    double clair_s0 = clair_data_b[clair_row];
    ...
    for (int64_t j = 0; j < w->cols; j++) {
        double clair_x = clair_data_x[j];
        clair_s0 = clair_s0 + clair_w0[j] * clair_x;
        clair_s1 = clair_s1 + clair_w1[j] * clair_x;
        clair_s2 = clair_s2 + clair_w2[j] * clair_x;
        clair_s3 = clair_s3 + clair_w3[j] * clair_x;
    }
    {
        int64_t i = clair_row;
        double s = clair_s0;
        /* y[i] = sigmoid(s), comme écrit */
    }
    ...
}
```

### Pourquoi c'est plus rapide

Dans la boucle simple, chaque addition attend le résultat de la précédente : le processeur passe
son temps à attendre. Avec 4 sommes indépendantes, il en fait avancer 4 en même temps, et `x[j]`
n'est lu qu'une fois pour les 4 lignes.

Mesure sur PC (nouveau programme `couche` du rapport, 256 → 256, 4000 fois) : Clair 0,067 s,
C écrit à la main 0,119 s, Clair V0.26 0,135 s.

### Résultat identique au bit près

Chaque ligne garde ses propres additions, dans le même ordre (`j` croissant). Le résultat est donc
exactement celui de la boucle simple. Ce qui suit la somme (`y[i] = sigmoid(s)`, un `print`, un
total…) s'exécute ligne par ligne, dans l'ordre d'origine.

### Quand Clair garde la boucle simple

- `x`, `w` ou `b` trop courts : l'erreur arrive au même moment qu'avant ;
- la liste écrite après la somme (`y`) est aussi `x`, `w` ou `b` ;
- ce qui suit la somme contient une boucle, `break`, `continue`, un `append`, ou appelle une
  fonction qui écrit dans une liste ;
- la somme ne commence pas par un nombre, une variable ou `b[i]`.

### Formes reconnues

- `w` matrice (`w[i][j]`) ou liste à plat (`w[i * n + j]`), variable ou champ (`self.w`) ;
- `x` liste, `b` liste, variable ou champ ;
- les opérandes de `+` et de `*` dans n'importe quel ordre.

## Calculs à virgule identiques partout

Sur le téléphone, le premier essai de la V0.27 a montré des différences au dernier chiffre entre
la boucle simple et la version 4 lignes. La cause n'était pas la transformation : sur ARM, Clang
a le droit de fusionner `a * b + c` en une seule instruction (FMA, un seul arrondi au lieu de
deux), et il le fait à certains endroits mais pas à d'autres. Dans la boucle simple elle-même, la
partie vectorisée faisait la multiplication et l'addition séparément, et la fin de boucle les
fusionnait.

Clair interdit maintenant cette fusion cachée (`-ffp-contract=off`) : chaque `+ - * /` est
arrondi séparément, comme en Python. Le même calcul donne donc exactement le même résultat sur PC
et sur téléphone, avec ou sans les transformations de Clair (produit de matrices en blocs, couche
4 lignes à la fois).

L'option `--fma` rend cette liberté à Clang, pour gagner un peu de vitesse sur ARM quand le
dernier chiffre n'a pas d'importance :

```sh
./build/clair calcul.cl -o calcul --fma
```

Le rapport du code machine compile le C écrit à la main avec la même règle, pour que la
comparaison reste juste.

## Rapport du code machine

Nouveau programme `couche` : une couche dense 256 → 256 avec sigmoïde, comparée à la même boucle
écrite à la main en C.

## Tests

- `matvec_rows.cl` : tailles de 1×1 à 130×17 et 7×300, chaque couche comparée case par case à la
  boucle non reconnue (0 différence), champs `self.w` et `self.b`, liste à plat avec `print` à
  chaque ligne, `y` égale à `x` ;
- `matvec_rows_bounds.cl` : `x` trop court, erreur au même moment qu'avant ;
- 750 couches tirées au hasard (formes, ordres, alias, tailles qui ne rentrent pas, classes,
  `print`, `if`, totaux) affichées avec 17 chiffres : sortie identique à la V0.26 ; les 4 lignes
  ont vraiment tourné dans 244 des 400 premiers programmes ;
- sur ARM64, le code de tous les programmes du rapport ne contient plus aucune instruction
  fusionnée (`fmadd`, `fmla`) quand `--fma` n'est pas donné.
