# Clair V0.70

`nbody` passe aux **milliers d'étoiles**, avec deux ajouts :

- l'**arbre de Barnes et Hut** (`s.gravity = "tree"`), calculé sur tous les cœurs ;
- les **sphères de Plummer**, des amas d'étoiles en équilibre.

Chaque mise à jour envoie aussi un **journal** sur GitHub : je peux voir moi-même si tout marche sur
le téléphone.

```
import nbody

def main() -> int {
    let s: Simulation = nbody.simulation(1.0)
    s.add_plummer(5000, 1.0, 0.5, 1, -3.0, 0.0, 0.0, 0.25, 0.1, 0.0, "bleu")    // deux amas qui se rencontrent
    s.add_plummer(5000, 1.0, 0.5, 2, 3.0, 0.0, 0.0, -0.25, -0.1, 0.0, "rouge")
    s.gravity = "tree"
    s.soft = 0.02
    s.show()
    return 0
}
```

## L'arbre de Barnes et Hut

```
s.gravity = "tree"      // "direct" par défaut : toutes les paires
s.theta = 0.5           // l'angle d'ouverture : plus petit = plus précis et plus lent
s.threads = 0           // les cœurs employés : 0 = tous
```

Calculer toutes les paires coûte N² : pour 100 000 étoiles, cela fait 5 milliards de paires à chaque
pas. L'arbre range les corps dans des cubes coupés en 8, encore et encore. Pour la force sur un corps,
un cube lointain compte comme une seule masse, posée en son centre de masse et corrigée par son
quadrupôle (sa forme). Le coût devient N log N.

- Le critère d'ouverture : un cube est regardé de plus près quand le corps est plus près que
  côté / θ + la distance entre le milieu du cube et son centre de masse (Barnes 1994). Ce petit ajout
  évite les plus grosses erreurs.
- Plusieurs cœurs : chaque corps est calculé seul, dans un ordre fixe. Le résultat est donc **le même au
  dernier chiffre** avec 1 cœur ou avec 8.
- L'arbre va avec le saute-mouton (par défaut) ou Yoshida. IAS15 et WHFast demandent des forces
  exactes ; avec eux, Clair refuse l'arbre et dit pourquoi.
- Les forces de l'arbre ne sont pas exactement opposées deux à deux. La quantité de mouvement bouge
  donc un peu (1e-6 sur 800 étoiles en une unité de temps), comme dans tous les codes à arbre.
  L'énergie, elle, est gardée à environ 1e-5.
- Les collisions (`s.merge`) cherchent leurs voisins dans l'arbre au lieu de tester toutes les paires.
- Sans `s.dt`, le pas vaut un deux-centième du temps de traversée de l'amas : √(R³ / G M), avec R le
  rayon quadratique moyen.

Source : J. Barnes et P. Hut, « A hierarchical O(N log N) force-calculation algorithm », Nature 324
(1986) 446 ; J. Barnes, « Treecode guide » (1994), pour le critère d'ouverture. Le code est écrit pour
Clair.

## Vérifié

**Forces**, sur une sphère de Plummer de 10 000 étoiles, adoucissement 0,01. La référence est le calcul
direct en `long double`. Les colonnes donnent l'erreur relative des forces :

| | médiane | 99 % | la plus grande | temps, 1 cœur |
|---|---|---|---|---|
| Clair, θ = 0,5 | 3,5e-4 | 1,8e-3 | 1,5e-2 | 0,16 s |
| REBOUND 5.2.2, θ = 0,5, avec quadrupôles | 4,9e-4 | 2,5e-3 | 1,2e-2 | 0,21 s |
| REBOUND 5.2.2, θ = 0,5, sans (réglage par défaut) | 1,7e-3 | 1,1e-2 | 6,3e-2 | 0,16 s |
| Clair, θ = 0,3 | 5,5e-5 | 2,3e-4 | 1,1e-3 | 0,37 s |
| Clair, θ = 0,7 | 1,2e-3 | 6,6e-3 | 4,1e-2 | 0,10 s |

