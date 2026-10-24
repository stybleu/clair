# Clair V0.21

Correction de performance trouvée grâce au rapport du code machine sur ARM64.

## Le problème

Sur le téléphone, le programme `reseau` (apprentissage de XOR) était 11 % plus lent que le même
programme écrit à la main en C : 0,261 s contre 0,234 s. En comparant l'assembleur ARM64, la boucle
d'apprentissage du C était vectorisée (27 instructions NEON) et celle de Clair ne l'était pas.

La cause : la boucle lit `self.w1[i][0]` et `self.w1[i][1]`. En C écrit à la main, la largeur de
ligne est la constante `2` (`w1[i * 2]`, `w1[i * 2 + 1]`), et Clang charge les deux colonnes d'un
coup (`ld2` / `st2`). Dans le C de Clair, la largeur était `w1->cols`, connue seulement à
l'exécution : Clang ne savait pas vectoriser un pas inconnu.

## La correction

Quand une boucle rapide n'utilise que des colonnes constantes d'une matrice (`m[i][0]`,
`m[i][1]`…), Clair ajoute une copie de la boucle où la largeur est écrite en dur :

```c
int64_t clair_cols_self__w1 = self->w1->cols;
if (clair_cols_self__w1 == 2) {
    /* row width known: Clang can vectorize like hand-written C */
    for (int64_t i = 0; i < clair_stop_i; i++) {
        ...
        clair_data_self__w1[i * 2] = clair_data_self__w1[i * 2] - lr * dh * x0;
        clair_data_self__w1[i * 2 + 1] = clair_data_self__w1[i * 2 + 1] - lr * dh * x1;
    }
} else {
    /* même boucle avec la largeur lue à l'exécution */
}
```

C'est le cas courant des matrices à largeur fixe : points 2-D ou 3-D, couleurs RGB, poids d'une
couche à 2 entrées. La boucle d'apprentissage est maintenant vectorisée en NEON (`ld2` / `st2`),
comme la version écrite à la main.

Aucun programme ne change de résultat : les 122 programmes de test et de mesure donnent la même
sortie qu'avec la V0.20.
