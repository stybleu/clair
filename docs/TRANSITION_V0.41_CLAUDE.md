# Transmission Clair V0.40 → V0.41 — Claude

## Organisation des versions

- **V0.39** : base historique du compilateur (`b82d730c38e9cf0b1a35074eb32fe03f7585e56d`).
- **V0.40** : intégration des types dynamiques `bigint` et `bigfloat`, avec multiplication classique/Karatsuba/Toom-Cook 3, division Knuth et récupération mémoire expérimentale.
- **V0.41** : version dont **Claude créera lui-même la branche et assurera le développement**, en repartant du commit V0.40 publié. Aucune branche V0.41 n'est créée par ce paquet.

## Compatibilité et validation V0.40

Les tests ont réussi sur Termux/Android ARM64 : programme `.cl` compilé et exécuté, suite `tests/run_tests.sh`, audit de 300 000 opérations bigint + 100 000 opérations bigfloat avec résultats exacts. Pic RSS observé dans cet audit combiné : **11,7 Mio**. La mesure dépend du téléphone et ne démontre pas à elle seule l'absence de fuites sur tous les chemins d'exécution.

Les nouveaux nombres sont des objets immuables à représentation dynamique. Les variables et paramètres sont enregistrés en tant que racines pour un collecteur aux points sûrs. Les nombres fixes `int128`, `uint128` jusqu'à 1024 bits restent indépendants et inchangés.

## Points à traiter en V0.41

1. **Mémoire** : auditer récursion, fonctions imbriquées, masquage de variables, alias, `return`, `break`, `continue` et champs des classes. Le collecteur n'est pas général : la collecte est différée pendant les fonctions imbriquées et désactivée pour les programmes utilisant des champs de type `bigint` ou `bigfloat`. Vérifier les valeurs échappées et les chemins d'erreur.
2. **Sémantique et compilateur** : ajouter au besoin conversions explicites, listes/matrices et fonctions avancées, mais préserver le comportement des types existants (`int`, `float`, `float32`, entiers fixes).
3. **Arithmétique et performances** : conserver les références exactes Python/Decimal, multiplier les cas adversariaux et réévaluer les seuils Karatsuba/Toom-Cook 3 sur les différentes architectures.
4. **Portabilité** : le pilote de compilation recherche actuellement les fichiers `src/clair_big*.c` à partir de la racine du dépôt ; adapter le chemin des sources pour une installation système. Le collecteur dépend de l'attribut de nettoyage de Clang.
5. **Documentation** : conserver le `README.md` et toute la documentation publique en **français**.

## Branches et précautions

- Branche publiée par ce paquet : `feature/clair-v040-big-numbers`. Claude choisira et créera sa propre branche V0.41.
- **Ne pas démarrer V0.41 à partir de `main` si V0.40 n'y a pas encore été fusionnée** : utiliser comme base le commit V0.40 publié, puis créer la branche V0.41.
- Ne pas remplacer les tests ARM64 ou modifier la branche historique `feature/int128`.
- Ce paquet ne crée pas la V0.41 et n'annonce aucune fonctionnalité V0.41.
