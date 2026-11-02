// V0.68 : nbody avec IAS15 (l'integrateur d'ordre 15 de REBOUND, pas de temps choisi seul), les elements
// orbitaux et le systeme solaire. Les valeurs de reference viennent de REBOUND 5.2.2 (IAS15, memes conditions
// de depart) et de la solution exacte de Kepler.
import nbody

def oui(b: bool) -> string {
    if b {
        return "oui"
    }
    return "NON"
}

def angle(d: float) -> float {
    if d > 180.0 {
        return d - 360.0
    }
    return d
}

def main() -> int {
    // Kepler, e = 0.9, cent tours : l'orbite ne bouge pas, la planete revient a l'heure
    let k: Simulation = nbody.simulation(1.0)
    k.integrator = "ias15"
    k.add("M", 1.0, 0.0, 0.0, 0.0, 0.0)
    k.add_orbit("p", 0.0, 0, 1.0, 0.9, 0.0, 0.0, 0.0, 0.0)
    k.dt = 0.001
    k.run(100.0 * 2.0 * 3.141592653589793)
    let el: list[float] = k.elements(1, 0)
    print("kepler, demi-grand axe garde a 1e-13 : " + oui(abs(el[0] - 1.0) < 1e-13))
    print("kepler, excentricite gardee a 1e-13 : " + oui(abs(el[1] - 0.9) < 1e-13))
    print("kepler, a l'heure a 1e-8 degre : " + oui(abs(angle(el[6])) < 1e-8))
    print("kepler, pas comme REBOUND (9717) : " + oui(k.steps_done(0) > 9700 and k.steps_done(0) < 9735))
    print("kepler, temps exact : " + oui(k.t == 100.0 * 2.0 * 3.141592653589793))

    // le probleme pythagoricien (Burrau) : rencontres tres proches, compare a REBOUND a t = 10
    let s: Simulation = nbody.simulation(1.0)
    s.integrator = "ias15"
    s.add("a", 3.0, 1.0, 3.0, 0.0, 0.0)
    s.add("b", 4.0, -2.0, -1.0, 0.0, 0.0)
    s.add("c", 5.0, 1.0, -1.0, 0.0, 0.0)
    s.dt = 0.001
    let e0: float = s.energy()
    let l0: float = s.angular_momentum()
    s.run(10.0)
    print("pythagore, energie gardee a 1e-12 : " + oui(abs((s.energy() - e0) / e0) < 1e-12))
    print("pythagore, moment cinetique garde : " + oui(abs(s.angular_momentum() - l0) < 1e-12))
    print("pythagore, comme REBOUND a 1e-8 : " + oui(abs(s.x[0] - 0.778480410137712) < 1e-8 and abs(s.y[0] - 0.141392300288796) < 1e-8 and abs(s.vx[2] + 0.814322252168940) < 1e-8))
    print("pythagore, pas comme REBOUND (615) : " + oui(s.steps_done(0) > 605 and s.steps_done(0) < 625))
    let t0: float = s.t
    s.step()
    print("un pas seul : " + oui(s.t > t0 and s.dt > 0.0))

    // les elements orbitaux, aller et retour : ellipse, hyperbole, Kepler
    let o: Simulation = nbody.simulation(2.5)
    o.add("centre", 1.0, 0.3, -0.2, 0.1, 0.05)
    o.add_orbit("ellipse", 0.001, 0, 2.0, 0.3, 20.0, 40.0, 60.0, 80.0)
    o.add_orbit("hyperbole", 0.0, 0, -1.0, 1.5, 120.0, 300.0, 10.0, 30.0)
    let a1: list[float] = o.elements(1, 0)
    print("ellipse : " + oui(abs(a1[0] - 2.0) < 1e-12 and abs(a1[1] - 0.3) < 1e-12 and abs(a1[2] - 20.0) < 1e-10 and abs(a1[3] - 40.0) < 1e-10 and abs(a1[4] - 60.0) < 1e-10 and abs(a1[5] - 80.0) < 1e-10))
    print("periode : " + format(a1[7], ".6f") + " (2 pi sqrt(8 / 2.5025) = " + format(2.0 * 3.141592653589793 * sqrt(8.0 / 2.5025), ".6f") + ")")
    let a2: list[float] = o.elements(2, 0)
    print("hyperbole : " + oui(abs(a2[0] + 1.0) < 1e-12 and abs(a2[1] - 1.5) < 1e-12 and abs(a2[2] - 120.0) < 1e-10 and abs(a2[3] - 300.0) < 1e-10 and abs(a2[4] - 10.0) < 1e-10 and abs(a2[5] - 30.0) < 1e-10))
    print("equation de Kepler : " + oui(abs(nbody.true_anomaly(60.0, 0.5) - 118.815000926997) < 1e-9 and abs(nbody.true_anomaly(100.0, 1.5) - 110.202798738228) < 1e-9 and nbody.true_anomaly(37.0, 0.0) == 37.0))
    print("autour du centre de masse (-1) : " + oui(abs(o.semi_major_axis(1, -1) - 2.0) < 1e-12))

    // le systeme solaire (JPL, J2000) : les orbites des planetes, cent ans
    let ss: Simulation = nbody.solar_system()
    print("corps : " + str(len(ss.m)) + ", G = " + format(ss.G, ".6f") + " UA^3 / (masse solaire an^2)")
    for i in [1, 3, 5, 8] {
        print(ss.name[i] + " : a = " + format(ss.semi_major_axis(i, 0), ".5f") + " UA, e = " + format(ss.eccentricity(i, 0), ".5f") + ", i = " + format(ss.inclination(i, 0), ".4f") + ", periode " + format(ss.period(i, 0), ".4f") + " an")
    }
    let es: float = ss.energy()
    ss.run(100.0)
    print("cent ans, energie gardee a 1e-14 : " + oui(abs((ss.energy() - es) / es) < 1e-14))
    print("cent ans, demi-grand axe de Jupiter : " + format(ss.semi_major_axis(5, 0), ".3f") + " UA")
    print("centre de masse immobile : " + oui(abs(ss.center_of_mass(0)) < 1e-12 and abs(ss.momentum(1)) < 1e-14))

    // une collision avec IAS15 : les deux corps n'en font qu'un, la quantite de mouvement reste
    let c: Simulation = nbody.simulation(1.0)
    c.integrator = "ias15"
    c.merge = true
    let u: int = c.add("u", 1.0, -1.0, 0.0, 0.5, 0.0)
    let v: int = c.add("v", 2.0, 1.0, 0.0, -0.1, 0.0)
    c.radius(u, 0.1)
    c.radius(v, 0.1)
    let p0: float = c.momentum(0)
    let chocs: int = c.run(5.0)
    print("collision : " + str(chocs) + ", corps restants " + str(c.count()) + ", quantite de mouvement gardee : " + oui(abs(c.momentum(0) - p0) < 1e-12))
    return 0
}
