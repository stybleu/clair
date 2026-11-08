# Clair V0.31

## Toute la couche dense sur tous les cœurs

En V0.30, sur le téléphone (8 cœurs), la couche dense n'allait que 1,3 fois plus vite qu'avec
un seul cœur. Les mesures ont montré deux freins :

- seules les sommes étaient partagées : `y[i] = sigmoid(s)` restait sur un seul cœur ;
- chaque partage coûtait cher : le programme attendait que chaque cœur ait répondu, même ceux
  qui n'avaient plus rien à faire.

### La suite de chaque ligne aussi

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

Quand ce qui suit la somme ne fait que calculer et écrire à la ligne `i`, chaque cœur fait ses
lignes jusqu'au bout : les sommes, puis la suite. Le C généré :

```c
/* dense layer of couche: the sums of rows [first, last), then the rest of each row */
typedef struct {
    ClairMatvecJob sums;
    double *clair_data_y;
} ClairLayer_couche_1;

static void clair_layer_couche_1(const void *clair_job, int64_t clair_first, int64_t clair_last) {
    const ClairLayer_couche_1 *clair_layer = clair_job;
    clair_rt_matvec_rows(&clair_layer->sums, clair_first, clair_last);
    double *clair_data_y = clair_layer->clair_data_y;
    for (int64_t i = clair_first; i < clair_last; i++) {
        double s = clair_layer->sums.s[i];
        clair_data_y[i] = clair_fn_sigmoid(s);
    }
}
```

et, dans `couche` :

```c
ClairLayer_couche_1 clair_layer = { { w->cols, clair_cols_w, clair_data_w, clair_data_x, clair_sums }, (double *)y->data };
clair_rt_layer_f64(w->rows, w->cols, clair_layer_couche_1, &clair_layer);
```

Ce qui est permis après la somme :

- écrire dans des listes de nombres à la ligne `i` (`y[i] = ...`, `z[i] = s`, `signe[i] = 1`) ;
- des variables locales (`let t: float = ...`), des `if` / `else` ;
- les calculs à virgule, `exp`, `tanh`, `sqrt`, `max`... et les fonctions qui ne font que
  calculer avec des nombres (`sigmoid`, `relu`, une activation avec un `if`) ;
- lire les variables de la fonction (`pente`, `seuil`...).

Sinon (un `print`, la lecture d'une liste, `total = total + s`, un `return`, une division
entière...), Clair garde la façon de la V0.30 : les sommes sur tous les cœurs, puis la suite ligne
par ligne, dans l'ordre.

Le résultat reste identique au bit près, quel que soit le nombre de cœurs. Clair vérifie avant
la boucle que les listes écrites sont assez longues ; sinon la boucle simple s'exécute et
l'erreur arrive à la même ligne qu'avant.

### Un partage plus léger

Le programme n'attend plus que les paquets de lignes en cours de calcul, et non les cœurs qui
arrivent en retard. Il ne fait plus d'appel au système quand les autres cœurs sont déjà
réveillés. Mesuré sur le téléphone avec 8 cœurs, avant la suite de chaque ligne : la couche
passe de 0,108 s à 0,088 s.

## Tests

- `plusieurs_coeurs.cl` : couche entière avec deux fonctions d'activation, un `if` / `else`,
  trois listes écrites (dont une liste d'entiers), une variable lue puis redéclarée, une méthode
  qui écrit `self.h[i]` ; identique au bit près à la boucle simple avec 1, 2, 3 et 7 cœurs ;
- `plusieurs_coeurs_bounds.cl` : liste écrite trop courte, l'erreur arrive à la même ligne ;
- couches denses tirées au hasard, petites et grandes, avec et sans suite reconnue : même
  sortie, au bit près, que la boucle simple des anciennes versions, et même sortie avec 1 et 3
  cœurs ;
- ThreadSanitizer et AddressSanitizer : aucune erreur.
