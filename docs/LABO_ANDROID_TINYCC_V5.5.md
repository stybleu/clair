# Laboratoire Android TinyCC V5.5

## Objectif

Expérience de compilation C native directement dans
une application Android ARM64.

Le langage Clair conserve Clang/LLVM comme backend
principal. TinyCC est utilisé ici uniquement pour
valider la compilation embarquée.

Le laboratoire reste séparé de Clair Studio.

## Résultats validés

Test réalisé sur Android ARM64 le 5 novembre 2026.

- Chargement du compilateur : OK
- Compilation C dans l'APK : OK
- Relocalisation en mémoire : OK
- Exécution du code généré : OK
- Détection des erreurs de syntaxe : OK
- Copie du journal : OK

Programme testé :

```c
int clair_calcul(int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        s += i % 97;
    return s;
}
```

Résultats :

```text
COMPILATION_DANS_APK=OK
COMPILATEUR=TINYCC_ARM64
LLVM_DANS_APK=NON
ENTREE=2500000
RESULTAT=119999259
COMPILATION_MS=2.859
EXECUTION_MS=13.881
CHARGEMENT_MEMOIRE=OK
```

Les durées proviennent d'un seul essai.
La durée de compilation comprend la relocalisation.

Test d'erreur :

```text
COMPILATION_DANS_APK=ECHEC
DIAGNOSTIC=<string>:1: error: identifier expected
```

## Organisation

Les sources sont dans :

`tools/studio/laboratoires/tinycc_v55/`

Fichiers :

- `construire.sh` : construction de l'APK
- `pont.c` : liaison JNI avec TinyCC
- `Probe.java` : interface Android et journal
- `AndroidManifest.xml` : configuration Android

## Construction

Prérequis : Termux ARM64, Clang, Git, Make, Python,
outils Java et Android (`javac`, `d8`, `aapt`,
`aapt2`, `apksigner`, `keytool`) et paquet `tcc`.

La bibliothèque TinyCC doit être reconstruite
en code indépendant de la position (`-fPIC`).

Révision testée :

`6a24b762d3e1086dcffd002c68cb5ca3a33a5c6d`

Préparation :

```bash
mkdir -p ~/clair_labs

git clone --branch mob \
    https://github.com/TinyCC/tinycc.git \
    ~/clair_labs/tinycc_source_v5

cd ~/clair_labs/tinycc_source_v5

git checkout --detach \
    6a24b762d3e1086dcffd002c68cb5ca3a33a5c6d

./configure \
    --cc=clang \
    --cpu=arm64 \
    --prefix="$PREFIX" \
    --disable-static \
    --disable-rpath \
    --extra-cflags="-O2 -fPIC" \
    --sysincludepaths="$PREFIX/include:$PREFIX/lib/tcc/include" \
    --libpaths="$PREFIX/lib:/system/lib64"

make -j2 libtcc.so
```

Le script de construction attend :

- Clair dans `~/clair`
- `libtcc.so` dans `~/clair_labs/tinycc_source_v5/`
- `libtcc1.a` dans `$PREFIX/lib/tcc/`

Lancement :

```bash
bash ~/clair/tools/studio/laboratoires/tinycc_v55/construire.sh
```

L'APK générée se trouve dans Téléchargements :

`clair-labo-v55.apk`

## Limitations

- TinyCC n'est pas LLVM.
- La compilation directe des fichiers `.cl`
  n'est pas encore intégrée.
- La compilation et l'exécution sont effectuées
  dans le processus Android de l'application.
- Un programme C incorrect peut provoquer
  le plantage du laboratoire.
- Le code non fiable ne doit pas être exécuté.
- Les performances sur d'autres téléphones
  restent à évaluer.

Les fichiers APK, bibliothèques binaires et
clés de signature ne sont pas publiés.

## Prochaine étape

Relier la génération C du compilateur Clair
au moteur de compilation embarqué, dans un
laboratoire séparé de Clair Studio.
