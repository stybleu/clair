# Clair — Différentiation automatique V4.25

## Activation

Compilation avec `--ad` ou `--ad --dce`.

## Fonctions générées

- `derivee_f_x(...)` : dérivée partielle.
- `gradient_f(...)` : gradient complet.
- `gradient_vers_f(..., sortie)` : gradient dans une liste réutilisable.

## Fonctionnalités

- Différentiation directe et inverse.
- Jusqu'à 16 paramètres float testés.
- Opérations +, -, *, /, sin, cos, exp, log et sqrt.
- Élimination des sous-expressions communes (CSE).
- Réutilisation de la mémoire des gradients.
- Écritures optimisées avec contrôle de sécurité.

## Validations

- V4.17 : 8/8 régressions, 5/5 tests ASAN/UBSAN.
- V4.18 : suite historique complète réussie.
- V4.19 : 19/19 programmes, 76/76 exécutions.
- V4.20 : 96/96 dérivées conformes.
- V4.21 : mémoire stable sur le benchmark.
- V4.24 : 137/137 points finis conformes.

## Limites

- Fonctions mathématiques simples avec une expression retournée.
- Boucles et conditions générales non différentiables actuellement.
- Réseaux neuronaux complets non encore pris en charge par AD.
- Limites expérimentales : 128 nœuds, profondeur 32.
- Certains zéros signés et infinis diffèrent entre modes.
- Absence générale de fuites mémoire non démontrée.

## État

Version expérimentale, non publiée sur GitHub main.
L'intégration nécessite un audit et des tests supplémentaires.
