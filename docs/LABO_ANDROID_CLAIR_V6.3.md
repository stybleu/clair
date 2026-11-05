# Clair V6.3 — compilation native dans Android

## Objectif

Exécuter du véritable code Clair `.cl` dans
une APK Android autonome, sans Termux pendant
l'exécution.

Chaîne de compilation :

Clair → génération C → TinyCC → ARM64 → exécution.

Le frontend est construit à partir des sources
officielles de Clair.

TinyCC est un backend expérimental. Le compilateur
principal conserve Clang/LLVM.

## Tests validés le 5 novembre 2026

Test sur un appareil Android ARM64.

### Programme simple

```clair
def main() -> int {
    return 42
}
```

Résultat : 42.

### Boucle

```clair
def main() -> int {
    let somme: int = 0

    for i in range(2500000) {
        somme = somme + (i % 97)
    }

    return somme
}
```

Résultat : 119999259.

Mesures d'un essai :

- Génération C : 0,331 ms
- Compilation TinyCC : 1,474 ms
- Exécution ARM64 : 31,828 ms

Ces mesures ne constituent pas un benchmark
comparatif avec Clang/LLVM.

### Erreur de syntaxe

Le programme incorrect est rejeté par le
frontend Clair.

```text
GENERATION_CLAIR=ECHEC
DIAGNOSTIC=parse error at 3:5: expected expression, got return
```

## Éléments techniques

Le laboratoire contient :

- `construire.sh` : construction de l'APK
- `construire_frontend.sh` : compilation du
  frontend officiel en bibliothèque ARM64
- `pont.c` : interface JNI Clair/TinyCC
- `Probe.java` : éditeur Android et journal
- `include/` : en-têtes C minimaux

Le script de construction reprend le laboratoire
local validé. Il suppose les prérequis déjà préparés
dans `~/clair_labs`, notamment :

- `v62_frontend/libclairfront.so`
- `tinycc_source_v5/libtcc.so`
- `v63_headers/`
- `test_compilateur_v55.sh`

La préparation de TinyCC est documentée dans
`docs/LABO_ANDROID_TINYCC_V5.5.md`.

Les sources et le compilateur Clair se trouvent
dans `~/clair`.

Ce laboratoire n'est donc pas encore un
installateur autonome reproductible sur un
téléphone neuf.

## Limites actuelles

- Pas de backend LLVM embarqué.
- En-têtes C très limités.
- Résultat numérique de `main()` affiché.
- Les sorties de `print()` ne sont pas encore
  récupérées dans la console Android.
- Pas d'isolation du code exécuté : un programme
  incorrect peut faire planter l'application.
- Ne pas exécuter de code non fiable.
- Les fonctionnalités avancées de Clair ne sont
  pas encore validées dans cette APK.
- Les programmes doivent être des sources
  Clair syntaxiquement correctes.

Le compilateur principal et Clair Studio
restent inchangés.

## Prochaine étape

Étendre la compatibilité des en-têtes C, récupérer
les sorties de `print()` et améliorer le traitement
des erreurs, avant une intégration éventuelle
dans Clair Studio.
