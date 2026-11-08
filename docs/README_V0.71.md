# Clair V0.71

Le module `nbody` gagne **MERCURIUS**, l'intégrateur hybride de REBOUND. Tant que les planètes restent loin
les unes des autres, il va aussi vite que WHFast. Quand deux corps se frôlent, il est aussi précis qu'IAS15.

```
import nbody

def main() -> int {
    let s: Simulation = nbody.simulation_solar()       // UA, années, masses du Soleil
    s.integrator = "mercurius"
    s.dt = 0.02                                        // une semaine
    s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
    s.add_orbit("Jupiter", 0.000954791915, 0, 5.2, 0.0, 0.0, 0.0, 0.0, 175.0)
    s.add_orbit("comète", 0.0, 0, 3.5, 0.5, 0.0, 0.0, 0.0, 180.0)
    s.run(3.0)                                         // la comète passe à 0,027 UA de Jupiter
    print(s.semi_major_axis(2, 0))                     // 4,53 : Jupiter l'a déviée (3,5 au départ)
    s.show()
    return 0
}
```

## Quand le prendre

| | pour quoi | le pas |
|---|---|---|
| `"whfast"` | des planètes qui ne se rapprochent jamais, sur des millions d'années | fixe |
| `"ias15"` | quelques corps, des rencontres aussi proches qu'on veut, le plus précis | choisi seul |
| `"mercurius"` | beaucoup de planètes ou de petits corps, avec des rencontres de temps en temps | fixe |

Avec WHFast, la comète de l'exemple finit à 0,28 UA de sa vraie place. Avec MERCURIUS, elle n'en est qu'à
0,00001 UA.

## Comment il marche

Le premier corps ajouté est l'étoile. Les places des autres corps sont comptées depuis l'étoile, et leurs
vitesses depuis le centre de masse (coordonnées héliocentriques démocratiques). Chaque pas comporte :

1. une demi-poussée : les planètes s'attirent entre elles ;
2. un demi-« saut » : l'étoile bouge sous la quantité de mouvement des planètes ;
3. pour chaque planète, son orbite de Kepler autour de l'étoile ;
4. un demi-saut ;
5. une demi-poussée.

L'attraction entre deux planètes est coupée en deux parts par une fonction L de leur distance, celle de
Mercury (Chambers 1999) : L = 10 y³ − 15 y⁴ + 6 y⁵.

- Loin l'une de l'autre (L = 1), toute l'attraction passe dans les poussées, comme avec WHFast.
- Plus près que la distance critique, une partie de l'attraction suit les orbites.

La distance critique est la plus grande de ces valeurs :

- `s.hill` rayons de Hill (3 par défaut) ;
- la distance parcourue en 0,4 pas ;
- deux fois la taille du corps.

À chaque pas, Clair cherche les paires qui passent sous 1,1 distance critique. Pour cela, il part de leurs
places et de leurs vitesses au début et à la fin du pas. La distance au carré est un polynôme de degré 3 du
temps, et son minimum se calcule directement. Les corps de ces paires refont le pas avec IAS15. IAS15
compte alors l'attraction de l'étoile et la part de l'attraction entre planètes que les poussées n'ont pas
prise.

- **Le pas est fixe** : `s.dt`, comme pour WHFast. Sans `s.dt`, Clair prend un quarantième de l'orbite la
  plus courte.
- **Les collisions** (`s.merge = true`) sont cherchées après chaque pas d'IAS15 pendant les rencontres.
  Deux planètes qui se touchent n'en font plus qu'une. Un corps qui touche l'étoile (si elle a un rayon)
  tombe dedans, et l'étoile prend sa masse.
- **Les distances critiques** sont calculées au premier pas, puis gardées dans `s.crit`, comme le fait
  REBOUND. Elles sont recalculées quand on ajoute des corps, ou quand `s.dt`, G ou `s.hill` changent.
  Chaque simulation a les siennes : deux simulations menées ensemble, pas à pas, donnent les mêmes
  nombres que si chacune avait tourné seule.
- Juste après `s.run()` :
  - `s.steps_done(3)` donne le nombre de pas qui ont eu une rencontre ;
  - `s.steps_done(4)` donne le nombre de pas d'IAS15 faits pendant ces rencontres.
- La fenêtre (`s.show()`) marche avec MERCURIUS.
- L'arbre (`s.gravity = "tree"`) ne va pas avec MERCURIUS, qui demande les forces exactes.

Sources :

- J. E. Chambers, « A hybrid symplectic integrator that permits close encounters between massive bodies »,
  MNRAS 304 (1999) 793 ;
- H. Rein, D. M. Hernandez, D. Tamayo et al., « Hybrid symplectic integrators for planetary dynamics »,
  MNRAS 485 (2019) 5490.

Le code est écrit pour Clair à partir de ces articles. REBOUND (licence GPL) sert de référence pour les
résultats, et rien n'en est recopié.

## Vérifié contre REBOUND

REBOUND 5.2.2 est compilé à part, avec MERCURIUS et ses réglages par défaut. Les deux programmes partent des
mêmes conditions de départ.

