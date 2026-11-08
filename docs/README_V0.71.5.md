# Clair V0.71.5

L'autocomplétion de Clair Studio connaît maintenant les modules au moment de les importer.

## Ce qui est proposé

| tu tapes | Clair Studio propose |
|---|---|
| `import ` | tous les modules de Clair (`math`, `phys`, `astro`, `os`, `web`, `plot`, `gui`, `nbody`), chacun avec ce qu'il fait, puis les fichiers `.cl` du dossier |
| `import nb` | `nbody` (gravité, N corps) |
| `import ou` | `outils` (le fichier `outils.cl` du dossier) |
| `from nbody import Sim` | `Simulation` d'abord (mêmes majuscules), puis `simulation`, `simulation_solar` |
| `from astro import UA, GM_m` | `GM_mercure`, `GM_mars`, avec leur unité |
| `phys.k` (après `import phys`) | `k_B`, en `J/K` |

- Toucher une proposition l'écrit. Après `from … import`, une fonction est écrite sans `()`.
- Les constantes de `phys` (CODATA) et d'`astro` (JPL, UAI) sont maintenant proposées avec leur unité,
  après `phys.`, `astro.` ou `from … import`.
- Les fichiers `.cl` du dossier sont relus au plus toutes les 5 secondes : un fichier que tu viens de créer
  apparaît tout seul. Le fichier ouvert lui-même n'est pas proposé.
- Parmi les propositions qui commencent par ce qui est tapé, celles qui ont les mêmes majuscules
  passent devant.

## Les modules à toi

Ils existent depuis la V0.47. Avec deux fichiers dans le même dossier :

```
// a.cl
def double(x: int) -> int {
    return 2 * x
}

Class Point {
    x: float
    y: float
}

def main() -> int {          // seul, a.cl se lance ; importé, ce main est laissé de côté
    print(double(4))
    return 0
}
```

```
// b.cl
import a
from a import double as fois_deux

def main() -> int {
    print(a.double(21))                     // 42
    print(fois_deux(5))                     // 10
    let p: Point = a.Point(1.0, 2.0)        // une classe garde son nom : Point ou a.Point
    print(p.x + p.y)                        // 3
    return 0
}
```

- `import a` lit `a.cl` dans le dossier du programme. On écrit le nom sans `.cl`.
- Le nom du fichier doit être un nom de Clair : des lettres sans accent, des chiffres, `_` (par
  exemple `outils.cl` ou `fichier_a.cl`, pas `fichier a.cl`).
- Pas de variable au niveau du fichier (pas de `let` hors d'une fonction) : un module donne des
  fonctions et des classes.
- Dans le Studio, ▶ sur `b.cl` recompile aussi quand `a.cl` a changé. Après `a.`, l'autocomplétion
  propose `double` et `Point`.

## Vérifié

Dans Chromium, sur un dossier avec plusieurs fichiers : tous les cas du tableau, et `b.cl` lancé
par ▶, qui affiche `42`, `10` et `3`.

Pour avoir la nouvelle autocomplétion, relance le Studio (`sh ~/clair/tools/studio/studio.sh`), puis
recharge sa page.
