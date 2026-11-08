# Clair V0.50 — Calculs certifiés

## Nouveautés

Clair V0.50 introduit le type certfloat, permettant
de réaliser des calculs avec des intervalles numériques
dont les bornes sont représentées exactement.

Les erreurs d'exécution disposent également
d'informations améliorées sur leur ligne source.

## Calculs certifiés

Fonctionnalités principales :

- Création de nombres certfloat.
- Addition, soustraction et multiplication.
- Division avec contrôle des intervalles.
- Racine carrée certifiée avec certsqrt().
- Décimales garanties avec certdigits().
- Gestion des erreurs numériques.

Les décimales qui ne peuvent pas être garanties
ne sont pas présentées comme exactes.

## Gestion mémoire

Le collecteur numérique V4 prend en charge
bigint, bigfloat et certfloat.

Il évite de parcourir systématiquement les nombres
protégés par les champs des objets.

Les chaînes identiques produites par certdigits()
peuvent être partagées afin de réduire leur coût.

Les chaînes temporaires utilisées directement
pour l'affichage sont libérées après utilisation.

## Résultats de validation

- 238 tests historiques adaptés réussis.
- 14 programmes compatibles avec la V0.49.
- 13 benchmarks validés fonctionnellement.
- 500 intervalles mathématiques vérifiés.
- 5 contrôles d'erreurs et de types réussis.
- Tests ASan et UBSan réussis.
- Parcours du collecteur linéaires sur le test
  instrumenté de création de 30 000 objets.

## Limites connues

Les instances de classes utilisent encore une
arène mémoire conservée jusqu'à la fin du programme.

Une instance devenue inaccessible n'est pas
automatiquement récupérée individuellement.

Les nombres conservés dans ses champs peuvent
rester alloués.

Une collecte générale des instances et des chaînes
reste une amélioration possible.

Les résultats des tests ne garantissent pas
l'absence de défauts sur tous les programmes.

## Statut

Validation technique expérimentale terminée
sur Android ARM64 avec Termux.

L'audit général historique de Clair V0.1 à V0.50
et la préparation de la publication restent à réaliser.