Avec 2 cœurs, Clair met 0,08 s pour θ = 0,5. Le calcul direct de toutes les paires prend environ 0,3 s (un cœur) pour 10 000 étoiles. Pour 100 000, il prendrait 30 s, contre moins de 3 s avec l'arbre.

**Dynamique**, sur une sphère de Plummer de 3 000 étoiles :
- après une unité de temps, l'arbre reste à 3,5e-5 en moyenne du calcul direct ;
- sur 10 unités de temps, le rapport du viriel 2K/|W| reste entre 0,98 et 1,01 ;
- les rayons de Lagrange (10 %, 50 %, 90 % de la masse) ne bougent pas plus que le bruit.

**Contrôles automatiques** : ASan, UBSan et ThreadSanitizer ne trouvent rien. Le partage du travail
entre les cœurs n'a pas de course aux données.

Vitesse (2 cœurs de cette machine) : 10 000 étoiles, 0,08 s par pas ; 100 000 étoiles, environ 2,7 s
par pas. Le téléphone a souvent 8 cœurs. Sa vitesse est mesurée à chaque mise à jour par
`tools/bench/nbody_vitesse.cl` et va dans le journal.

## Les sphères de Plummer

```
s.add_plummer(n, masse, rayon, graine, x, y, z, vx, vy, vz, couleur)
```

Cette fonction ajoute n étoiles de même masse, en équilibre, autour de (x, y, z), le tout se déplaçant
à (vx, vy, vz). Elle donne le numéro de la première étoile.

- Le `rayon` est le rayon de Plummer *a* : la moitié de la masse est à 1,305 *a*.
- La même `graine` donne le même amas, partout.
- La `couleur` est `""` ou une couleur (`"bleu"`, `"#ffcc00"`).

La méthode est celle d'Aarseth, Hénon et Wielen (1974), A&A 37, 183. Le rayon vient de la masse
intérieure, la vitesse est tirée par le rejet de von Neumann. Aucune étoile n'est plus loin que 22 *a*.
Les nombres aléatoires viennent de SplitMix64.

Deux mesures pour vérifier un amas :
- `s.virial()` : 2K / |W|, qui vaut 1 à l'équilibre (W est calculé par le calcul direct) ;
- `s.lagrange_radius(f)` : le rayon qui contient la fraction f de la masse.

## La fenêtre avec beaucoup d'étoiles

- Au-delà de 300 corps, chacun est un point.
- La vue s'ajuste sur 95 % des corps : quelques étoiles éjectées ne rapetissent plus l'amas.
- L'énergie affichée est calculée avec l'arbre, et seulement toutes les 30 images au-delà de 2 000
  corps.
- Rien n'est dessiné sous les boutons.

## Le journal

À chaque mise à jour, la commande envoie un compte rendu dans la branche `journal` de GitHub (dossier
`journal/`), **même en cas d'échec**. La branche `main` n'est pas touchée. Le compte rendu contient :

- l'état et les tests réussis ou ratés ;
- la fin des messages d'erreur ;
- la vitesse de `nbody` ;
- le processeur, le nombre de cœurs, Android et clang.

Il ne contient rien de personnel : pas de nom, pas de modèle de téléphone, pas de chemin.

Pour envoyer la sortie de n'importe quel programme :

```
./mon_programme 2>&1 | sh ~/clair/tools/journal.sh "mon essai"
```

## Tests

`tests/nbody_tree.cl` :
- sphère de Plummer : équilibre, rayon de demi-masse, même graine ;
- arbre contre calcul direct ;
- énergie ;
- un cœur ou tous, au dernier chiffre ;
- Yoshida avec l'arbre ;
- collisions trouvées par l'arbre, les mêmes qu'avec toutes les paires ;
- quantité de mouvement ;
- deux amas sauvés et relus.

`tests/nbody_ias15_fenetre.cl` montre maintenant aussi un amas de 2 000 étoiles avec l'arbre.

Le saute-mouton garde les forces de la fin d'un appel pour le suivant quand rien n'a bougé : cette
vérification se fait au dernier bit, et le résultat est le même que si on les recalculait. Les tests
de la V0.66 donnent donc les mêmes sorties.
