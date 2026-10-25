# Clair V0.40 — Grands nombres dynamiques (candidate expérimentale)

Cette version expérimentale ajoute deux types indépendants des entiers fixes `int128` à `int1024` :

- `bigint` : entiers signés arbitraires, calculs exacts, multiplication classique/Karatsuba/Toom-Cook 3 et division Knuth.
- `bigfloat` : flottant **décimal** à précision configurable (de 1 à 1 000 000 chiffres significatifs), avec arrondi au plus proche, égalité au chiffre pair.

## Exemples

```clair
let n: bigint = bigint("123456789012345678901234567890")
let k: bigint = bigint("999999999999999999999999999999")
print(n * k)
let planck: bigfloat = bigfloat("1.616255e-35", 80)
let tres_petit: bigfloat = bigfloat("1e-1000000000", 80)
print(planck / tres_petit)
```

Les deux types prennent leurs valeurs initiales sous forme de **chaînes décimales**, afin de ne jamais passer par `int64` ni `double` (qui perdraient précision ou amplitude). Les conversions implicites avec `int`, `float`, `float32` et les autres types ne sont pas prises en charge dans cette première version.

`bigint` fournit `+`, `-`, `*`, `/`, `%`, les comparaisons et `print`. La division entière s'effectue **vers zéro** ; le reste possède le signe du dividende.

`bigfloat` fournit `+`, `-`, `*`, `/`, les comparaisons exactes des valeurs représentées et `print`. Lorsqu'on combine deux précisions différentes, le calcul utilise la plus élevée. Le format est décimal scientifique, et non la précision binaire MPFR. Une représentation possible de `1e-1000000000` est stockée sans un milliard de zéros.

## Limites et travaux restant à qualifier

1. **Gestion de la mémoire :** les objets sont immuables et libérés en fin de processus par un registre `atexit`. Les valeurs intermédiaires ne sont pas réclamées pendant les boucles ; une application réalisant des millions d'opérations peut accumuler beaucoup de mémoire. Une gestion de durée de vie par blocs ou comptage de références reste nécessaire.
2. **Précondition de compilation :** invoquer `./build/clair` depuis la racine du dépôt : les sources de runtime sont recherchées dans `src/`. L'installation du compilateur dans un autre répertoire n'est pas encore gérée.
3. **Sémantique :** les listes et matrices de `bigint`/`bigfloat`, les conversions et les méthodes ne sont pas encore intégrées.
4. **Précision :** la preuve exhaustive d'un arrondi correct dans tous les cas difficiles reste à établir ; comparaison à `decimal` de Python réussie sur les jeux testés, mais cela ne constitue pas une preuve générale.
5. **Performances :** les seuils de multiplication et la consommation mémoire doivent encore être mesurés dans de vrais programmes Clair.

## Validation

Le moteur autonome V4 a déjà réussi les tests précédemment exécutés sous Termux ARM64. Le **runtime d'adaptation V0.40** a été vérifié sous Clang et AddressSanitizer/UndefinedBehaviorSanitizer dans un environnement Linux. La compilation et la validation de Clair modifié ont réussi sur Termux ARM64, y compris la régression complète et l’audit mémoire combiné.

L'intégrateur refuse toute version différente de `b82d730c38e9cf0b1a35074eb32fe03f7585e56d` et tout arbre Git non propre. Il ne crée ni commit, ni push, ni changement sur GitHub.


## Mise à jour mémoire validée sur ARM64

La première candidate récupérait tous les objets en fin de processus.
La version testée utilise désormais un **collecteur expérimental à points sûrs**,
avec racines des variables locales et paramètres. Sur Termux/Android ARM64,
l'audit combiné de 300 000 opérations `bigint` et 100 000 opérations
`bigfloat` a produit les résultats exacts, avec un pic RSS de **11,7 Mio**.
La régression intégrale `tests/run_tests.sh` a réussi.

**Limites :** la collecte est différée dans les fonctions imbriquées et
désactivée lorsque des champs d'objets sont de type `bigint` ou `bigfloat`.
Les cas adversariaux de récursion, d'alias, de contrôle de flux et de
concurrence restent à auditer. Ce n'est pas un GC général.

La prochaine version **V0.41** est réservée à Claude ; lire
`docs/TRANSITION_V0.41_CLAUDE.md`.
