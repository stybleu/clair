// V0.53 : des mesures avec leur incertitude (valeur ± incertitude), propagees par Clair
import phys
from astro import GM_terre, R_terre

Class Pendule {
    longueur: measure[m]
    periode: measure[s]

    def g() -> measure[m/s^2] {
        return 39.47841760435743 * self.longueur / (self.periode * self.periode)
    }
}

// la duree d'une chute de hauteur h
def chute(h: measure[m], g: measure[m/s^2]) -> measure[s] {
    return sqrt(2.0 * h / g)
}

def main() -> int {
    // une chute de 20 m : 2,019 s a 3 ms pres
    let g: measure[m/s^2] = 9.81 ± 0.02
    let h: measure[m] = 20.0[m] +/- 0.05
    let t: measure[s] = chute(h, g)
    print(t)
    print(value(t))
    print(uncertainty(t))
    print("t = " + str(t) + " s")
    print(format(t, ".5f"))

    // une meme mesure comptee une fois : x - x = 0, x / x = 1
    let x: measure = 3.0 ± 0.1
    print(x - x)
    print(x / x)
    print(x * x)
    print(x + x)
    let y: measure = 4.0 ± 0.2
    print(hypot(x, y))
    print(atan2(x, y))
    print(pow(x, 2))
    print(-x)
    print(abs(-x))
    print(sqrt(y) * sqrt(y) - y)

    // les constantes de phys gardent leur incertitude (CODATA 2022)
    print(measure(phys.G))
    print(measure(phys.m_e))
    print(measure(phys.alpha))
    print(measure(phys.c))
    let un: measure = phys.eps0 * phys.mu0 * phys.c * phys.c
    print(un)
    let a0: measure[m] = phys.hbar / (phys.m_e * phys.c * phys.alpha)
    print(a0)

    // la masse de la Terre : G n'est connu qu'a 2e-5 pres ; GM = masse * G redevient exact
    let masse: measure[kg] = GM_terre / phys.G
    print(masse)
    let gm: measure[m^3/s^2] = masse * phys.G
    print(gm)
    let v_lib: measure[m/s] = sqrt(2.0 * phys.G * masse / R_terre)
    print(v_lib)

    // un pendule : g a partir de la longueur et de la periode mesurees
    let p: Pendule = Pendule(0.9942 ± 0.0005, 2.0 ± 0.002)
    print(p.g())
    if p.g() > 9.0[m/s^2] {
        print("g plus grand que 9")
    }

    // 100 mesures independantes de 1.0 ± 0.1 : la somme est 100.0 ± 1.0
    let somme: measure = 0.0
    for i in range(100) {
        somme += 1.0 ± 0.1
    }
    print(somme)

    // un float reste un float, sans incertitude
    let gf: float = phys.G
    print(gf)
    print(measure(gf))
    return 0
}
