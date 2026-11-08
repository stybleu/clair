# Clair V0.51

## Des classes aussi rapides qu'en C

```text
Class Vec {
    x: float
    y: float
    z: float

    def plus(o: Vec) -> Vec {
        return Vec(self.x + o.x, self.y + o.y, self.z + o.z)
    }
}

let p: Vec = Vec(0.0, 0.0, 0.0)
for i in range(n) {
    p = p.plus(v)            // V0.50 : un objet de plus en mémoire à chaque tour
}
```

Jusqu'à la V0.50, chaque objet (`Vec(...)`) était rangé en mémoire et y restait jusqu'à la fin du
programme. Une boucle qui fabrique des vecteurs prenait donc de plus en plus de mémoire, et chaque
objet coûtait une réservation de mémoire.

La V0.51 regarde le programme entier et choisit, sans rien demander, pour chaque classe et chaque
objet :

### 1. Les classes valeur

Une classe dont **aucun champ n'est jamais changé** (nulle part `obj.x = ...`, `self.x = ...`,
`obj.x += ...`) devient une structure du C, copiée comme un nombre : rien n'est réservé en mémoire,
et Clang garde les champs dans les registres du processeur.

Le programme donne exactement les mêmes résultats : comme les champs ne changent jamais, personne
ne peut voir la différence entre une copie et le même objet.

```c
static ClairClass_Vec clair_fn_Vec__plus(ClairClass_Vec self, ClairClass_Vec o) {
    return ((ClairClass_Vec){self.x + o.x, self.y + o.y, self.z + o.z});
}
```

Une classe reste un objet ordinaire quand :

- un de ses champs est changé quelque part dans le programme ;
- elle fait plus de 64 octets (plus de 8 champs) : la copier coûterait plus cher que la montrer ;
- elle a un champ `bigint`, `bigfloat` ou `certfloat` : le collecteur de la V0.50 compte ces
  champs, il n'est pas touché ;
- elle se contient elle-même.

Une classe valeur peut contenir une liste (`xs: list[int]`) : la liste reste partagée entre les
copies, comme avant (`sac.met(3)` ajoute dans la même liste).

### 2. Les objets sur la pile

Un objet d'une classe ordinaire, créé par `let c: Compteur = Compteur(0, 5)`, qui **ne sort jamais de
sa fonction** est rangé sur la pile du C : il disparaît tout seul à la fin de son bloc.

```c
ClairClass_Compteur clair_obj_c = {0, 5};
ClairClass_Compteur *c = &clair_obj_c;
```

L'objet ne sort pas quand on s'en sert seulement par `c.n`, `c.n = 3`, `c.avance()`, ou en le
donnant à une fonction qui ne le garde pas. Il reste en mémoire comme avant dès qu'il peut sortir :

- `let d: Compteur = c` ou `c = Compteur(...)` (un deuxième nom) ;
- rangé dans le champ d'un autre objet (`Equipe(c, 3)`, `eq.chef = c`) ;
- rendu par `return`, ou donné à une fonction qui le rend ou le range (Clair suit les appels de
  fonction en fonction, jusqu'au bout).

## Mesures

Mesurées ici, sur un ordinateur (x86-64) ; le rapport de la commande les refait sur le téléphone.

| programme | V0.50 | V0.51 | C écrit à la main |
|---|---|---|---|
| `vecteurs` (20 millions de pas, nouveau jeu du rapport) : temps | 2,39 s | 0,39 s | 0,35 s |
| `vecteurs` : mémoire | 3,6 Go | 12 Mo | 12 Mo |
| vecteurs 3D, 20 millions de tours : temps | 0,625 s | 0,022 s | |
| vecteurs 3D, 20 millions de tours : mémoire | 1,2 Go | 12 Mo | |
| deux corps qui s'attirent (objets changés, sur la pile) | 0,525 s | 0,470 s | |

Sur un téléphone, la V0.50 pouvait être arrêtée faute de mémoire sur ces boucles.

`tools/jeux/vecteurs.cl` : une balle lancée dans l'air (pesanteur, frottement), six vecteurs
fabriqués à chaque pas ; `vecteurs_ref.c` fait le même calcul en C avec des structures. Les deux
écrivent les mêmes chiffres.

## Tests

- `classes_valeur.cl` : copie puis remplacement d'une valeur, une valeur qui contient des valeurs
  et une chaîne, une valeur avec une liste partagée, des objets sur la pile changés directement et
  par une fonction, dans une boucle, un objet de plus de 64 octets, des objets qui sortent (deuxième
  nom, champ d'un autre objet, valeur rendue), un champ `bigint` ; identique à Python, et le C
  généré est vérifié pour chaque cas ;
- jeu `vecteurs` (1000 pas) : même sortie que le C ;
- 1500 programmes tirés au hasard avec huit classes (valeur ou non selon le programme, objets
  imbriqués, deuxièmes noms, remplacements, objets rendus ou rangés dans des champs, fonctions qui
  changent leurs paramètres, boucles et `if` imbriqués, champs `bigint`) : même sortie qu'avec la
  V0.50, et aucune erreur pour AddressSanitizer et UndefinedBehaviorSanitizer ;
- le C des programmes sans classe est identique à la V0.50 ; 17 programmes de test sur 140 changent,
  tous ceux qui ont des classes.
