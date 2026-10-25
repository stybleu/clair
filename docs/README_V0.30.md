# Clair V0.30

## Plusieurs cœurs

Les grands produits de matrices et les grandes couches denses se partagent maintenant entre tous
les cœurs du processeur. Rien ne change dans le programme : Clair reconnaît les mêmes boucles
qu'avant (produit de matrices depuis la V0.25, couche dense depuis la V0.27).

```text
def couche(w: matrix[float], b: list[float], x: list[float], y: list[float]) -> int {
    for i in range(rows(w)) {
        let s: float = b[i]
        for j in range(cols(w)) {
            s = s + w[i][j] * x[j]
        }
        y[i] = sigmoid(s)
    }
    return 0
}
```

### Quand

- produit de matrices : à partir d'environ un million de multiplications (100 × 100 × 100) ;
- couche dense : à partir de 32 768 multiplications (par exemple 256 entrées × 128 sorties).

En dessous, un seul cœur : réveiller les autres coûterait plus que ce qu'ils feraient gagner.

### Résultat identique au bit près

Chaque ligne du résultat est calculée en entier par un seul cœur, avec les additions dans le
même ordre que la boucle simple. Le résultat ne dépend donc pas du nombre de cœurs : 1, 2, 3 ou
8 cœurs donnent exactement les mêmes chiffres.

### Choisir le nombre de cœurs

Par défaut, Clair prend tous les cœurs que le système donne au programme (16 au plus). La
variable `CLAIR_THREADS` choisit :

```sh
CLAIR_THREADS=1 ./programme     # un seul cœur
CLAIR_THREADS=4 ./programme     # quatre cœurs
```

### Comment

- Les threads sont lancés la première fois qu'un grand calcul arrive, puis restent prêts.
- Le travail est coupé en paquets de lignes (8 lignes pour un produit, 16 pour une couche).
  Chaque cœur prend le paquet libre suivant : les grands et les petits cœurs du téléphone se
  partagent le travail sans que l'un attende l'autre.
- Couche dense : toutes les sommes sont d'abord calculées sur tous les cœurs, puis la suite de la
  boucle (`y[i] = sigmoid(s)`, un `print`, un `return`...) s'exécute ligne par ligne, dans
  l'ordre écrit. C'est possible parce que les sommes ne lisent que `w`, `x` et `b`, et que Clair
  vérifie que la suite ne peut pas les modifier.

Le C généré :

```c
double clair_sums_small[CLAIR_SUMS_SMALL];
double *clair_sums = clair_rt_sums_make(w->rows, clair_sums_small);
for (int64_t i = 0; i < w->rows; i++) clair_sums[i] = clair_data_b[i];
clair_rt_matvec_f64(w->rows, w->cols, clair_data_w, clair_cols_w, clair_data_x, clair_sums);
for (int64_t i = 0; i < w->rows; i++) {
    double s = clair_sums[i];
    {
        int64_t __clair_list_index_0 = i;
        double __clair_list_value_0 = clair_fn_sigmoid(s);
        *clair_rt_list_at_f64_checked(y, __clair_list_index_0) = __clair_list_value_0;
    }
}
clair_rt_sums_free(clair_sums, clair_sums_small);
```

Pour une petite couche (64 sorties au plus), les sommes restent sur la pile : aucune allocation à
chaque appel.

## Rapport du code machine

Le tableau principal ne change pas : Clair et C tournent sur un seul cœur, le plus rapide. Un
second tableau, `PLUSIEURS COEURS`, lance `matmul`, `matrice` et `couche` sur un cœur puis sur
tous, et donne le gain.

## Tests

- `plusieurs_coeurs.cl` : produits de matrices et couches denses assez grands pour plusieurs
  cœurs, tailles qui ne tombent pas juste sur les paquets, `return` au milieu des lignes, ordre
  de la suite de la boucle ; résultat identique au bit près au calcul simple, et même sortie
  avec `CLAIR_THREADS=1`, `2`, `3` et `7` ;
- produits et couches tirés au hasard, petits et grands : même sortie, au bit près, que la
  boucle simple des anciennes versions, et même sortie avec 1 et 3 cœurs ;
- ThreadSanitizer (accès concurrents) et AddressSanitizer : aucune erreur.
