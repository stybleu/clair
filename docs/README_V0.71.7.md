# Clair V0.71.7

Corrige la construction de l'appli Clair Studio (V0.71.6) sur le téléphone.

## Ce qui s'est passé

La V0.71.6 est passée : elle a été compilée, testée (279 tests) et envoyée. Les outils Android de Termux se
sont installés. La construction de l'appli s'est arrêtée à `d8`, l'outil qui traduit les classes Java pour
Android :

```
Error in .../classes/clair/studio/Studio$1$1.class:
java.lang.NullPointerException: Cannot invoke "String.length()" because "<parameter1>" is null
```

Deux outils du téléphone ne s'entendent pas :

- Le Java de Termux est la version 21. Depuis cette version, `javac` ajoute aux classes anonymes et internes
  un attribut `MethodParameters`. Il décrit les paramètres cachés de leur constructeur, sans leur donner de
  nom.
- Le `d8` de Termux est ancien (R8 3.3). Il s'attend à ce que chaque paramètre ait un nom, et plante sur ces
  paramètres sans nom.

Dans le bac à sable, l'APK avait été construit avec `dx` à la place de `d8`. `dx` ignore cet attribut, donc
le problème ne s'était pas montré.

Le même journal montrait un deuxième problème : sur Android 16, Termux n'a plus le droit d'utiliser
`pm list packages` (« Failed transaction »). C'est avec lui que `fabriquer.sh` regardait si l'appli était
installée.

## Corrigé

- **Plus aucune classe anonyme ni interne** dans `Studio.java`. Les aides sont des classes statiques :
  - `Sonde`, un fil qui interroge le Studio ;
  - `Retour` et `Tache`, ce qui revient sur le fil de l'interface ;
  - `Client`, `Pont`, `Vue` et `Filtre`.

  Une sonde revient avec un numéro d'étape, au lieu d'une fonction à rappeler. Le fonctionnement de l'appli
  ne change pas.
- **`fabriquer.sh` vérifie avant `d8`**, avec `javap`, qu'aucune classe n'a cet attribut. Si une classe
  l'a, il donne son nom au lieu de laisser `d8` planter.
- **L'appli donne elle-même sa version installée.** Un récepteur `Version` répond à
  `am broadcast -n clair.studio/.Version -f 32`. `-f 32` fait répondre l'appli même si elle n'a encore
  jamais été ouverte. Sans l'appli, la réponse n'a pas de version.
- L'appli passe en version 2. `studio.sh` la propose donc de nouveau, même là où la version 1 a déjà été
  proposée.

## Vérifié

- `javap -v` ne trouve aucun `MethodParameters` dans les classes de l'appli. Avec une classe anonyme
  ajoutée exprès, `fabriquer.sh` s'arrête et la nomme.
- L'APK est construit, aligné et signé. `aapt2 dump badging` y lit la version 2 et le récepteur `Version`.
- `fabriquer.sh` a été essayé avec un `pm` qui échoue comme sur Android 16 : l'installation est vue par
  `am broadcast`.
