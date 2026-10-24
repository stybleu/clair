# Clair V0.20

Écrire un réseau de neurones avec des classes devient naturel : on modifie directement les
champs, et les boucles sur `self.w[i][j]` sont aussi rapides que sur une variable.

## Écrire à travers un champ

Ces trois écritures demandaient avant une variable intermédiaire :

```text
self.b[i] = 0.5
self.w[i][j] = self.w[i][j] - lr * grad
self.history.append(loss)
net.first.w[0][0] = 9.0      // chaînes de champs aussi
```

## Boucles rapides sur les champs

La version rapide des boucles (V0.17) s'applique maintenant à `self.w[i][j]` et `self.items[i]`.
Le C généré copie une seule fois le pointeur avant la boucle :

```c
double *clair_data_self__w = (double *)self->w->data;
int64_t clair_cols_self__w = self->w->cols;
for (int64_t j = 0; j < clair_stop_j; j++) {
    s = s + clair_data_self__w[i * clair_cols_self__w + j] * clair_data_x[j];
}
```

C'est sûr seulement si rien dans la boucle ne peut remplacer `self.w` par une autre matrice.
Clair le vérifie : pas d'affectation `obj.w = ...` dans la boucle, et pas d'appel à une fonction
qui affecte des champs, directement ou par ses propres appels. Une fonction comme `sigmoid` reste
permise. Sinon, la boucle garde la version vérifiée.

## Exemple complet : apprentissage de XOR

`tests/xor_training.cl` entraîne un réseau 2-4-1 (couche cachée sigmoïde) par descente de
gradient. Le générateur pseudo-aléatoire est écrit en Clair, donc le résultat est le même à chaque
exécution. Après 4000 passes, le réseau donne 0,03 / 0,97 / 0,97 / 0,04 pour les quatre entrées de
XOR, comme la même version écrite en Python.

`tools/bench/reseau.cl` est la même chose en plus grand (2-16-1, 200 000 passes), comparée dans
le rapport du code machine à la même version écrite à la main en C (`reseau_ref.c`).

Mesure sur PC : Clair 0,15 s, C écrit à la main 0,14 s. Les 5 à 10 % d'écart viennent des
vérifications de bornes faites à l'entrée de chaque petite boucle (16 tours seulement) : c'est le
prix de la sécurité, que le C n'a pas.

## Changement

`f().append(x)`, un append sur le résultat d'un appel, reste refusé. Le message dit maintenant :
`append() needs a list variable or field`. Le test `invalid_append_on_field.cl` devient
`invalid_append_on_call.cl`, puisque `obj.items.append(x)` est désormais valide.

## Limites connues

- les boucles `while` n'ont pas encore de version rapide ;
- dans `obj.items.append(f())`, `f()` est évalué avant de lire `obj.items` : cela ne change le
  résultat que si `f` remplace `obj.items`.
