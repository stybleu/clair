// V0.63 : de l'air chaud et de l'air froid emportes par le vent, qui se melangent peu a peu.
// La temperature est sur une grille de nx x ny cases (comme dans un modele meteo, en beaucoup plus petit).
// A chaque pas de temps, chaque case echange de la chaleur avec ses voisines : ce que le vent apporte
// (advection) et ce qui s'etale tout seul (diffusion). Le domaine se referme sur lui-meme (ce qui sort
// a droite revient a gauche), donc la chaleur totale ne change pas : un des tests le verifie.
// clair --test chaleur.cl -o t && ./t
import plot
import math

Class Grille {
    nx: int
    ny: int
    t: list[float]      // la temperature de la case (i, j) : t[j * nx + i]
    u: list[float]      // le vent d'ouest en est sur la face gauche de chaque case
    v: list[float]      // le vent du sud au nord sur la face du bas de chaque case
}

// un vent fait de tourbillons, tire d'une "fonction de courant" : il ne cree ni ne detruit d'air
def courant(x: float, y: float, nx: int, ny: int) -> float {
    return 3.0 * math.sin(2.0 * math.pi * x / float(nx)) * math.sin(2.0 * math.pi * y / float(ny))
}

def creer(nx: int, ny: int) -> Grille {
    let g: Grille = Grille(nx, ny, [], [], [])
    for j in range(0, ny) {
        for i in range(0, nx) {
            // une bulle chaude et une bulle froide dans de l'air a 10 degres
            let dc: float = (float(i) - 16.0) * (float(i) - 16.0) + (float(j) - 20.0) * (float(j) - 20.0)
            let df: float = (float(i) - 48.0) * (float(i) - 48.0) + (float(j) - 12.0) * (float(j) - 12.0)
            g.t.append(10.0 + 15.0 * math.exp(-dc / 40.0) - 10.0 * math.exp(-df / 40.0))
            // les vents des faces, par differences de la fonction de courant aux coins
            g.u.append(courant(float(i), float(j) + 1.0, nx, ny) - courant(float(i), float(j), nx, ny))
            g.v.append(courant(float(i), float(j), nx, ny) - courant(float(i) + 1.0, float(j), nx, ny))
        }
    }
    return g
}

// le plus grand pas de temps sur : le vent ne traverse pas plus d'une case, la diffusion reste stable
def pas_de_temps(g: Grille, kappa: float) -> float {
    let vmax: float = 0.0
    for k in range(0, len(g.u)) {
        vmax = max(vmax, abs(g.u[k]) + abs(g.v[k]))
    }
    return 0.9 / (2.0 * vmax + 4.0 * kappa)
}

// un pas : flux par les faces (on prend la temperature de la case d'ou vient le vent), puis diffusion
def avancer(g: Grille, dt: float, kappa: float) -> int {
    let nx: int = g.nx
    let ny: int = g.ny
    let neuf: list[float] = []
    for j in range(0, ny) {
        for i in range(0, nx) {
            let k: int = j * nx + i
            let gauche: int = j * nx + (i + nx - 1) % nx
            let droite: int = j * nx + (i + 1) % nx
            let bas: int = ((j + ny - 1) % ny) * nx + i
            let haut: int = ((j + 1) % ny) * nx + i
            // ce qui entre par la gauche et par le bas, ce qui sort par la droite et par le haut
            let fg: float = g.u[k] * g.t[gauche]
            if g.u[k] < 0.0 {
                fg = g.u[k] * g.t[k]
            }
            let fd: float = g.u[droite] * g.t[k]
            if g.u[droite] < 0.0 {
                fd = g.u[droite] * g.t[droite]
            }
            let fb: float = g.v[k] * g.t[bas]
            if g.v[k] < 0.0 {
                fb = g.v[k] * g.t[k]
            }
            let fh: float = g.v[haut] * g.t[k]
            if g.v[haut] < 0.0 {
                fh = g.v[haut] * g.t[haut]
            }
            let lap: float = g.t[gauche] + g.t[droite] + g.t[bas] + g.t[haut] - 4.0 * g.t[k]
            neuf.append(g.t[k] + dt * (fg - fd + fb - fh) + dt * kappa * lap)
        }
    }
    g.t = neuf
    return 0
}

def total(g: Grille) -> float {
    let s: float = 0.0
    for x in g.t {
        s += x
    }
    return s
}

def main() -> int {
    let g: Grille = creer(64, 40)
    let kappa: float = 0.05
    let dt: float = pas_de_temps(g, kappa)
    let avant: float = total(g)
    plot.size(640, 440)
    plot.equal()
    plot.colormap("bleu-rouge")
    plot.crange(0.0, 25.0)              // les memes couleurs pour les memes temperatures, d'une image a l'autre
    for image in range(0, 40) {
        plot.title("Chaleur emportée par le vent, t = " + format(float(image * 12) * dt, ".0f"))
        plot.heatmap(g.t, g.nx, g.ny)
        plot.arrows(g.u, g.v, g.nx, g.ny)
        plot.frame()
        for k in range(0, 12) {
            avancer(g, dt, kappa)
        }
    }
    plot.animate(8.0)
    let apres: float = total(g)
    print("Chaleur totale au début : " + format(avant, ".6f"))
    print("Chaleur totale à la fin : " + format(apres, ".6f") + " (elle se conserve)")
    return 0
}

test "la chaleur totale se conserve" {
    let g: Grille = creer(32, 20)
    let dt: float = pas_de_temps(g, 0.05)
    let avant: float = total(g)
    for k in range(0, 200) {
        avancer(g, dt, 0.05)
    }
    assert(abs(total(g) - avant) < 1e-9 * abs(avant), "ecart " + str(total(g) - avant))
}

test "aucune case ne devient plus chaude que la plus chaude au depart" {
    let g: Grille = creer(32, 20)
    let haut: float = -1000.0
    let bas: float = 1000.0
    for x in g.t {
        haut = max(haut, x)
        bas = min(bas, x)
    }
    let dt: float = pas_de_temps(g, 0.05)
    for k in range(0, 200) {
        avancer(g, dt, 0.05)
    }
    for x in g.t {
        assert(x <= haut + 1e-9 and x >= bas - 1e-9, "une temperature de " + str(x) + " hors de [" + str(bas) + ", " + str(haut) + "]")
    }
}

test "le vent ne cree ni ne detruit d'air" {
    let g: Grille = creer(32, 20)
    for j in range(0, g.ny) {
        for i in range(0, g.nx) {
            let k: int = j * g.nx + i
            let sortie: float = g.u[j * g.nx + (i + 1) % g.nx] - g.u[k] + g.v[((j + 1) % g.ny) * g.nx + i] - g.v[k]
            assert(abs(sortie) < 1e-12, "case " + str(i) + ", " + str(j))
        }
    }
}
