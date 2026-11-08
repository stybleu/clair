// V0.48 : les unites verifiees a la compilation ; un transfert de Hohmann de la Terre vers Mars
import math as m
import phys
from astro import GM_soleil, GM_terre, R_terre, UA, jour

Class Orbite {
    nom: string
    rayon: float[m]

    def vitesse() -> float[m/s] {
        return m.sqrt(GM_soleil / self.rayon)
    }
}

// la vitesse au perihelie d'une ellipse de demi-grand axe a
def vitesse_ellipse(r: float[m], a: float[m]) -> float[m/s] {
    return m.sqrt(GM_soleil * (2.0 / r - 1.0 / a))
}

def main() -> int {
    let terre: Orbite = Orbite("Terre", UA)
    let mars: Orbite = Orbite("Mars", 1.523679 * UA)
    let a: float[m] = (terre.rayon + mars.rayon) / 2.0
    let dv1: float[m/s] = vitesse_ellipse(terre.rayon, a) - terre.vitesse()
    let dv2: float[m/s] = mars.vitesse() - vitesse_ellipse(mars.rayon, a)
    let duree: float[s] = m.pi * m.sqrt(pow(a, 3) / GM_soleil)
    print(dv1)
    print(dv2)
    print(duree / jour)

    // depuis une orbite basse de 400 km : les km se convertissent, ils ne se melangent pas
    let altitude: float[km] = 400.0
    let r0: float[m] = R_terre + altitude * 1000.0[m/km]
    let v0: float[m/s] = m.sqrt(GM_terre / r0)
    let v_lib: float[m/s] = m.sqrt(2.0) * v0
    print(v0)
    print(v_lib - v0)

    // force, energie, puissance : N, J et W sont des kg, m et s
    let masse: float[kg] = 420000.0
    let poids: float[N] = phys.G * 5.972e24[kg] * masse / pow(r0, 2)
    let energie: float[J] = 0.5 * masse * v0 * v0
    let puissance: float[W] = energie / (90.0 * 60.0[s])
    print(poids)
    print(energie)
    print(puissance)

    // les angles : rad est sans unite, deg se convertit avec radians()
    let angle: float[deg] = 30.0
    print(m.sin(m.radians(angle)))
    let distances: list[float[m]] = [1.0, 2.5, 4.0]
    let total: float[m] = 0.0
    for d in distances {
        total += d
    }
    distances.append(3.0[m])
    print(total)
    if total > 0.0 {
        print(len(distances))
    }
    return 0
}
