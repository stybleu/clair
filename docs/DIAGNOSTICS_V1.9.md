# Clair — Système de diagnostics V1.9

## 1. Présentation

Clair possède un système de diagnostics enrichis
pour faciliter la recherche des erreurs de compilation.

Ce système est entièrement écrit en C et conserve
Clang/LLVM comme backend de compilation.

Il permet notamment :
- d'identifier la nature d'une erreur ;
- d'afficher le fichier et la ligne concernés ;
- d'indiquer la colonne de l'erreur ;
- de souligner les identifiants incorrects ;
- de proposer des corrections de noms ;
- de fournir des messages d'aide ;
- de traiter certaines expressions multilignes.

## 2. Activation

Compiler un programme avec :

    ./build/clair programme.cl -o programme --diagnostics

L'option --diagnostics active les messages enrichis.

Sans cette option, le comportement historique des
messages d'erreur est conservé autant que prévu par
l'implémentation.

## 3. Codes des diagnostics

| Code | Signification |
|------|---------------|
| CLAIR-P001 | Expression syntaxique manquante |
| CLAIR-S001 | Variable inconnue |
| CLAIR-S002 | Addition entre types incompatibles |
| CLAIR-S003 | Initialisation ou affectation incompatible |
| CLAIR-S004 | Nombre d'arguments incorrect |
| CLAIR-S005 | Type de retour incompatible |
| CLAIR-S006 | Opération arithmétique invalide |

Les codes P concernent l'analyse syntaxique.
Les codes S concernent l'analyse sémantique.

## 4. Exemple : addition incompatible

Programme :

    def main() -> int {
        let x: string = "bonjour" + 42
        return 0
    }

La compilation échoue, car une chaîne ne peut pas
être additionnée à un entier.

Clair affiche le fichier, la position de l'opérateur
et le diagnostic CLAIR-S002.

L'aide recommande d'utiliser deux nombres ou
deux chaînes de caractères.

## 5. Exemple : variable inconnue

Programme :

    def main() -> int {
        let compteur: int = 12
        print(compteurr)
        return 0
    }

Clair peut suggérer :

    Vouliez-vous dire 'compteur' ?

L'identifiant erroné est souligné entièrement
lorsque sa position peut être déterminée.

## 6. Suggestions intelligentes

La recherche de suggestions utilise la proximité
entre les noms des variables.

Elle prend en compte les variables accessibles
dans la portée courante.

Une variable déclarée dans un bloc devenu
inaccessible ne doit pas être proposée.

Lorsqu'il existe plusieurs correspondances
également plausibles, la suggestion est supprimée.

Les suggestions restent des aides :
elles ne remplacent pas une vérification humaine.

## 7. Positionnement des erreurs

La version V1.8 a introduit le soulignement
complet des identifiants inconnus.

La version V1.9.1 a amélioré le positionnement
des opérateurs en conservant leur colonne
dans l'arbre syntaxique (AST).

Cas vérifiés :
- premier opérateur d'une expression ;
- dernier opérateur d'une expression ;
- opérateur présent dans une chaîne ;
- expression avec parenthèses ;
- plusieurs additions sur une ligne.

Le positionnement dépend toutefois des
informations disponibles pour chaque diagnostic.

## 8. Expressions multilignes

Clair peut localiser certaines erreurs
sur plusieurs lignes.

Exemple :

    def main() -> int {
        let compteur: int = 4
        print(
            compteurr
        )
        return 0
    }

Le diagnostic d'identifiant inconnu doit
pointer sur la ligne contenant compteurr.

## 9. Architecture technique

Le système utilise notamment :

- src/diagnostics.c
- src/diagnostics.h
- src/lexer.c
- src/parser.c
- src/semantic.c
- src/ast.c
- src/ast.h
- src/main.c

L'analyse syntaxique construit l'AST.

L'analyse sémantique vérifie les types,
les symboles et la validité des opérations.

Le système de diagnostics utilise les
informations disponibles pour produire
des messages plus précis.

La génération C et le backend Clang/LLVM
sont conservés.

## 10. Compatibilité

Les diagnostics sont une fonctionnalité du
compilateur et ne modifient pas volontairement
la sémantique des programmes Clair.

Les fonctionnalités existantes doivent rester
compatibles, notamment :
- fonctions et classes ;
- listes et dictionnaires ;
- entiers et flottants ;
- grands nombres ;
- unités physiques ;
- calcul scientifique ;
- différentiation automatique ;
- optimisations du compilateur.

Cette compatibilité est contrôlée par les
tests de régression.

## 11. Tests et validation

Laboratoire V1.9 :

    TESTS_VALIDES=12/12
    AUDIT_CIBLE=OK

Correction du positionnement V1.9.1 :

    TESTS_POSITION=4/4
    CORRECTION_POSITION=OK

Intégration dans Clair principal :

    COMPILATEUR_PRINCIPAL=OK
    DIAGNOSTIC_S002=OK
    REGRESSION_PRINCIPALE=OK

Ces résultats correspondent aux campagnes
de tests réalisées lors du développement.

Ils ne constituent pas une preuve formelle
d'absence de tous les défauts possibles.

## 12. Limites actuelles

Les vérifications réalisées ne démontrent pas
que chaque diagnostic possible possède toujours
une colonne exacte.

Certaines erreurs peuvent encore utiliser
une position approximative.

Les diagnostics portent sur la compilation.
Les erreurs à l'exécution relèvent également
des mécanismes de sécurité du runtime Clair.

De futurs audits pourront étendre :
- les soulignements de plusieurs caractères ;
- la couverture des opérateurs ;
- les suggestions de corrections ;
- les diagnostics dans les modules ;
- les tests de fichiers Unicode ;
- la stabilité sur des programmes plus complexes.

## 13. Historique

V1.3 : localisation et premières aides.
V1.4 : diagnostic S002.
V1.5 : diagnostics S003 à S006.
V1.6 : suggestions de noms.
V1.7 : présentation des aides.
V1.8 : soulignement des identifiants.
V1.9 : audit de robustesse.
V1.9.1 : correction des colonnes d'opérateurs.
V1.9.2 : compatibilité des tests historiques.

## 14. Licence

Clair est distribué sous licence MIT.

La documentation publique est rédigée
en français.
