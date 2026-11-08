// V0.69 : nbody avec WHFast (Wisdom-Holman, comme dans REBOUND), et s.save() / nbody.load().
// Les valeurs de reference : la solution exacte de Kepler, et la meme methode calculee avec des nombres
// de 80 bits au lieu de 64 (voir docs/README_V0.69.md).
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

def main() -> int {
    // deux corps : WHFast suit exactement l'orbite de Kepler, meme avec un grand pas (e = 0.9)
    let k: Simulation = nbody.simulation(1.0)
    k.integrator = "whfast"
    k.add("M", 1.0, 0.0, 0.0, 0.0, 0.0)
    k.add_orbit("p", 0.0, 0, 1.0, 0.9, 0.0, 0.0, 0.0, 0.0)
    k.dt = 0.3
    k.run(300.0)
    let el: list[float] = k.elements(1, 0)
    print("kepler, grand pas, orbite gardee : " + oui(abs(el[0] - 1.0) < 1e-11 and abs(el[1] - 0.9) < 1e-12))
    print("kepler, a l'heure a 1e-7 degre : " + oui(abs(el[6] - 268.7338539246969) < 1e-7))
    let h: Simulation = nbody.simulation(1.0)
    h.integrator = "whfast"
    h.add("M", 1.0, 0.0, 0.0, 0.0, 0.0)
    h.add_orbit("passage", 0.0, 0, -2.0, 1.5, 30.0, 0.0, 0.0, -60.0)
    h.dt = 0.7
    h.run(70.0)
    let eh: list[float] = h.elements(1, 0)
    print("hyperbole : " + oui(abs(eh[0] + 2.0) < 1e-11 and abs(eh[1] - 1.5) < 1e-11 and abs(eh[2] - 30.0) < 1e-9))

    // le systeme solaire, 1 000 ans, pas de 0.005 an
    let s: Simulation = nbody.solar_system()
    s.integrator = "whfast"
    s.dt = 0.005
    let e0: float = s.energy()
    s.run(1000.0)
    let de: float = abs((s.energy() - e0) / e0)
    print("systeme solaire, energie : ecart " + format(de, ".1e") + " (l'erreur de la methode, bornee)")
    print("comme le calcul en 80 bits a 1e-8 UA : " + oui(abs(s.x[1] + 0.1180892654175700) < 1e-8 and abs(s.y[1] + 0.4519282250957961) < 1e-8 and abs(s.x[8] - 26.35240466649480) < 1e-8 and abs(s.y[8] + 14.39108776597206) < 1e-8))
    print("centre de masse immobile : " + oui(abs(s.center_of_mass(0)) < 1e-12 and abs(s.momentum(1)) < 1e-14))
    print("demi-grand axe de Jupiter : " + format(s.semi_major_axis(5, -1), ".4f") + " UA")

    // le pas choisi seul : un quarantieme de l'orbite la plus courte
    let d: Simulation = nbody.solar_system()
    d.integrator = "whfast"
    print("pas par defaut : " + format(d.time_step() * 365.25, ".2f") + " jours")

    // sauver, relire, continuer : les memes nombres jusqu'au dernier chiffre
    let a: Simulation = nbody.solar_system()
    a.integrator = "whfast"
    a.dt = 0.01
    a.run(100.0)
    a.save("build/nbody_sauve.txt")
    let b: Simulation = nbody.load("build/nbody_sauve.txt")
    let pareil: bool = b.t == a.t and b.dt == a.dt and b.G == a.G and b.integrator == "whfast" and b.name[2] == "Vénus" and b.time_unit == "an" and b.colors[5] == a.colors[5]
    for i in range(len(a.m)) {
        if b.x[i] != a.x[i] or b.vz[i] != a.vz[i] or b.m[i] != a.m[i] or b.r[i] != a.r[i] {
            pareil = false
        }
    }
    print("relu au dernier chiffre : " + oui(pareil))
    a.run(100.0)
    b.run(100.0)
    let meme: bool = true
    for i in range(len(a.m)) {
        if a.x[i] != b.x[i] or a.vy[i] != b.vy[i] {
            meme = false
        }
    }
    print("la suite est la meme : " + oui(meme))
    let c: Simulation = nbody.solar_system()
    c.run(10.0)
    c.save("build/nbody_sauve.txt")
    let c2: Simulation = nbody.load("build/nbody_sauve.txt")
    c.run(10.0)
    c2.run(10.0)
    print("IAS15 repris : " + oui(abs(c.x[3] - c2.x[3]) < 1e-12 and c2.integrator == "ias15"))
    return 0
}
