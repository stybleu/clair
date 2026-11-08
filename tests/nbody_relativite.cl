// V0.71.1 : la relativite generale (s.c, avec IAS15). References : la precession de Mercure (43" par
// siecle, la formule d'Einstein) et REBOUNDx 5.1.0 (effet "gr", avec REBOUND 5.2.2, memes departs).
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

def ecart(s: Simulation, i: int, x: float, y: float, z: float) -> float {
    return sqrt((s.x[i] - x) * (s.x[i] - x) + (s.y[i] - y) * (s.y[i] - y) + (s.z[i] - z) * (s.z[i] - z)) * 149597870.7
}

// la rotation de l'orbite de Mercure autour du Soleil seul, en secondes d'arc par siecle (pente sur cent ans)
def precession(relativite: bool) -> float {
    let s: Simulation = nbody.simulation_solar()
    s.integrator = "ias15"
    s.add("Soleil", 1.0, 0.0, 0.0, 0.0, 0.0)
    s.add_orbit("Mercure", 1.0 / 6023600.0, 0, 0.38709927, 0.20563593, 0.0, 0.0, 77.45779628, 0.0)
    if relativite {
        s.c = nbody.c_solar()
    }
    let w0: float = s.elements(1, 0)[4]
    let n: float = 0.0
    let st: float = 0.0
    let sw: float = 0.0
    let stt: float = 0.0
    let stw: float = 0.0
    for k in range(10001) {
        let w: float = (s.elements(1, 0)[4] - w0) * 3600.0
        n += 1.0
        st += s.t
        sw += w
        stt += s.t * s.t
        stw += s.t * w
        s.run(0.01)
    }
    return (n * stw - st * sw) / (n * stt - st * st) * 100.0
}

def main() -> int {
    // Mercure : la relativite fait tourner son orbite ; Newton seul, deux corps, ne la fait pas tourner
    let a: float = 0.38709927
    let e: float = 0.20563593
    let gm: float = nbody.G_solar() * (1.0 + 1.0 / 6023600.0)
    let c: float = nbody.c_solar()
    let theorie: float = 6.0 * 3.141592653589793 * gm / (c * c * a * (1.0 - e * e)) * 100.0 / (2.0 * 3.141592653589793 * sqrt(a * a * a / gm)) * 180.0 / 3.141592653589793 * 3600.0
    let p: float = precession(true)
    print("Mercure, avec la relativite : " + format(p, ".2f") + " secondes d'arc par siecle (Einstein : " + format(theorie, ".2f") + "), a 0.001 pres " + oui(abs(p - theorie) < 0.001))
    print("Mercure, Newton seul : " + oui(abs(precession(false)) < 1e-6))

    // le systeme solaire, cent ans, avec et sans la relativite
    let n: Simulation = nbody.solar_system()
    n.run(100.0)
    let r: Simulation = nbody.solar_system()
    r.c = nbody.c_solar()
    let p0: float = r.momentum(0)
    let q0: float = r.momentum(1)
    r.run(100.0)
    let dm: float = ecart(r, 1, n.x[1], n.y[1], n.z[1])
    let dt: float = ecart(r, 3, n.x[3], n.y[3], n.z[3])
    print("cent ans : la relativite deplace Mercure de 7 300 km " + oui(dm > 7200.0 and dm < 7450.0) + ", la Terre de 6 100 km " + oui(dt > 6000.0 and dt < 6200.0))
    print("comme REBOUNDx a 1 km (Mercure, Terre) : " + oui(ecart(r, 1, 0.253142739070579, -0.349041708504261, -0.0513496769335668) < 1.0 and ecart(r, 3, -0.151948105299691, 0.971714669307427, -0.000470339939378495) < 1.0))
    print("quantite de mouvement gardee : " + oui(abs(r.momentum(0) - p0) < 1e-14 and abs(r.momentum(1) - q0) < 1e-14))

    // sauver et reprendre : s.c est garde
    r.save("relativite_etat.txt")
    let l: Simulation = nbody.load("relativite_etat.txt")
    n.save("newton_etat.txt")
    print("sauve et relu : s.c garde " + oui(l.c == nbody.c_solar()) + ", Newton reste Newton " + oui(nbody.load("newton_etat.txt").c == 0.0))
    return 0
}