| Essai | Clair | REBOUND |
|---|---|---|
| Système solaire, 1 000 ans, pas de 0,005 an : écart relatif de l'énergie | 6,149e-10 | 6,150e-10 |
| écart entre les places des deux | 8e-9 UA au plus (Vénus) | |
| temps (cette machine) | 0,34 s | 0,39 s (0,33 s sans resynchroniser à chaque pas) |
| Comète qui frôle Jupiter, pas de 0,02 an, 3 ans : écart à IAS15 | 9,81e-6 UA | 9,81e-6 UA |
| écart entre les places des deux | 6e-14 UA | |
| Trois planètes serrées, 100 premiers pas | les mêmes à 2e-15 près | |
| pas d'IAS15 pendant les 1 326 premiers pas | 12 352 | 12 354 |
| 20 systèmes de trois planètes serrées, 26 525 pas chacun : écart médian de l'énergie | 1,8e-8 | 1,7e-8 |
| temps pour les 20 | 3,7 s | 5,2 s |

**La comète** passe à 0,027 UA de Jupiter, moins d'un dixième de son rayon de Hill. Clair et REBOUND la placent
au même endroit à 6e-14 UA près. L'écart à IAS15 est l'erreur de la méthode elle-même : il est divisé par
4 quand le pas est divisé par 2 (méthode d'ordre 2). WHFast, lui, se trompe de beaucoup :

| pas | MERCURIUS : écart à IAS15 | WHFast : écart à IAS15 |
|---|---|---|
| 0,05 an | 6,1e-5 UA | 0,21 UA |
| 0,02 an | 9,8e-6 UA | 0,28 UA |
| 0,01 an | 2,5e-6 UA | 0,093 UA |
| 0,005 an | 6,2e-7 UA | 0,024 UA |

**Les trois planètes serrées** sont placées à 0,1 UA l'une de l'autre, soit moins d'un rayon de Hill : elles
se rencontrent à chaque tour. Ce système est chaotique, et deux calculs qui diffèrent au dernier chiffre se
séparent vite. REBOUND contre lui-même le montre : en changeant seulement le moment où il remet ses
coordonnées en place (ce qui ne change que le dernier chiffre), ses deux calculs s'écartent de 1e-12
après 124 pas, puis de 1e-6 après 1 326 pas. Clair et REBOUND s'écartent de la même façon. On les compare
donc sur les premiers pas, où ils sont pareils au dernier chiffre près, puis en moyenne sur 20 systèmes :
les erreurs sont les mêmes.

## Sauver et reprendre

`s.save()` garde aussi les distances critiques : c'est le 10e nombre de chaque corps dans le fichier. Une
simulation relue par `nbody.load()` continue donc exactement comme si elle ne s'était pas arrêtée, même
dans un autre programme. Les fichiers des versions d'avant se lisent toujours. Sans ce 10e nombre, les
distances sont recalculées.

## Plus rapide aussi : WHFast

La série qui donne les fonctions de Stumpff (dans l'équation de Kepler) est maintenant calculée avec des
coefficients préparés à l'avance, par la méthode de Horner. Avant, chaque terme demandait deux divisions.
Résultat :

- l'équation de Kepler demande 2,4 fois moins d'instructions ;
- WHFast fait 1 000 ans de système solaire en 0,25 s au lieu de 0,35 s, sur cette machine ;
- Mercure finit à 2,6e-10 UA du calcul fait avec des nombres de 80 bits, au lieu de 6,8e-10 UA.

## Corrigé

La fenêtre, quand le calcul tourne dans son propre fil, lisait le nombre de corps dans les listes de la
simulation. Or MERCURIUS remplace ces listes pendant une rencontre. ThreadSanitizer a trouvé ce problème.
La fenêtre prend maintenant ce nombre dans sa propre copie.

## Tests

`tests/nbody_mercurius.cl` vérifie :

- le système solaire sur 1 000 ans : aucune rencontre, l'énergie et les places comme REBOUND ;
- la comète qui frôle Jupiter : comme REBOUND à 1e-10 UA, à moins de 2e-5 UA d'IAS15, l'ordre 2, et
  WHFast qui se trompe de plus de 0,1 UA ;
- les trois planètes serrées : comme REBOUND pendant 100 pas, et l'énergie ;
- sauver et relire : la suite est la même au dernier chiffre ;
- deux simulations menées ensemble ;
- les collisions entre planètes, et avec l'étoile : la masse et la quantité de mouvement sont gardées.

`tests/nbody_fil.cl` fait aussi tourner la comète dans la fenêtre, avec le calcul dans un fil à part.

ASan, UBSan et ThreadSanitizer ne trouvent rien.

`tools/bench/nbody_vitesse.cl` mesure aussi MERCURIUS, et le résultat va dans le journal.

## Ce qui manque encore par rapport à REBOUND

Prévu ensuite :

- **V0.72** : les équations variationnelles et le **MEGNO**, pour mesurer le chaos ;
- **V0.73** : un module `fluide` (méthode de Boltzmann sur réseau), pour voir des écoulements.

Pas prévu pour l'instant : TRACE (l'autre intégrateur hybride de REBOUND) et les forces en plus.
