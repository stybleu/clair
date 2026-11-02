# Clair V0.71.3

Dans l'éditeur de Clair Studio, le texte sélectionné est **surligné partout où il revient** dans le
fichier, comme dans VS Code.

- **Un mot entier**, sélectionné par un double toucher ou un appui long : seuls les mêmes mots entiers
  sont surlignés. Sélectionner `total` surligne `total`, mais ni `total_x` ni `totaux`.
- **Un autre morceau d'une ligne**, de 2 caractères au moins (une partie d'un mot, `t >`, `.x`...) : tous
  les endroits où ce texte apparaît sont surlignés.
- La recherche respecte les majuscules et les lettres accentuées.
- Le surlignage disparaît dès que la sélection disparaît, par exemple quand tu tapes.
- Il marche dans les deux thèmes, clair et sombre, avec la couleur d'accent du Studio.

## Comment

Seules les lignes visibles sont dessinées : c'est déjà le cas depuis la V0.59. Pour chacune, `studio.js`
cherche le texte sélectionné dans la ligne, puis l'entoure d'un `<mark class="meme">` dans la ligne déjà
coloriée.

- Quand une marque traverse une couleur (un mot-clé, une chaîne), elle est fermée avant la balise, puis
  rouverte après.
- `&amp;`, `&lt;` et `&gt;` comptent pour un seul caractère.
- La marque n'a ni marge ni bordure (le cadre est une ombre) : le texte coloré reste exactement sous le
  texte que l'on tape.

## Vérifié

Avec Chromium, sur un fichier qui contient des mots-clés, des chaînes, un commentaire, `<&>` et des
lettres accentuées :

| sélection | surlignés |
|---|---|
| le mot `total` | 8 (et non `total_x` ni `totaux`) |
| le morceau `tot` | 10 |
| `été` | 2 |
| `t >` (deux couleurs) | 1 |
| rien | 0 |

Dans tous les cas, le texte dessiné reste identique, caractère par caractère, au texte du fichier.
L'image est juste dans les deux thèmes.

Cette version contient aussi la V0.71.2 (la console rapide) pour qui ne l'a pas encore.

Pour avoir le nouvel éditeur, recharge la page du Studio.
