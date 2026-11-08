# Clair Studio — Navigation Android V6.3

## Statut

Ces fonctionnalités appartiennent au laboratoire Android
de Clair Studio. Elles ne sont pas encore intégrées au
code de l'application stable dans la branche `main`.

## Coloration syntaxique

L'éditeur expérimente une coloration enrichie pour
identifier plus facilement les classes, fonctions,
méthodes, attributs et variables.

## Autocomplétion

L'éditeur propose des membres et des méthodes selon
le contexte. Par exemple, après `moteur.`, il peut
proposer les attributs et méthodes de la classe Moteur.

Cette analyse est réalisée dans l'éditeur : elle ne
remplace pas les vérifications du compilateur Clair.

## Aller à la définition

Dans l'application Android du laboratoire :

1. Effectuer un appui long sur un identifiant.
2. Choisir « Aller à la définition ».
3. L'éditeur cherche sa déclaration dans le fichier.
4. La ligne retrouvée est mise en évidence.

Le surlignage utilise un fond coloré, un repère
dans la marge et un texte plus marqué.

## Menu Android

Le menu natif conserve les actions de sélection
habituelles : Couper, Copier, Coller et Tout sélectionner.

L'action « Aller à la définition » a été ajoutée
dans ce menu pour éviter d'encombrer la barre inférieure.

Les commandes Annuler et Refaire restent accessibles
dans la barre d'édition. Elles ne servent pas à
revenir à un emplacement précédent du programme.

## Retour au code

Une fonction de retour à l'emplacement initial a
été expérimentée. Son comportement dans le menu
contextuel reste à fiabiliser avant intégration.

Elle n'est donc pas considérée comme une fonctionnalité
validée pour l'application stable.

## Tests et limites

La navigation et le surlignage ont été observés
sur Android dans le laboratoire.

Un scénario utilisant un programme de 150 lignes
a été essayé avec succès.

La reconnaissance des définitions reste limitée :
certaines expressions complexes, les noms ambigus
et les fichiers importés peuvent ne pas être gérés.

Le menu contextuel doit encore subir une validation
complète avant son intégration à l'application stable.

## Compatibilité

Ces développements concernent uniquement l'interface
Clair Studio.

Le langage Clair, son compilateur en C et le backend
Clang/LLVM ne sont pas modifiés.
