# Clair V0.28

## La mémoire est rendue pendant le programme

Jusqu'ici, toutes les listes et matrices restaient en mémoire jusqu'à la fin du programme. Une
boucle d'entraînement qui crée une liste à chaque tour grossissait donc sans arrêt :

```text
for epoch in range(100000) {
    let grads: list[float] = make(1000, epoch)
    h = make(100, epoch + 1)
    let m: matrix[float] = matrix(20, 20)
    ...
}
```

Ce programme (`tests/memory_freeing.cl`) utilisait 1200 Mo. Il en utilise maintenant 15, et va
4 fois plus vite.

### Ce que fait Clair

Une variable déclarée avec une liste ou une matrice neuve (`[...]`, `matrix(r, c)`, ou l'appel
d'une fonction qui rend une liste neuve, comme `make`) en devient propriétaire, si rien d'autre
ne peut garder cette liste. La liste est alors rendue :

- à la fin du bloc où la variable est déclarée (à chaque tour de boucle) ;
- avant un `break`, un `continue` ou un `return` qui sort de ce bloc ;
- quand la variable reçoit une nouvelle liste (`h = make(...)` rend l'ancienne).

Le C généré le montre :

```c
for (int64_t epoch = 0; epoch < 100000; epoch++) {
    ClairList grads = clair_fn_make(1000, epoch);
    {
        ClairList clair_new = clair_fn_make(100, epoch + 1);
        clair_rt_list_free(h);
        h = clair_new;
    }
    ...
    clair_rt_matrix_free(m);
    clair_rt_list_free(grads);
}
```

### Quand Clair garde la liste jusqu'à la fin, comme avant

Dès qu'une autre référence pourrait survivre, Clair ne rend rien :

- la liste est rangée dans un objet (`sac.items = xs`) ou donnée à un constructeur ;
- elle est copiée dans une autre variable (`let b: list[int] = a`, `b = a`) ;
- elle est donnée à une fonction qui la garde, ou qui la rend (`return xs`) ;
- la variable reçoit autre chose qu'une liste neuve ;
- une boucle `for x in v` parcourt la liste pendant que `v` est remplacée.

Clair calcule lui-même, pour chaque fonction, quels paramètres elle peut garder, et quelles
fonctions rendent une liste neuve.

Les objets, les chaînes et les dictionnaires restent gardés jusqu'à la fin, comme avant.

## Rapport du code machine : programmes plus longs

Sur le nouveau téléphone, plusieurs programmes du rapport ne duraient que 0,05 s : le moindre
bruit pesait 10 %. Ils tournent maintenant 2 à 4 fois plus longtemps (environ 0,2 s chacun sur le
téléphone). Les versions C sont allongées de la même façon et donnent toujours le même résultat.

## Tests

- `memory_freeing.cl` : boucle d'entraînement avec listes, matrice, remplacement de liste,
  `continue`, `break`, `return` au milieu d'une boucle, liste gardée dans un objet ; même sortie
  qu'en V0.27 ; le test vérifie que la mémoire reste sous 200 Mo (15 Mo mesurés, 1200 Mo avant) ;
- `tests/max_memory.c` : petit programme qui mesure la mémoire maximale d'un autre ;
- 1000 programmes tirés au hasard qui créent, partagent, rangent, rendent et remplacent des
  listes et des matrices : même sortie qu'en V0.27 et aucune erreur d'AddressSanitizer
  (utilisation après libération, double libération) ;
- 984 programmes existants dont le C rend maintenant de la mémoire : même sortie qu'en V0.27,
  aucune erreur d'AddressSanitizer.
