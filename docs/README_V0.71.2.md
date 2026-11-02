# Clair V0.71.2

La console de Clair Studio suit un programme qui écrit beaucoup sans ralentir.

## Mesuré avant

Mesures faites avec Chromium (le navigateur de Chrome), sur la machine de test. Un téléphone est plusieurs
fois plus lent.

| | temps | travail de la page | éléments dans la console |
|---|---|---|---|
| un programme qui écrit 100 000 lignes | 18 s | 15,4 s | 100 034 |
| « bonjour » | 0,36 s | 0,04 s | 1 |
| un programme `nbody`, premier ▶ (compilation comprise) | 1,6 s | 0,03 s | 1 |
| le même, ▶ encore (déjà compilé) | 0,34 s | 0,03 s | 1 |
| écrire dans un petit fichier | 3 ms par touche | | |
| écrire dans un fichier de 8 000 lignes | 21 ms par touche | | |

Seul, sans le Studio, le programme de 100 000 lignes s'exécute en 0,03 s sur la même machine. Dans le
Studio, trois choses le ralentissaient :

1. **Chaque ligne était un élément de la page.** Après 100 000 lignes, la page en avait 100 000, et chaque
   nouveau morceau de sortie obligeait le navigateur à tout remettre en page.
2. **Le serveur ne lisait que 64 Ko à chaque appel.** Le programme, bloqué, attendait que la page revienne
   chercher la suite.
3. **La page attendait 150 ms entre deux appels**, même quand la sortie arrivait sans arrêt.

## Corrigé

1. **La console montre les 5 000 dernières lignes.** Au-dessus, une ligne indique combien de lignes ne sont
   plus affichées. Le bouton **Copier garde toute la sortie**, y compris les lignes cachées. Les lignes
   arrivent par paquets, en un seul élément quand elles ne contiennent pas d'erreur. Les erreurs restent en
   rouge, avec le lien vers leur ligne, et les figures de `plot` s'affichent comme avant.
2. **Le serveur lit tant qu'il y a de la sortie**, jusqu'à 1 Mo ou 60 ms par appel (`/api/suivre`).
3. **La page revient après 20 ms** quand le programme écrit, et après 150 ms quand il n'écrit rien.

## Mesuré après

| | avant | après |
|---|---|---|
| 100 000 lignes : temps | 18 s | 0,57 s |
| travail de la page | 15,4 s | 0,44 s |
| éléments dans la console | 100 034 | 3 |
| Copier | 100 002 lignes | 100 002 lignes |

Les programmes courts ne changent pas : « bonjour » s'exécute en 0,34 s.

## Ce qui reste

- **La compilation d'un programme `nbody` prend environ 1,4 s** sur la machine de test, à chaque fois que
  le programme a changé. Ce temps vient de clang, qui recompile à chaque fois les 3 500 lignes du moteur de
  `nbody` (IAS15, WHFast, MERCURIUS, l'arbre, la fenêtre). Un programme sans module se compile en 0,1 s,
  et un programme inchangé ne se recompile pas. Pour gagner ce temps, il faudrait compiler le moteur une
  seule fois et le garder. Ce changement touche le compilateur en profondeur : c'est une version à part.
- **L'éditeur** ne dessine que les lignes visibles. Dans un fichier de 8 000 lignes, la frappe coûte
  quand même 21 ms par touche, surtout dans le champ de texte du navigateur lui-même. Les programmes
  habituels, de quelques centaines de lignes, ne sont pas concernés.

## Tests

`tests/run_studio.sh` lance un programme qui écrit 100 000 lignes et vérifie deux choses :

- toutes les lignes arrivent ;
- il faut au plus 12 appels à `/api/suivre`. Le test en fait 4, contre 22 avant la correction.

La console a été vérifiée dans Chromium : 100 000 lignes, une erreur avec son lien, une figure et Copier.

Pour avoir la nouvelle console, relance le Studio (`sh ~/clair/tools/studio/studio.sh`, il se recompile tout seul), puis recharge sa page.
