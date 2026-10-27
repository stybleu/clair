# Audit de Clair V0.50

## Tests et compatibilité

- 238 tests officiels réussis lors de l'audit initial.
- 12 compilateurs historiques reconstruits sur 12.
- 10 contrôles numériques historiques XOR réussis.
- 6 contrôles numériques historiques float32 réussis.
- Résultats float64 identiques entre V0.49 et V0.50.
- 500 vérifications d'intervalles certifiés réussies.

## Performances

- Instructions NEON conservées sur les benchmarks examinés.
- Aucune régression générale de performance démontrée.
- Collecteur V4 : suppression du comportement quadratique observé.

## Limites connues

Les instances de classes restent conservées en arène
jusqu'à la fin de l'exécution.

Les contrôles réalisés ne constituent pas une preuve
exhaustive d'absence de défauts.

## Publication

V0.50 ajoute les calculs certifiés et les améliorations
de gestion mémoire. La reconstruction et les tests
officiels sont obligatoires avant publication.
