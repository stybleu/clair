# Clair V0.71.4

L'éditeur de Clair Studio sait **rechercher et remplacer**, comme VS Code.

## Ouvrir

- La touche **🔍** dans la barre des touches, à côté de Copier et Coller.
- Avec un clavier : **Ctrl+F** pour rechercher, **Ctrl+H** pour rechercher et remplacer.
- Si du texte est sélectionné (sur une seule ligne), il est déjà dans la case.

## Rechercher

- Tous les endroits trouvés sont surlignés en orange clair. Celui où l'on est est surligné plus fort, et
  la vue le suit, en hauteur comme en largeur.
- Le compteur montre où l'on en est : `3 / 12`, ou `aucun`.
- **↓** ou Entrée : l'endroit suivant. **↑** ou Maj+Entrée : le précédent. On repart du début après le
  dernier.
- Trois options, comme dans VS Code :
  - **Aa** : respecter les majuscules (sinon, `Total` trouve aussi `total`) ;
  - **Mot** : seulement les mots entiers (`total` ne trouve plus `total_x`) ;
  - **.\*** : une expression régulière (`t (<|>) ([0-9.]+)`). Si elle est fausse, la case devient rouge
    et le compteur dit `invalide`.
- La recherche commence là où était le curseur, et se met à jour pendant que tu modifies le code.
- **✕** ou Échap ferme la recherche. Le curseur revient alors sur l'endroit où l'on était.

## Remplacer

Le bouton **Remplacer** de la deuxième ligne ouvre la case « Remplacer par ».

- **Remplacer** (ou Entrée dans cette case) : remplace l'endroit actuel, puis passe au suivant.
- **Tout** : remplace partout. Le compteur dit combien d'endroits ont été remplacés (`7 remplacés`).
- Avec **.\***, le remplacement peut reprendre les morceaux trouvés : `$1`, `$2`… (les groupes entre
  parenthèses), `$&` (tout ce qui a été trouvé), `$$` (un `$`). Par exemple, chercher `t (<|>) ([0-9.]+)`
  et remplacer par `t $1= $2` change `t > 1.0` en `t >= 1.0`.
- **On peut revenir en arrière** avec ↶ : un retour par remplacement, et un seul pour Tout.

Une recherche reste dans une ligne : elle ne trouve pas un texte coupé par un retour à la ligne. Au plus
20 000 endroits sont trouvés (le compteur affiche alors `+`).

## Vérifié

Avec Chromium (le navigateur de Chrome), dans les thèmes clair et sombre, sur un fichier qui contient des
mots-clés, des chaînes, un commentaire, `<&>` et des lettres accentuées :

| action | résultat |
|---|---|
| Ctrl+F, puis `total` | 9 trouvés (avec `total_x`) |
| option Mot | 8 |
| Entrée, puis Maj+Entrée | 1 / 8, puis 8 / 8 |
| Remplacer par `somme` | 7 restent, 1 `somme` |
| Tout | `7 remplacés`, plus aucun `total` entier ; `total_x` est gardé |
| ↶ une fois, puis deux fois | 7 `total`, puis le texte de départ |
| `.*` : `t (<|>) ([0-9.]+)` → `t $1= $2` | `t >= 1.0` et `t <= 9.0` |
| `(` avec `.*` | `invalide`, case rouge |
| Échap | fermé, plus rien de surligné |
| la touche 🔍 | ouverte, `1 / 3` pour `valeurs` |

À chaque étape, le texte dessiné reste identique, caractère par caractère, au texte du fichier.

Cette version contient aussi la V0.71.2 (la console rapide) et la V0.71.3 (le texte sélectionné surligné
partout).

Pour avoir le nouvel éditeur, relance le Studio (`sh ~/clair/tools/studio/studio.sh`), puis recharge sa
page.
